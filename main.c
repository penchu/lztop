#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <time.h>
#include <stdbool.h>

#define token_number 11
#define token_size 10
#define MAX_PROCS 512
#define NUM_DISKS 2
#define NUM_SNPS 2

typedef struct {
    float conv_val;
    char *unit_conv;
} ValConv;

typedef struct {
    unsigned long RXb;
    unsigned long RXp;
    unsigned long TXb;
    unsigned long TXp;
} Speed;

typedef struct {
   pid_t pid;
   char name[17];
   char state;
   int rss;
//    int vsize;
   unsigned long vsize;
   int proc_cpu_usage;
   int utime;
   int stime;
   int n;
} Process;

typedef struct {
    float cpu_usage;
    unsigned long MemAvailable;
    unsigned long disc_free;
    unsigned long disc_total;
} Big_data;

typedef struct {
    char path[64];
    unsigned long disc_free;
    unsigned long disc_total;
} DiskStats;

void cpu_usage_calc(Big_data *bd);
unsigned long *read_cpu_snapshot(void);
void read_meminfo(Big_data *bd, unsigned long *MemTotal);
ValConv readable_values(unsigned long value);
int disk_usage_calc(char *path, Big_data *bd, DiskStats disks[]);
Speed network_stats(void);
void ntwrk_spd_calc(Speed snapshots[]);
int processes_list(unsigned long MemTotal, Process proc_snps[][MAX_PROCS]);
int take_proc_snapshot(Process proc_snps[]);
int sort_procs(Process *snap, int n, int (*cmp)(const void *, const void *));
int compare_sort(const void *a, const void *b);
int compare_sort_cpu(const void *a, const void *b);
int compare_search(const void* a, const void* b);
void print_proc_stats(Process *snap, unsigned long MemTotal);
void collecting_data(Big_data *bd, unsigned long *MemTotal, DiskStats disks[], Speed snapshots[], Process proc_snps[][MAX_PROCS]);
void printing_data(Big_data *bd, unsigned long MemTotal, DiskStats disks[], Speed snapshots[], Process proc_snps[][MAX_PROCS]);

Process *process_list = NULL;

int main(void) {
    unsigned long MemTotal;
    Big_data bd = {0};
    DiskStats disks[NUM_DISKS];
    Speed snapshots[NUM_SNPS];
    Process proc_snps[NUM_SNPS][MAX_PROCS];

    printf("\x1b[H");
    printf("\x1b[?1049h");

    while(true) {

        printf("\x1b[2J");
        printf("\x1b[H");

        collecting_data(&bd, &MemTotal, disks, snapshots, proc_snps);
        printf("---------------------------------------\n");
        printing_data(&bd, MemTotal, disks, snapshots, proc_snps);
        // print_proc_stats(proc_snps[1], MemTotal);
        printf("---------------------------------------\n");

        fflush(stdout);
        sleep(1);
    }

    return 0;
}

void cpu_usage_calc(Big_data *bd) {
    unsigned long *cpu_snapshot = read_cpu_snapshot();
    
    unsigned long stored_cpu_snapshot[100] = {0};

    for (int i = 0; i <= 9; i++) {
        stored_cpu_snapshot[i] = cpu_snapshot[i];        
    }
    sleep(1);
    cpu_snapshot = read_cpu_snapshot();

    unsigned long total_active = 0;
    unsigned long total_idle = 0;
    for (int i = 0; i <= 7; i++) {
        total_active += (cpu_snapshot[i] - stored_cpu_snapshot[i]);
    }
    total_idle = (cpu_snapshot[3] - stored_cpu_snapshot[3]) + (cpu_snapshot[4] - stored_cpu_snapshot[4]);

    bd->cpu_usage = (((double)total_active - (double)total_idle)/(double)total_active)*100;

    stored_cpu_snapshot[0] = '\0';
    cpu_snapshot[0] = '\0';
}

unsigned long *read_cpu_snapshot(void) {
    FILE *fptr;
    fptr = fopen("/proc/stat", "r");
    size_t buff_size = 128;    
    char buff[buff_size];
    fgets(buff, buff_size, fptr);
    buff[strcspn(buff, "\n")] = '\0';
    fclose(fptr);

    int n = 0;
    static unsigned long token[token_number];
    char *p = buff;
    int i = 0;

    while (!isdigit(buff[i])) {
        i++;
    };
    p += i;
    
    while (*p != '\0') {        
        token[n++] = strtoul(p, &p, 10);
    }

    return token;
}

void read_meminfo(Big_data *bd, unsigned long *MemTotal) {
    FILE *fptr;
    fptr = fopen("/proc/meminfo", "r");
    size_t buff_size = 128;    
    char buff[buff_size];
    char token[token_number][token_size*3] = {0};
    char token_new[token_number];
    int n = 0;
    int m = 0;
    char *p = NULL;

    while (fgets(buff, buff_size, fptr) != NULL) {
        // buff[strcspn(buff, "\n")] = '\0';
        if (strstr(buff, "MemTotal")) {
            p = buff;
            for (int i = 0; buff[i] != '\0'; i++) {
                if (isdigit(buff[i])) {                
                    *MemTotal = strtol(p + i, NULL, 10);
                    break;
                }
            }
            n++;                       
        }
        if (strstr(buff, "MemAvailable")) {
            p = buff;
            for (int i = 0; buff[i] != '\0'; i++) {
                if (isdigit(buff[i])) {  
                    bd->MemAvailable = strtol(p + i, NULL, 10);
                    break;
                }
            }
            n++;                       
        }
        if (n == 2) break;
    }
    fclose(fptr);
}

ValConv readable_values(unsigned long value) {
    // value *= 1024; 
    ValConv s1;
    int count = 0;
    s1.conv_val = value;
       
    static char units[4][3] = {"B", "KB", "MB", "GB"};

    while (s1.conv_val >= 1024) { 
        s1.conv_val = s1.conv_val/1024.0;
        units[count++];
    }   
    s1.unit_conv = units[count];

    return s1;
}

int disk_usage_calc(char *path, Big_data *bd, DiskStats disks[]) {
    struct statvfs stat;  

    if (statvfs(path, &stat) != 0) {
        return -1;
    } 

    if (strcmp(path, "/") == 0) {
        strcpy(disks[0].path, "/");
        disks[0].disc_free = stat.f_bfree*stat.f_frsize;
        disks[0].disc_total = stat.f_blocks*stat.f_frsize;
    }
    else {
        strcpy(disks[1].path, "/mnt/sdb1");
        disks[1].disc_free = stat.f_bavail*stat.f_frsize;
        disks[1].disc_total = stat.f_blocks*stat.f_frsize;
    }

    return 0;
}

Speed network_stats(void) {
    char p[250];
    int n = 0;  

    FILE *fptr;
    fptr = fopen("/proc/net/dev", "r");
    size_t buff_size = 128;    
    char buff[buff_size];

    char *endptr;
    unsigned long token[20];
    int i = 0;
    int pos = 6;
    char *ptr;

    while (fgets(buff, buff_size, fptr) != NULL) {
        if (strncmp(buff, "enp5s0:", 7) == 0) {
            buff[strcspn(buff, "\n")] = '\0'; // not sure if needed
            
            while (!isdigit(buff[pos])) pos++;
            ptr = buff + pos;

            while (*ptr != '\0') {
                while (*ptr == ' ') ptr++;
                token[i++] = strtoul(ptr, &ptr, 10);     
            }            
        }
    }    
    fclose(fptr);

    Speed s1;
    s1.RXb = token[0];
    s1.RXp = token[1]; 
    s1.TXb = token[8];
    s1.TXp = token[9];
    return s1;
}

void ntwrk_spd_calc(Speed snapshots[]) {

    snapshots[0] = network_stats();
    sleep(1);
    snapshots[1] = network_stats(); 
}

int processes_list(unsigned long MemTotal, Process proc_snps[][MAX_PROCS]) {
    
    // Process snap1[MAX_PROCS];
    // int n = take_proc_snapshot(snap1, MAX_PROCS);   
    proc_snps[0]->n = take_proc_snapshot(proc_snps[0]);
    // for (int i = 0; i < 5; i++) {
    //     printf("name: %s\n", proc_snps[0]->name);
    // }

    sort_procs(proc_snps[0], proc_snps[0]->n, compare_sort);
    
    struct timespec interval;
    interval.tv_sec = 1;
    interval.tv_nsec = 0;
    nanosleep(&interval, NULL);   
    long ticks = sysconf(_SC_CLK_TCK);
    double elapsed = interval.tv_sec + interval.tv_nsec / 1e9;
    
    // Process snap2[MAX_PROCS];
    proc_snps[1]->n = take_proc_snapshot(proc_snps[1]);

    Process *item;
    
    for (int i = 0; i < proc_snps[1]->n; i++) {
        pid_t key = proc_snps[1][i].pid;
        item = bsearch(&key, proc_snps[0], proc_snps[0]->n, sizeof(proc_snps[0][0]), compare_search);
        if (item != NULL) {
            proc_snps[1][i].proc_cpu_usage = ((proc_snps[1][i].utime + proc_snps[1][i].stime) - (item->utime + item->stime))/(elapsed*ticks)*100;
        }        
    }

    sort_procs(proc_snps[1], proc_snps[1]->n, compare_sort_cpu);

    // print_proc_stats(proc_snps[1], proc_snps[1]->n, MemTotal);
    memset(&proc_snps[0], 0, sizeof(proc_snps[0]));
    // memset(&proc_snps[1], 0, sizeof(proc_snps[1]));

    return 0;
}

int take_proc_snapshot(Process proc_snps[]) {
    struct dirent *pDirent;
    DIR *pDir;
    pDir = opendir("/proc");
    if (pDir == NULL) {
        printf("Cannot open directory /proc");
        return 1;
    }

    int n = 0;    

    FILE *fptr;
    char path_pid[300];
    char comm_name[50];
    char buff[128] = {0};

    while ((pDirent = readdir(pDir)) != NULL) {
        if (isdigit(pDirent->d_name[0])) {            
            proc_snps[n].pid = atoi(pDirent->d_name);
            snprintf(path_pid, 300, "/proc/%s/status", pDirent->d_name);            
            fptr = fopen(path_pid, "r");
            char *p = NULL;
            while (fgets(buff, 128, fptr)) {
                if (strncmp("Name", buff, 4) == 0) {
                    p = buff + 6;
                    strcpy(proc_snps[n].name, p);                    
                    proc_snps[n].name[strcspn(proc_snps[n].name, "\n")] = '\0';
                }
                if (strncmp("State", buff, 5) == 0) {
                    proc_snps[n].state = buff[7];
                }
                if (strncmp("VmSize", buff, 6) == 0) {
                    sscanf(buff, "VmSize: %ld", &proc_snps[n].vsize);
                }
                if (strncmp("VmRSS", buff, 5) == 0) {
                    sscanf(buff, "VmRSS: %d", &proc_snps[n].rss);
                    break;                    
                }                
            }
            fclose(fptr);            

            snprintf(path_pid, 300, "/proc/%s/stat", pDirent->d_name);            
            fptr = fopen(path_pid, "r");
            fgets(buff, 128, fptr);
            buff[strcspn(buff, "\n")] = '\0';
            
            char *p2 = NULL;
            for (int i = 0; buff[i] != '\0'; i++) {
                if (buff[i] == ')') {
                    p2 = buff + i + 2;
                    break;
                }
            }

            int n1 = 1;
            char *p3 = strtok(p2, " ");
            while (p3 != NULL) {
                p3 = strtok(NULL, " ");
                n1++;
                if (n1 == 12) proc_snps[n].utime = atoi(p3);
                if (n1 == 13) {
                    proc_snps[n].stime = atoi(p3);
                    break;
                }
            }    
            fclose(fptr);    
            n++;
        }
    }
    return n;
}

int insertion_sort(Process *snap, int n) {
    int temp;
    int position;
    for (int i = 0; i < n; i++) {
        if (snap[i].pid < snap[i+1].pid) continue;
        else {
            temp = snap[i].pid;
            position = i - 1;
            while (snap[i].pid < snap[position].pid) {
                snap[position+1].pid = snap[position].pid;
                position--;
            }
            snap[position].pid = snap[i].pid;
        }
    }

    return 0;
}

int sort_procs(Process *snap, int n, int (*cmp)(const void *, const void *)) {
    qsort(snap, n, sizeof(snap[0]), cmp);
    return 0;
}

int compare_sort(const void *a, const void *b) {
    const Process *pA = a;
    const Process *pB = b;
    return pA->pid - pB->pid;
}

int compare_sort_cpu(const void *a, const void *b) {
    const Process *pA = a;
    const Process *pB = b;
    return pA->proc_cpu_usage - pB->proc_cpu_usage;
}

int compare_search(const void* a, const void* b) {
    // return (*(int*)a - *(int*)b);
    const pid_t *key = (const pid_t *)a;
    const Process *elem = (const Process *)b;
    if (*key > elem->pid) return 1;
    if (*key < elem->pid) return -1;
    return 0;
}

void print_proc_stats(Process *snap, unsigned long MemTotal) {  
    for (int i = (snap->n)-1; i >= ((snap->n)-5); i--) {
        // if (strcmp(snap[i].name, "597515") > 0) {
        //     printf("vsize: %d\n", snap[i].vsize);
        // }
        if (snap[i].vsize > 0) {
            ValConv struct_vsize = readable_values(snap[i].vsize*1024);
            ValConv struct_rss = readable_values(snap[i].rss*1024);
            double mem_usage = ((double)snap[i].rss/MemTotal)*100;
            printf("%d:%s %c %.2f%s/%.2f%s cpu:%d%% mem:%.2f%%\n", 
                    snap[i].pid, 
                    snap[i].name, 
                    snap[i].state,
                    struct_rss.conv_val, 
                    struct_rss.unit_conv, 
                    struct_vsize.conv_val, 
                    struct_vsize.unit_conv, 
                    snap[i].proc_cpu_usage,
                    mem_usage);
        }
        else (printf("%d:%s %c %d\n", 
                    snap[i].pid, 
                    snap[i].name, 
                    snap[i].state, 
                    snap[i].proc_cpu_usage));
    }
    // memset(&snap[0], 0, sizeof(snap[0]));
    // memset(&snap[1], 0, sizeof(snap[1]));
}

void collecting_data(Big_data *bd, unsigned long *MemTotal, DiskStats disks[], Speed snapshots[], Process proc_snps[][MAX_PROCS]) {
    cpu_usage_calc(bd);
    read_meminfo(bd, MemTotal);
    disk_usage_calc("/", bd, disks);
    disk_usage_calc("/mnt/sdb1", bd, disks);
    ntwrk_spd_calc(snapshots);    
    processes_list(*MemTotal, proc_snps);
}

void printing_data(Big_data *bd, unsigned long MemTotal, DiskStats disks[], Speed snapshots[], Process proc_snps[][MAX_PROCS]) {
    printf("CPU usage is: %.2f%%\n", bd->cpu_usage);
    
    unsigned long MemUsed = 0;
    float MemUsage = 0;
    MemUsage = (((double)(MemTotal) - (double)(bd->MemAvailable))/(double)(MemTotal))*100;
    MemUsed = MemTotal - bd->MemAvailable;
    ValConv struct_used_mem = readable_values(MemUsed*1024);
    ValConv struct_total_mem = readable_values(MemTotal*1024);
    printf("Memory usage is: %.2f%s/%.2f%s %.2f%%\n", 
            struct_used_mem.conv_val, 
            struct_used_mem.unit_conv, 
            struct_total_mem.conv_val, 
            struct_total_mem.unit_conv, 
            MemUsage); 

    double usage;
    unsigned long used;
    for (int i = 0; i < NUM_DISKS; i++) {
        used = disks[i].disc_total - disks[i].disc_free;
        usage = ((double)used/(double)disks[i].disc_total)*100;
        ValConv struct_used_disck = readable_values(used);
        ValConv struct_total_disk = readable_values(disks[i].disc_total);    
        printf("%s %.2f%s/%.2f%s %.2f%%\n", 
                disks[i].path, 
                struct_used_disck.conv_val, 
                struct_used_disck.unit_conv, 
                struct_total_disk.conv_val, 
                struct_total_disk.unit_conv, 
                usage);
    }

    ValConv rx_bytes_conv = readable_values(snapshots[0].RXb);
    ValConv tx_bytes_conv = readable_values(snapshots[0].TXb);

    printf("Network (enp5s0):\nRX: %.2f%s, %ld packets, \nTX: %.2f%s, %ld packets\n",
            rx_bytes_conv.conv_val, 
            rx_bytes_conv.unit_conv, 
            snapshots[0].RXp, 
            tx_bytes_conv.conv_val, 
            tx_bytes_conv.unit_conv, 
            snapshots[0].TXp);

    ValConv rx_r_conv = readable_values(snapshots[1].RXb - snapshots[0].RXb);
    ValConv tx_r_conv = readable_values(snapshots[1].TXb - snapshots[0].TXb);    

    printf("Download: %.2f%s/s, Upload: %.2f%s/s\n", 
            rx_r_conv.conv_val, 
            rx_r_conv.unit_conv, 
            tx_r_conv.conv_val, 
            tx_r_conv.unit_conv); 
    printf("\n");
    print_proc_stats(proc_snps[1], MemTotal);
    // memset(&proc_snps[0], 0, sizeof(proc_snps[0]));
    memset(&proc_snps[1], 0, sizeof(proc_snps[1]));

}
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <time.h>

#define token_number 11
#define token_size 10
#define MAX_PROCS 512

typedef struct {
    float conv_val;
    char *unit_conv;
} ValConv;

typedef struct {
    unsigned long RXb;
    unsigned long RXe;
    unsigned long RXp;
    unsigned long TXb;
    unsigned long TXe;
    unsigned long TXp;
} Speed;

typedef struct {
   pid_t pid;
   char name[17];
   char state;
   int rss;
   int vsize;
   int proc_cpu_usage;
   int utime;
   int stime;
} Process;

void cpu_usage_calc(void);
unsigned long* read_cpu_snapshot(void);
void read_meminfo(unsigned long *MemTotal);
ValConv readable_values(unsigned long value);
void disk_usage(void);
int disk_usage_calc(char *path);
Speed network_stats(void);
void ntwrk_spd_calc(void);
int processes_list(unsigned long MemTotal);
int take_proc_snapshot(Process *snap, int max_procs);
int sort_procs(Process *snap, int n, int (*cmp)(const void *, const void *));
int compare_sort(const void *a, const void *b);
int compare_sort_cpu(const void *a, const void *b);
int compare_search(const void* a, const void* b);

Process *process_list = NULL;

int main(void) {
    unsigned long MemTotal;

    cpu_usage_calc();
    read_meminfo(&MemTotal);
    disk_usage();
    network_stats();
    ntwrk_spd_calc();

    processes_list(MemTotal);

    return 0;
}

void cpu_usage_calc(void) {
    unsigned long* cpu_snapshot = read_cpu_snapshot();
    
    unsigned long stored_cpu_snapshot[100] = {0};

    for (int i = 0; cpu_snapshot[i] != '\0'; i++) {
        stored_cpu_snapshot[i] = cpu_snapshot[i];        
    }
    sleep(1);
    cpu_snapshot = read_cpu_snapshot();

    unsigned long total_active = 0;
    unsigned long total_idle = 0;
    for (int i = 0; i <= 3; i++) {
        total_active += (cpu_snapshot[i] - stored_cpu_snapshot[i]);
    }
    total_idle = (cpu_snapshot[3] - stored_cpu_snapshot[3]);

    float cpu_usage = (((double)total_active - (double)total_idle)/(double)total_active)*100;

    printf("CPU usage is: %.2f%%\n", cpu_usage);
}

unsigned long* read_cpu_snapshot(void) {
    FILE *fptr;
    fptr = fopen("/proc/stat", "r");
    size_t buff_size = 128;    
    char buff[buff_size];
    fgets(buff, buff_size, fptr);
    buff[strcspn(buff, "\n")] = '\0';
    fclose(fptr);

    char token[token_number][token_size];
    int n = 0;
    int m = 0;
    for (int i = 5; buff[i] != '\0'; i++) {
        if (buff[i] == ' ') {
            i++;
            n++;
            m = 0;
        }
        token[n][m++] = buff[i];
    }

    static unsigned long token_int[token_number];

    for (int i = 0; i <= n; i++) {
        token_int[i] = strtoul(token[i], NULL, 10);
    }

    return token_int;
}

void read_meminfo(unsigned long *MemTotal) {
    FILE *fptr;
    fptr = fopen("/proc/meminfo", "r");
    size_t buff_size = 128;    
    char buff[buff_size];
    char token[token_number][token_size*3] = {0};
    char token_new[token_number];
    int n = 0;
    int m = 0;
    // unsigned long MemTotal = 0;
    unsigned long MemAvailable = 0;
    unsigned long MemUsed = 0;
    float MemUsage = 0; 
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
                    MemAvailable = strtol(p + i, NULL, 10);
                    break;
                }
            }
            n++;                       
        }
        if (n == 2) break;
    }
    fclose(fptr);

    MemUsage = (((double)(*MemTotal) - (double)MemAvailable)/(double)(*MemTotal))*100;
    MemUsed = *MemTotal - MemAvailable;

    ValConv struct_used = readable_values(MemUsed*1024);
    ValConv struct_total = readable_values(*MemTotal*1024);

    printf("Memory usage is: %.2f%s/%.2f%s %.2f%%\n", struct_used.conv_val, struct_used.unit_conv, struct_total.conv_val, struct_total.unit_conv, MemUsage); 
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

void disk_usage(void) {
    disk_usage_calc("/");
    disk_usage_calc("/mnt/sdb1");
}

int disk_usage_calc(char *path) {
    struct statvfs stat;  

    if (statvfs(path, &stat) !=0) {
        return -1;
    } 

    double usage;
    unsigned long free;
    unsigned long total;
    unsigned long used;

    if (strcmp(path, "/") == 0) {
        free = stat.f_bfree*stat.f_frsize;
    }
    else {
        free = stat.f_bavail*stat.f_frsize;
    }
    total = stat.f_blocks*stat.f_frsize;
    used = total - free;
    usage = ((double)used/(double)total)*100;


    ValConv struct_used = readable_values(used/1024);
    ValConv struct_total = readable_values(total/1024);
    
    printf("%s  %.2f%s/%.2f%s %.2f%%\n", path, struct_used.conv_val, struct_used.unit_conv, struct_total.conv_val, struct_total.unit_conv, usage);

    return 0;
}

Speed network_stats(void) {
    char p[250];
    int n = 0;
    unsigned long rx_bytes;
    unsigned long rx_packets;
    unsigned long rx_errs;
    unsigned long tx_bytes;
    unsigned long tx_packets;
    unsigned long tx_errs;    

    FILE *fptr;
    fptr = fopen("/proc/net/dev", "r");
    size_t buff_size = 128;    
    char buff[buff_size];

    while (fgets(buff, buff_size, fptr) != NULL) {
        if (strstr(buff, "enp5s0")) {
            for (int i = 8; buff[i] != '\0'; i++) {
                p[n++] = buff[i];
            }             
        }
    }
    
    fclose(fptr);
    
    int i = 0;
    long token[20];
    char *myPtr = strtok(p, " ");
    while (myPtr != NULL) {
        if (myPtr != 0) {
            token[i++] = strtol(myPtr, NULL, 10);
        }
        myPtr = strtok(NULL, " ");
    } 
    rx_bytes = token[0];
    rx_packets = token[1];
    rx_errs = token[2];
    tx_bytes = token[8];
    tx_packets = token[9];
    tx_errs = token[10];

    // ValConv rx_bytes_conv = readable_values(rx_bytes);
    // ValConv tx_bytes_conv = readable_values(tx_bytes);
    // printf("Network (enp5s0):\nRX: %.2f%s, %ld packets, %ld errors\nTX: %.2f%s, %ld packets, %ld errors\n",
    //         rx_bytes_conv.conv_val, rx_bytes_conv.unit_conv, rx_packets, rx_errs, tx_bytes_conv.conv_val, 
    //         tx_bytes_conv.unit_conv, tx_packets, tx_errs);

    Speed s1;
    s1.RXb = rx_bytes;
    s1.RXe = rx_errs;
    s1.RXp = rx_packets;
    s1.TXb = tx_bytes;
    s1.TXe = tx_errs;
    s1.TXp = tx_packets;
    return s1;
}

void ntwrk_spd_calc(void) {

    Speed s1 = network_stats();
    unsigned long rx1 = s1.RXb;
    unsigned long tx1 = s1.TXb;

    ValConv rx_bytes_conv = readable_values(rx1);
    ValConv tx_bytes_conv = readable_values(tx1);

    printf("Network (enp5s0):\nRX: %.2f%s, %ld packets, %ld errors\nTX: %.2f%s, %ld packets, %ld errors\n",
            rx_bytes_conv.conv_val, rx_bytes_conv.unit_conv, s1.RXp, s1.RXe, tx_bytes_conv.conv_val, 
            tx_bytes_conv.unit_conv, s1.TXp, s1.TXe);

    sleep(1);
    Speed s2 = network_stats();
    unsigned long rx2 = s2.RXb;
    unsigned long tx2 = s2.TXb;

    unsigned long rx_rate = s2.RXb - s1.RXb;
    unsigned long tx_rate = s2.TXb - s1.TXb;

    ValConv rxr_conv = readable_values(rx_rate);
    ValConv txr_conv = readable_values(tx_rate);

    printf("Download: %.2f%s/s, Upload: %.2f%s/s\n", rxr_conv.conv_val, rxr_conv.unit_conv, txr_conv.conv_val, txr_conv.unit_conv); 
        
}

int processes_list(unsigned long MemTotal) {
    
    Process snap1[MAX_PROCS];
    int n = take_proc_snapshot(snap1, MAX_PROCS);

    Process *item;

    sort_procs(snap1, n, compare_sort);
    // qsort(snap1, n, sizeof(snap1[0]), compare_sort);
    
    struct timespec interval;
    interval.tv_sec = 1;
    interval.tv_nsec = 0;
    nanosleep(&interval, NULL);   
    long ticks = sysconf(_SC_CLK_TCK);
    double elapsed = interval.tv_sec + interval.tv_nsec / 1e9;
    
    Process snap2[MAX_PROCS];
    int n2 = take_proc_snapshot(snap2, MAX_PROCS);
    
    for (int i = 0; i < n2; i++) {
        pid_t key = snap2[i].pid;
        item = bsearch(&key, snap1, n, sizeof(snap1[0]), compare_search);
        snap2[i].proc_cpu_usage = ((snap2[i].utime + snap2[i].stime) - (item->utime + item->stime))/(elapsed*ticks)*100;
    }

    sort_procs(snap2, n, compare_sort_cpu);
    // qsort(snap2, n2, sizeof(snap2[0]), compare_sort_cpu); 

    for (int i = n; i >= (n2-10); i--) {
        if (snap2[i].vsize > 0) {
            ValConv struct_vsize = readable_values(snap2[i].vsize*1024);
            ValConv struct_rss = readable_values(snap2[i].rss*1024);
            // printf("%ld\n", MemTotal);
            double mem_usage = ((double)snap2[i].rss/MemTotal)*100;
            printf("%d:%s %c %.2f%s/%.2f%s cpu:%d%% mem:%.2f%%\n", 
                    snap2[i].pid, 
                    snap2[i].name, 
                    snap2[i].state,
                    struct_vsize.conv_val, 
                    struct_vsize.unit_conv, 
                    struct_rss.conv_val, 
                    struct_rss.unit_conv, 
                    snap2[i].proc_cpu_usage,
                    mem_usage);
        }
        else (printf("%d:%s %c %d\n", 
                    snap2[i].pid, 
                    snap2[i].name, 
                    snap2[i].state, 
                    snap2[i].proc_cpu_usage));
    }
    return 0;
}

int take_proc_snapshot(Process *snap, int max_procs) {
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
            snap[n].pid = atoi(pDirent->d_name);

            snprintf(path_pid, 300, "/proc/%s/status", pDirent->d_name);            
            fptr = fopen(path_pid, "r");
            char *p = NULL;
            while (fgets(buff, 128, fptr)) {
                if (strncmp("Name", buff, 4) == 0) {
                    p = buff + 6;
                    strcpy(snap[n].name, p);                    
                    snap[n].name[strcspn(snap[n].name, "\n")] = '\0';
                }
                if (strncmp("State", buff, 5) == 0) {
                    snap[n].state = buff[7];
                }
                if (strncmp("VmSize", buff, 6) == 0) {
                    sscanf(buff, "VmSize: %d", &snap[n].vsize);
                }
                if (strncmp("VmRSS", buff, 5) == 0) {
                    sscanf(buff, "VmRSS: %d", &snap[n].rss);
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
                if (n1 == 12) snap[n].utime = atoi(p3);
                if (n1 == 13) {
                    snap[n].stime = atoi(p3);
                    break;
                }
            }    
            fclose(fptr);    
            n++;
        }
    }
    return n;
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


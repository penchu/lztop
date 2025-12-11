#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/statvfs.h>

#define token_number 11
#define token_size 10

typedef struct {
    float conv_val;
    char *unit_conv;
} ValConv;

// typedef struct {
//     unsigned long total;
//     unsigned long free;
//     double usage;
// } DiskUsage;

float cpu_usage_calc(void);
unsigned long* read_cpu_snapshot(void);
void read_meminfo(void);
ValConv readable_values(unsigned long value);
void disk_usage(void);
int disk_usage_calc(char *path);

int main(void) {

    float cpu_usage = cpu_usage_calc();
    printf("CPU usage is: %.2f%%\n", cpu_usage);

    read_meminfo();

    disk_usage();

    return 0;
}

float cpu_usage_calc(void) {
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

    return cpu_usage;
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

void read_meminfo(void) {
    FILE *fptr;
    fptr = fopen("/proc/meminfo", "r");
    size_t buff_size = 128;    
    char buff[buff_size];
    char token[token_number][token_size*3] = {0};
    char token_new[token_number];
    int n = 0;
    int m = 0;
    unsigned long MemTotal = 0;
    unsigned long MemAvailable = 0;
    unsigned long MemUsed = 0;
    float MemUsage = 0; 
    char *p = NULL;

    while (fgets(buff, buff_size, fptr) != NULL) {
        buff[strcspn(buff, "\n")] = '\0';
        if (strstr(buff, "MemTotal")) {
            p = buff;
            for (int i = 0; buff[i] != '\0'; i++) {
                if (isdigit(buff[i])) {                
                    MemTotal = strtol(p + i, NULL, 10);
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

    MemUsage = (((double)MemTotal - (double)MemAvailable)/(double)MemTotal)*100;
    MemUsed = MemTotal - MemAvailable;

    ValConv struct_used = readable_values(MemUsed);
    ValConv struct_total = readable_values(MemTotal);

    printf("Memory usage is: %.2f%s/%.2f%s %.2f%%\n", struct_used.conv_val, struct_used.unit_conv, struct_total.conv_val, struct_total.unit_conv, MemUsage); 
}

ValConv readable_values(unsigned long value) {
    value *= 1024; 
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
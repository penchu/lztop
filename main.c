#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#define token_number 11
#define token_size 10

float cpu_usage_calc(void);
unsigned long* read_cpu_snapshot(void);
void read_meminfo(void);
char* readable_values(unsigned long long value);

int main(void) {

    float cpu_usage = cpu_usage_calc();
    printf("CPU usage is: %.2f%%\n", cpu_usage);

    read_meminfo();

    readable_values(6054420);

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

    char MemUsed_conv[50]; 
    char MemTotal_conv[50]; 
    strcpy(MemUsed_conv, readable_values(MemUsed));
    strcpy(MemTotal_conv, readable_values(MemTotal));

    printf("Memory usage is: %s/%s %.2f%%\n", MemUsed_conv, MemTotal_conv, MemUsage);
}

char* readable_values(unsigned long long value) {
    value *= 1024; 
    int count = 0;
    float unit_inc = value;
       
    char units[4][3] = {"B", "KB", "MB", "GB"};

    // printf("%.2f %s %lld\n", unit_inc, units[count], value);

    while (unit_inc >= 1024) { 
        unit_inc = unit_inc/1024.0;
        units[count++];
    }

    // printf("%.2f %s\n", unit_inc, units[count]);    
    static char convertion[50];
    memset(convertion, 0, sizeof(convertion));
    sprintf(convertion, "%.2f%s", unit_inc, units[count]);          

    return convertion;
}
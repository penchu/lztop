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

typedef struct {
    unsigned long RXb;
    unsigned long RXe;
    unsigned long RXp;
    unsigned long TXb;
    unsigned long TXe;
    unsigned long TXp;
} Speed;

void cpu_usage_calc(void);
unsigned long* read_cpu_snapshot(void);
void read_meminfo(void);
ValConv readable_values(unsigned long value);
void disk_usage(void);
int disk_usage_calc(char *path);
Speed network_stats(void);
void ntwrk_spd_calc(void);

int main(void) {

    cpu_usage_calc();
    read_meminfo();
    disk_usage();
    network_stats();
    ntwrk_spd_calc();

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
        // buff[strcspn(buff, "\n")] = '\0';
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

    ValConv struct_used = readable_values(MemUsed*1024);
    ValConv struct_total = readable_values(MemTotal*1024);

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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include<unistd.h>

#define token_number 11
#define token_size 10

float cpu_usage_calc(void);
unsigned long* read_cpu_snapshot(void);
// unsigned long* read_meminfo(void);
void read_meminfo(void);

int main(void) {

    float cpu_usage = cpu_usage_calc();
    printf("CPU usage is: %.2f%%\n", cpu_usage);

    read_meminfo();

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

// unsigned long* read_meminfo(void)
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

    while (fgets(buff, buff_size, fptr) != NULL) {
        buff[strcspn(buff, "\n")] = '\0';
        if (strstr(buff, "MemTotal")) {
            strcpy(token[n++], buff);
        }
        else if (strstr(buff, "MemAvailable")) {
            strcpy(token[n++], buff);
        }
        if (n == 2) break;
    }
    fclose(fptr);

    for (int i = 0; i < n; i++) {
        char *myPtr = strtok(token[i], " \t");        
        while(myPtr != NULL) {
            printf("%s\n", myPtr);
            myPtr = strtok(NULL, " \t");
        }
    }

    for (int i = 0; token[i][0] != '\0'; i++) {
        printf("token[%d] is %s\n", i, token[i]);
    }

    // for (int i = 0; i < n; i++) {
    //     for (int j = 0; token[i][j] != '\0'; j++) {
    //         char *myPtr = strtok(token[i], " ");
    //         while(myPtr != NULL) {
    //             myPtr = strtok(NULL, " ");
    //         }
    //         // printf("token[%d][%d] is %c\n", i, j, token[i][j]);
    //     }
    //     printf("token[%d] is %s\n", i, token[i]);
    // }

    // for (int i = 0; token[i] != NULL; i++) {
    //     printf("%s ", token[i+1]);
    //     if (strcmp(token[i], "MemTotal:") == 0) MemTotal = strtoul(token[i+1], NULL, 10);
    //     else if (strcmp(token[i], "MemAvailable:") == 0) {
    //         MemAvailable = strtoul(token[i+1], NULL, 10);
    //         break;
    //     }
    // }

    // for (int i = 0; token[i] != NULL; i++) {
    //     if (strstr(token[i], "MemTotal")) MemTotal = strtoul(token[i+1], NULL, 10);
    //     else if (strstr(token[i], "MemAvailable")) MemAvailable = strtoul(token[i+1], NULL, 10);
    // } 

    // printf("%ld, %ld\n", MemAvailable, MemTotal);


    // for (int i = 0; i < n; i++) {
    //     for (int j = 0; token[i][j] != '\0'; j++) {
    //         printf("%c", token[i][j]);
    //         while (token[i][j] == ' ') {
    //             j++;
    //         }
    //         token_new[m] == token[i][j];
    //     }
    //     printf("\n");
    // }
    // printf("\n");


}
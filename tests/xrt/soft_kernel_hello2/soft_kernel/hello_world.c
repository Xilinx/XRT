#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <syslog.h>
#include <dirent.h>
#include <stdbool.h>

#include "sk_types.h"

//Use packed struct in both host application and soft kernel
typedef struct __attribute__ ((packed)) regfile {
    unsigned long long out_hello;
    unsigned long long out_log;
    int size_hello;
    unsigned int size_log;
} regfile_t;

size_t write_log(FILE* f1, size_t size_log, char* bo_log, size_t curr_log_size)
{
    fseek(f1, 0, SEEK_END);
    size_t s1 = ftell(f1);
    rewind(f1);
    size_t read_size = 0;
    if (s1 > 0 && (curr_log_size + s1) < size_log) {
        read_size = fread((void*)(bo_log + curr_log_size), sizeof(char), s1, f1);
        if (read_size > 0) {
            s1 = read_size;
        } else {
            s1 = 0;
        }
        return s1;
    }
    return -1;
}

size_t write_syslog(FILE* f3, size_t size_log, char* bo_log, size_t curr_log_size)
{
    fseek(f3, 0, SEEK_END);
    size_t s3 = ftell(f3);
    rewind(f3);
    size_t read_size = 0;
    if (s3 > 0 && (curr_log_size + s3) > size_log) {
        int ret = fseek(f3, ((long) s3) - size_log + curr_log_size + 500, SEEK_SET);
        if (ret != 0) {
            rewind(f3);
            syslog(LOG_ERR, "%s: fseek set failed to use partial filsyslog e\n", __func__);
        } else {
            syslog(LOG_ERR, "%s: Soft kernel log buffer size is insufficient for syslog file so will use partial file\n", __func__);
        }
        s3 = size_log - curr_log_size - 2;
    }
    if (s3 > 0) {
        syslog(LOG_INFO, "%s: syslog file: Reading the file now\n", __func__);
        syslog(LOG_INFO, "%s: syslog file: Reading the file now\n", __func__);
        read_size = fread((void *)(bo_log + curr_log_size), sizeof(char), s3, f3);
        if (read_size > 0) {
            s3 = read_size;
        } else {
            s3 = 0;
        }
        return s3;
    }

    return -1;
}


__attribute__((visibility("default")))
int hello_world(void *args, struct sk_operations *ops)
{
    unsigned int boHandle_hello;
    unsigned int boHandle_log;
    char *bo_hello, *bo_log;
    openlog("xsoft_kernel", LOG_PID | LOG_CONS | LOG_NDELAY, LOG_NEWS);
    syslog(LOG_INFO, "%s: Started\n", __func__);
    time_t tmp_time = time(NULL);
    struct tm tmp_tm = *localtime(&tmp_time);
    syslog(LOG_INFO, "%s: Started: %d-%02d-%02d %02d:%02d:%02d\n", __func__, tmp_tm.tm_year+1900, tmp_tm.tm_mon+1, tmp_tm.tm_mday, tmp_tm.tm_hour, tmp_tm.tm_min, tmp_tm.tm_sec);

    if (!args) {
        syslog(LOG_ERR, "%s: Soft kernel args ptr is NULL\n", __func__);
        closelog();
        return -1;
    }
    if (!ops) {
        syslog(LOG_ERR, "%s: Soft kernel ops ptr is NULL\n", __func__);
        closelog();
        return -1;
    }
    regfile_t *ar = (regfile_t *)args;
    //unsigned int *ar = (unsigned int *)args;

    if (ar->size_hello > 511) {
        boHandle_hello = ops->getHostBO(ar->out_hello, ar->size_hello);
        //boHandle_hello = ops->getHostBO(0x800000000, 4096);//for debug
       
        bo_hello = (char *)ops->mapBO(boHandle_hello, true);
        //strcpy(bo_hello, "Hello World - ");
        snprintf(bo_hello, 510, "Hello World -  %d-%02d-%02d %02d:%02d:%02d", tmp_tm.tm_year+1900, tmp_tm.tm_mon+1, tmp_tm.tm_mday, tmp_tm.tm_hour, tmp_tm.tm_min, tmp_tm.tm_sec);

        //for debug
        //snprintf(bo_hello, 510, "Hello World - 0x%llx - 0x%llx - %d - %d - %d-%02d-%02d %02d:%02d:%02d", ar->out_hello, ar->out_log, ar->size_hello, ar->size_log, tmp_tm.tm_year+1900, tmp_tm.tm_mon+1, tmp_tm.tm_mday, tmp_tm.tm_hour, tmp_tm.tm_min, tmp_tm.tm_sec);
        //snprintf(bo_hello, 510, "Hello World - 0x%08x - 0x%08x - 0x%08x - 0x%08x - 0x%08x - 0x%08x - 0x%08x - 0x%08x - 0x%08x - 0x%08x - 0x%08x - 0x%08x - 0x%08x", ar[0], ar[1], ar[2], ar[3], ar[4], ar[5], ar[6], ar[7], ar[8], ar[9], ar[10], ar[11], ar[12]);

        ops->freeBO(boHandle_hello);
        syslog(LOG_INFO, "%s: Finished step-1: hello_world\n", __func__);
    } else {
        syslog(LOG_ERR, "%s: Soft kernel hello_world buffer size is smaller than 512. size = %d\n", __func__, ar->size_hello);
    }

    if (ar->size_log > 1023) {
        DIR *dir;
        struct dirent *item;

        if ((dir = opendir("/sys/class/drm/renderD128/device")) != NULL) {
            while ((item = readdir(dir)) != NULL) {
                syslog(LOG_INFO, "%s: drm dir tree: %s\n", __func__, item->d_name);
            }
            closedir(dir);
        } else {
            syslog(LOG_ERR, "%s: Unable to scan drm directory for kds files\n", __func__);
        }

        boHandle_log = ops->getHostBO(ar->out_log, ar->size_log);
        bo_log = (char *)ops->mapBO(boHandle_log, true);
        memset(bo_log, 0, ar->size_log);
        strcpy(bo_log, "-- TODO --");
        FILE *f1, *f2, *f3;
        unsigned int s1, s2, s3;
        unsigned int ptr_log_file = 0;
        f1 = fopen("/sys/class/drm/renderD128/device/kds_stat", "rb");
        if (!f1) {
            syslog(LOG_ERR, "%s: Soft kernel Unable to open kds_stat file\n", __func__);
        } else {
            s1 = write_log(f1, ar->size_log, bo_log, ptr_log_file);
            if (s1 > 0) {
                ptr_log_file = s1;
            } else {
                syslog(LOG_ERR, "%s: Soft kernel log buffer size is insufficient for kds_stat file\n", __func__);
            }
            fclose(f1);
        }

        f2 = fopen("/sys/class/drm/renderD128/device/kds_stats", "rb");
        if (!f2) {
            syslog(LOG_ERR, "%s: Soft kernel Unable to open kds_stats file\n", __func__);
        } else {
            s2 = write_log(f2, ar->size_log, bo_log, ptr_log_file);
            if (s2 > 0) {
                ptr_log_file += s2;
            } else {
                syslog(LOG_ERR, "%s: Soft kernel log buffer size is insufficient for kds_stats file\n", __func__);
            }
            fclose(f2);
        }

        f2 = fopen("/sys/class/drm/renderD128/device/kds_custat", "rb");
        if (!f2) {
            syslog(LOG_ERR, "%s: Soft kernel Unable to open kds_custat file\n", __func__);
        } else {
            s2 = write_log(f2, ar->size_log, bo_log, ptr_log_file);
            if (s2 > 0) {
                ptr_log_file += s2;
            } else {
                syslog(LOG_ERR, "%s: Soft kernel log buffer size is insufficient for kds_custat file\n", __func__);
            }
            fclose(f2);
        }
        f2 = fopen("/sys/class/drm/renderD128/device/kds_skstat", "rb");
        if (!f2) {
            syslog(LOG_ERR, "%s: Soft kernel Unable to open kds_skstat file\n", __func__);
        } else {
            s2 = write_log(f2, ar->size_log, bo_log, ptr_log_file);
            if (s2 > 0) {
                ptr_log_file += s2;
            } else {
                syslog(LOG_ERR, "%s: Soft kernel log buffer size is insufficient for kds_skstat file\n", __func__);
            }
            fclose(f2);
        }
        f3 = fopen("/var/log/messages", "rb");
        if (!f3) {
            s3 = 0;
            syslog(LOG_ERR, "%s: Soft kernel Unable to open syslog file\n", __func__);
        } else {
            s3 = write_syslog(f3, ar->size_log, bo_log, ptr_log_file);
            if (s3 > 0) {
                ptr_log_file += s3;
            } else {
                syslog(LOG_ERR, "%s: Soft kernel log buffer size is insufficient for syslog file\n", __func__);
            }
            fclose(f3);
        }
        if (ptr_log_file == 0) {
            strcpy(bo_log, "-- Unable to get log files --");
        }
        ops->freeBO(boHandle_log);
    } else {
        syslog(LOG_ERR, "%s: Soft kernel log buffer size is smaller than 1024. size = %d\n", __func__, ar->size_log);
    }

    syslog(LOG_INFO, "%s: Finished\n", __func__);
    closelog();
    return 0;
}



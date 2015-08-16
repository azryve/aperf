#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <curses.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/cpuctl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include "aperf.h"

struct config conf = {
                        .wait_intvl = 1,
                        .repeat_flag = 0,
                        .ncpu = 1
};

struct cpu_counters *cpus;

void update_counters(struct cpu_counters *cpu_cnts) {
    size_t i,j;
    for(i=0; i < conf.ncpu; ++i) {
        for(j=0; j < PER_CPU_COUNTERS; ++j) {
            struct cpu_msr *msr = &cpu_cnts[i].msrs[j];
            msr->old = msr->marg.data;
            ioctl(cpu_cnts[i].fd, CPUCTL_RDMSR, &msr->marg);
            msr->delta = msr->marg.data - msr->old;
        }
        cpu_cnts[i].throt_ratio = 1.0 * cpu_cnts[i].daperf / cpu_cnts[i].dmperf;
        cpu_cnts[i].busy_hz =   (cpu_cnts[i].dtsc / UNIT) *
                                (1.0 * cpu_cnts[i].daperf / cpu_cnts[i].dmperf);
    }
}

void init_counters(struct cpu_counters *cpu_cnts) {
    size_t i,j;
    cpuctl_cpuid_args_t idarg;

    for(i = 0; i < conf.ncpu; ++i) {
        char ctlpath[200];
        sprintf(ctlpath, "/dev/cpuctl%lu", i);
        cpu_cnts[i].fd = open(ctlpath, O_RDONLY);
        if(cpu_cnts[i].fd == -1) { 
            errx(1, "Error opening %s: ", ctlpath);
        }

        idarg.level = 0x80000007;
        ioctl(cpu_cnts[i].fd, CPUCTL_CPUID, &idarg);
        if(! (idarg.data[3] & (1 << 8))) {
            errx(1, "Invariant TSC not available on CPU%lu", i);
        }
        idarg.level = 0x6;
        ioctl(cpu_cnts[i].fd, CPUCTL_CPUID, &idarg);
        if(! (idarg.data[2] & 1)) {
            errx(1, "APERF/MPERF MSR not available on CPU%lu", i);
        }

        for(j=0; j < PER_CPU_COUNTERS; ++j) {
            struct cpu_msr *msr = &cpu_cnts[i].msrs[j];
            msr->marg.msr = msrs_to_count[j];
            ioctl(cpu_cnts[i].fd, CPUCTL_RDMSR, &msr->marg);
            msr->old = msr->marg.data;
            msr->delta = 0;
        }
    }
}

void parse_args(int argc, char *argv[]) {
    char ch;
    while((ch = getopt(argc, argv, "tw:")) != -1) {
        switch(ch) {
            case 't':
                conf.repeat_flag = 1;
                conf.wait_intvl = 1;
                break;
            case 'w':
                conf.wait_intvl = strtoul(optarg, 0, 10);
                if(conf.wait_intvl == 0) {
                    errx(1,"Illigal interval");
                }
                break;
            default:
                errx(1, "Usage:\n"
                         "-t : repeat indefinetly top-style\n"
                         "-w <i> : wait <i> seconds between scanning counters\n");
        }
    }
}


int main(int argc, char *argv[]) {
    size_t i;

    parse_args(argc, argv);

    i = sizeof(conf.ncpu);
    sysctlbyname("hw.ncpu", &conf.ncpu, &i, 0, 0);
    if((cpus = calloc(conf.ncpu, sizeof(struct cpu_counters))) == NULL) {
        errx(1, "Failed to alloc cpu counters");
    }
    init_counters(cpus);
    initscr();
    cbreak();
    do {
        sleep(conf.wait_intvl);
        erase();
        update_counters(cpus);
        for(i = 0; i < conf.ncpu; ++i) {
            printw("CPU%d:\t%8.2f\t%1.3f\n", i, cpus[i].busy_hz/conf.wait_intvl, cpus[i].throt_ratio);
        }
        refresh();
    } while (conf.repeat_flag);
    endwin();
    return 0;
}

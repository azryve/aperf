#define PER_CPU_COUNTERS 3
#define IA32_TSC    0x10
#define IA32_MPERF  0xe7
#define IA32_APERF  0xe8

#define Mhz	1000000.0
#define UNIT	Mhz

struct config {
    int repeat_flag;
    size_t wait_intvl;
    size_t ncpu;
};

uint32_t
msrs_to_count[PER_CPU_COUNTERS] =
{                           IA32_TSC,
                            IA32_MPERF,
                            IA32_APERF
};


#define dtsc	msrs[0].delta	/* shortcuts */
#define dmperf	msrs[1].delta
#define daperf	msrs[2].delta

struct cpu_msr {
    cpuctl_msr_args_t   marg;
    uint64_t            old;
    uint64_t            delta;
};

struct cpu_counters {
    int fd;
    struct cpu_msr msrs[PER_CPU_COUNTERS];
    float throt_ratio;				/* (APERF/MPERF) */
    float busy_hz;				/* Working clock rate in Mhz units */
};

void init_counters(struct cpu_counters *cpu_cnts);
void update_counters(struct cpu_counters *cpu_cnts);

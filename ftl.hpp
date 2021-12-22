#ifndef FTL_HPP
#define FTL_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Configs
extern long long SECTOR_SIZE;
extern long long SECTOR_PER_PAGE;
extern long long PAGES_PER_BLOCK;
extern long long PAGE_SIZE;
extern long long BLOCK_SIZE;
extern long long LOGICAL_FLASH_SIZE;
extern long long OP_REGION;
extern long long LOGICAL_PAGE;
extern long long FLASH_SIZE;
extern long long BLOCKS_PER_LOGICAL_FLASH;
extern long long BLOCKS_PER_FLASH;
extern long long PAGES_PER_FLASH;
extern int OP_PERCENTAGE;
extern int GC_THRESHOLD;
extern int GC_TYPE;
extern int GC_WINDOW_SIZE;
extern int cpu_num;

void ftl_init();
void ftl_close();

int ftl_write(int page_num, int cpu_id);
void ftl_discard(int page_num, int cpu_id);
void ftl_read (int page_num, int cpu_id);


//-------------------------------

// GC variables
extern long long total_invalid_cnt;

typedef struct _STATISTICS {
    long long read_cnt;
    long long write_cnt;
    long long discard_cnt;
    long long gc_cnt;
    long long copyback_cnt;
    double gc_run_time;
}STATISTICS;

extern STATISTICS total_stat;

typedef struct _CPU_STATISTICS {
    long long read_cnt;
    long long write_cnt;
    long long discard_cnt;
}CPU_STATISTICS;

extern CPU_STATISTICS *cpu_stat;


typedef struct _LOGICAL_MAP{ // This is a logical page table that stores physical page number
    int num;
}LOGICAL_MAP;

typedef struct _PHYSICAL_MAP{ // This is a physical page table that stores logical page number and status of the physical page
    int num;
    int is_valid;
}PHYSICAL_MAP;

extern LOGICAL_MAP *logical_map;
extern PHYSICAL_MAP *physical_map;


typedef struct _BLOCK_MAP{
    int invalid_cnt;
    int next_block;
    int erase_cnt;
    int cpu_id;
}BLOCK_MAP;

extern BLOCK_MAP *block_map;


typedef struct _CURRENT_STATE{
    int block;
    int page;
}CURRENT_STATE;

extern CURRENT_STATE *current_state;


typedef struct _FREE_BLOCKS{
    int cnt;
    int head;
    int tail;
}FREE_BLOCKS;

extern FREE_BLOCKS free_blocks;

#endif

#include "ftl.hpp"
#include <string>
#include <iostream>
#include <set>
#include <chrono>
#include <queue>
#include <boost/any.hpp>

using namespace std;
using namespace chrono;
using boost::any;

long long total_invalid_cnt;
queue<int> gc_queue;
bool debug = false;

STATISTICS total_stat;
CPU_STATISTICS *cpu_stat;
LOGICAL_MAP *logical_map;
PHYSICAL_MAP *physical_map;
BLOCK_MAP *block_map;
CURRENT_STATE *current_state;
FREE_BLOCKS free_blocks;

// Time GC function run time
void time_callable(void (*func)()) {
  auto t_start = high_resolution_clock::now();
  func();
  auto t_end = high_resolution_clock::now();
  duration<double, std::milli> ms_double = t_end - t_start;
  total_stat.gc_run_time += ms_double.count();
}

void init_stat() {

    total_stat.read_cnt = 0;
    total_stat.write_cnt = 0;
    total_stat.discard_cnt = 0;
    total_stat.copyback_cnt = 0;
    total_stat.gc_cnt = 0;
    total_stat.gc_run_time = 0.0;
    total_invalid_cnt = 0;

    cpu_stat = new CPU_STATISTICS[cpu_num];
    for (int i=0 ; i < cpu_num; i++) {
        cpu_stat[i].read_cnt = 0;
        cpu_stat[i].write_cnt = 0;
        cpu_stat[i].discard_cnt = 0;
    }
}

void ftl_init() {
    init_stat();
    
    free_blocks.cnt = BLOCKS_PER_FLASH;
    free_blocks.head = 0;
    free_blocks.tail = BLOCKS_PER_FLASH - 1;

    logical_map = new LOGICAL_MAP[LOGICAL_PAGE];
    physical_map = new PHYSICAL_MAP[PAGES_PER_FLASH];
    block_map = new BLOCK_MAP[BLOCKS_PER_FLASH];

    current_state = new CURRENT_STATE[cpu_num];

    for(long long i = 0; i < LOGICAL_PAGE; i++) 
        logical_map[i].num = -1;

    for(long long i = 0; i < PAGES_PER_FLASH; i++) {
        physical_map[i].num = -1;
        physical_map[i].is_valid = 0;
    }

    for(long long i = 0; i < BLOCKS_PER_FLASH; i++) {
        block_map[i].invalid_cnt = 0;
        block_map[i].next_block = i + 1;
        block_map[i].erase_cnt = 0;
        block_map[i].cpu_id = -1;
    }
    block_map[BLOCKS_PER_FLASH - 1].next_block = -1;

    for (long long i = 0; i < cpu_num; i++) {
        current_state[i].block = -1;
        current_state[i].page = 0;
    }
}

void ftl_close() {
    while(!gc_queue.empty()){
        gc_queue.pop();
    } 
    delete[] cpu_stat;
    delete[] physical_map;
    delete[] block_map;
    delete[] current_state;
    delete[] logical_map;
}

int fetch_free_block(int cpu_id) {
    
    if (free_blocks.cnt == 0)
        throw new exception;

    // Here we point to the next free block!
    int free_block_num = free_blocks.head;
    free_blocks.head = block_map[free_block_num].next_block;
    free_blocks.cnt--;

    // Update new free block's status
    block_map[free_block_num].cpu_id = cpu_id;

    return free_block_num;
}

void ftl_erase (int block_num) {
    block_map[free_blocks.tail].next_block = block_num;
    free_blocks.tail = block_num;
    free_blocks.cnt++;
    block_map[free_blocks.tail].next_block = -1;
    
    total_invalid_cnt -= block_map[block_num].invalid_cnt;
    block_map[block_num].erase_cnt++;
    block_map[block_num].invalid_cnt = 0;
    block_map[block_num].cpu_id = -1;
    
    try {
        if(free_blocks.head == -1){
            free_blocks.head = block_num;
            if(free_blocks.head != free_blocks.tail) {
                throw string("[ERROR] ftl_erase: free_blocks' linked list has an issue!");
            }
        }
    } catch (string err_message) {
        cout << err_message << endl;
        exit(0);
    }
    if(debug)cout << "erasing block: " << block_num << endl;
}

void ftl_copyback (int block_num) {
    int cpu_id = block_map[block_num].cpu_id;
    if(debug)cout << cpu_id << " gc block: " << block_num << endl;
    int start_page = block_num * PAGES_PER_BLOCK;
    for (int cur_page = start_page; cur_page < start_page + PAGES_PER_BLOCK; ++cur_page)
        if (physical_map[cur_page].is_valid) {
            try {
                if (current_state[cpu_id].page >= PAGES_PER_BLOCK) { // Reached the end of the current block!

                    // Handle GC Algo
                    switch(GC_TYPE){
                        case(0):
                            break;
                        case(1):
                            gc_queue.push(current_state[cpu_id].block);
                            break;
                        case(2):
                            gc_queue.push(current_state[cpu_id].block);
                            while(gc_queue.size() > GC_WINDOW_SIZE)
                                gc_queue.pop();
                            break;
                        default:
                            break;
                    }
                    int fetched_block;

                    try{
                        fetched_block = fetch_free_block(cpu_id);
                    }catch(exception e){
                        cout << "[ERROR] Couldn't find any free block for copy back!\n";
                        exit(0);
                    }
                    if(debug) cout << cpu_id << "getting block during copyback" << fetched_block << endl;
                    current_state[cpu_id].block = fetched_block;
                    current_state[cpu_id].page = 0;
                }
            } catch (string err_message) {
                cout << err_message << endl;
                exit(0);
            }

            int page = physical_map[cur_page].num;
            
            // Free old pysical page
            physical_map[cur_page].num = -1;
            physical_map[cur_page].is_valid = 0;

            // Copyback
            int new_phys_num = current_state[cpu_id].block * PAGES_PER_BLOCK + current_state[cpu_id].page;
            logical_map[page].num = new_phys_num;
            physical_map[new_phys_num].num = page;
            physical_map[new_phys_num].is_valid = 1;
        
            // Update stats
            total_stat.copyback_cnt++;
            current_state[cpu_id].page++;
        }
}

int greedy_choose_gc_block() { // Choose a block for gc to free it
    
    int gc_block = -1;
    int max_invalid_cnt = -1;
    
    set<int> current_blocks;
    for (int i = 0; i < cpu_num; i++)
        current_blocks.insert(current_state[i].block);

    // Find a block with max invalid pages
    for (int i = 0; i < BLOCKS_PER_FLASH; i++) {
        if (block_map[i].cpu_id == -1 || current_blocks.count(i)) // skip current and free blocks
            continue;
        if (block_map[i].invalid_cnt > max_invalid_cnt) {
            max_invalid_cnt = block_map[i].invalid_cnt;
            gc_block = i;
        }
    }

    try {
        if (max_invalid_cnt > PAGES_PER_BLOCK)
            throw string("[ERROR] Invalid_cnt out of bound!\n");
    } catch (string err_message) {
        cout << err_message << endl;
    }

    try {
        if (gc_block == -1)
            throw string("[ERROR] All physical pages are valid! No block left for GC!\n");
    } catch (string err_message) {
        cout << err_message << endl;
        exit(0);
    }

    int gc_cpu = block_map[gc_block].cpu_id;
    try {
        if(gc_cpu < 0 || cpu_num <= gc_cpu){
            throw string("[ERROR] Invalid cpu_id for gc_block!\n");
        }
    } catch (string err_message) {
        cout << err_message << endl;
    }

    return gc_block;
}

int fifo_choose_gc_block() {
    try {
        if (gc_queue.size() == 0)
            throw string("[ERROR] No block is available for GC!\n");
    } catch (string err_message) {
        cout << err_message << endl;
    }

    int gc_block = gc_queue.front();
    gc_queue.pop();

    return gc_block;
}

int window_choose_gc_block() {
    try {
        if (gc_queue.size() == 0)
            throw string("[ERROR] No block is available for GC!\n");
    } catch (string err_message) {
        cout << err_message << endl;
    }

    int gc_block = -1;
    int max_invalid_cnt = -1;
    
    if(debug){
        cout << "queue before gc: " << endl;
        for (int i = 0; i < gc_queue.size(); i++) { // print queue before pop
            int current_block_num = gc_queue.front();
            gc_queue.pop();
            cout << current_block_num << " ";
            gc_queue.push(current_block_num);
        }
        cout << endl;
    }

    int queue_size = gc_queue.size();

    // Find a block with max invalid pages
    for (int i = 0; i < queue_size; i++) { // find most dirty block
        int current_block_num = gc_queue.front();
        gc_queue.pop();
        if (block_map[current_block_num].invalid_cnt > max_invalid_cnt) {
            max_invalid_cnt = block_map[current_block_num].invalid_cnt;
            gc_block = current_block_num;
        }
        gc_queue.push(current_block_num);
    }

    if(debug){
        cout << "queue between gc: " << endl;
        for (int i = 0; i < gc_queue.size(); i++) { // print queue before pop
            int current_block_num = gc_queue.front();
            gc_queue.pop();
            cout << current_block_num << " ";
            gc_queue.push(current_block_num);
        }
        cout << endl;
    }

    for (int i = 0; i < queue_size; i++) { // pop most dirty block
        int current_block_num = gc_queue.front();
        gc_queue.pop();
        if (current_block_num != gc_block) {
            gc_queue.push(current_block_num);
        }
    }

    if(debug){
        cout << "queue after gc: " << endl;
        for (int i = 0; i < gc_queue.size(); i++) { // print queue after pop
            int current_block_num = gc_queue.front();
            gc_queue.pop();
            cout << current_block_num << " ";
            gc_queue.push(current_block_num);
        }
        cout << endl;
        cout << endl;
    }
    return gc_block;
}

void ftl_gc() {
    
    int gc_block;

    // Find the proper block for gc
    switch(GC_TYPE){
        case(0):
            gc_block = greedy_choose_gc_block();
            break;
        case(1):
            gc_block = fifo_choose_gc_block();
            break;
        case(2):
            gc_block = window_choose_gc_block();
            break;
        default:
            gc_block = greedy_choose_gc_block();
            break;
    }
    
    // Copyback the valid pages of gc_block and mark the old physical pages as stale
    ftl_copyback(gc_block);
    
    // Erase and free gc_block and add it to free_blocks linked list
    ftl_erase(gc_block);


    total_stat.gc_cnt++;
    return;
}

void ftl_gc_on_threshold(){
    if (GC_THRESHOLD > 0){
        if(total_invalid_cnt * 100 / PAGES_PER_FLASH > GC_THRESHOLD){
            ftl_gc();
        }
    }
}

void search_free_block(int cpu_id) {
    
    if (free_blocks.cnt <= cpu_num) {
        
        try{
            current_state[cpu_id].block = fetch_free_block(cpu_id);
            
        }catch(exception e){
            cout << "[ERROR] Couldn't find any free block to write!\n";
            exit(0);
        }
        if(debug)cout << cpu_id << " getting free block: " << current_state[cpu_id].block << endl;
        current_state[cpu_id].page = 0;
        
        while (free_blocks.cnt < cpu_num){
            time_callable(&ftl_gc);
        }

        return;
    }
    
    current_state[cpu_id].block = fetch_free_block(cpu_id);
    current_state[cpu_id].page = 0;
    if(debug)cout << cpu_id << " getting free block: " << current_state[cpu_id].block << endl;

}

int ftl_write(int page_num, int cpu_id) {
    
    try {
        if (cpu_num <= cpu_id)
            throw(cpu_id);
    }
    catch (int cpu_id) {
        cout << "[ERROR] cpu_id(" << cpu_id << ") is out of bound!\n";
        exit(0);
    }
    
    if(current_state[cpu_id].block == -1){
        search_free_block(cpu_id);
    }
    if (logical_map[page_num].num != -1) {
        int old_phys_num = logical_map[page_num].num; // Find the physical page assigned to the page
        physical_map[old_phys_num].num = -1; // Break the link between the physical page & the page
        physical_map[old_phys_num].is_valid = 0; // Change the status of the physical page to invalid

        int block_num = old_phys_num / PAGES_PER_BLOCK;
        block_map[block_num].invalid_cnt++; // Update coressponding block's meta-data
        time_callable(&ftl_gc_on_threshold);
    }

    // Write New data to curBlock, curPage
    // if update block is full, get new free block to write
    while(current_state[cpu_id].page == PAGES_PER_BLOCK){
        
        if(debug) std::cout << cpu_id << " finished block: " << current_state[cpu_id].block << endl;

        // Handle GC Algo
        switch(GC_TYPE){
            case(0):
                break;
            case(1):
                gc_queue.push(current_state[cpu_id].block);
                break;
            case(2):
                gc_queue.push(current_state[cpu_id].block);
                while(gc_queue.size() > GC_WINDOW_SIZE)
                    gc_queue.pop();
                break;
            default:
                break;
        }

        total_invalid_cnt += block_map[current_state[cpu_id].block].invalid_cnt;
        //get new free block
        search_free_block(cpu_id);
    }
    // write data to new physical address
    int new_phys_num = current_state[cpu_id].block * PAGES_PER_BLOCK + current_state[cpu_id].page;
    logical_map[page_num].num = new_phys_num;
    physical_map[new_phys_num].num = page_num;
    physical_map[new_phys_num].is_valid = 1;
    
    total_stat.write_cnt++;
    cpu_stat[cpu_id].write_cnt++;
    current_state[cpu_id].page++;
    if(debug) cout << cpu_id << " write to block: " << current_state[cpu_id].block<<" page: "<< current_state[cpu_id].page<< endl;
    if(current_state[cpu_id].page > PAGES_PER_BLOCK) cout << cpu_id << " invalid page num write to block: " << current_state[cpu_id].block<<" page: "<< current_state[cpu_id].page<< endl;
    return 1;
}

void ftl_discard(int page_num, int cpu_id) {
    int phys_num;

    total_stat.discard_cnt++;
    cpu_stat[cpu_id].discard_cnt++;
    phys_num = logical_map[page_num].num;
    physical_map[phys_num].num = -1;
    physical_map[phys_num].is_valid = 0;

    int block_num = phys_num / PAGES_PER_BLOCK;
    block_map[block_num].invalid_cnt++;
    
    set<int> current_blocks;
    for (int i = 0; i < cpu_num; i++)
        current_blocks.insert(current_state[i].block);
    if (! current_blocks.count(block_num)){
        total_invalid_cnt++; // Update global meta-data
    }
    time_callable(&ftl_gc_on_threshold);
}

void ftl_read(int page_num, int cpu_id) {
    total_stat.read_cnt++;
    cpu_stat[cpu_id].read_cnt++;
}

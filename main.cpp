#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <regex>
#include <cmath>
#include "ftl.hpp"
#include <unistd.h>
#include <boost/program_options.hpp>
#include <boost/format.hpp>

using namespace std;
using boost::format;
namespace po = boost::program_options;

string blktrace_file_path;
string log_file_path;

long long SECTOR_SIZE;
long long SECTOR_PER_PAGE;
long long PAGES_PER_BLOCK;
long long LOGICAL_FLASH_SIZE;
int OP_PERCENTAGE;
int cpu_num;

long long PAGE_SIZE;
long long BLOCK_SIZE;
long long OP_REGION;
long long LOGICAL_PAGE;
long long FLASH_SIZE;
long long BLOCKS_PER_LOGICAL_FLASH;
long long BLOCKS_PER_FLASH;
long long PAGES_PER_FLASH;

int GC_THRESHOLD;
int GC_TYPE;
int GC_WINDOW_SIZE;

void config_init(int argc, char *argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "produce help message")
    ("input_file,i", po::value<string>(&blktrace_file_path), "Path to blktrace file to simulate")
    ("log_file,o", po::value<string>(&log_file_path), "Path to log file to output simulation result")
    ("lsec_size", po::value<long long>(&SECTOR_SIZE)->default_value(512), "Controller Sector Size")
    ("lsecs_per_pg", po::value<long long>(&SECTOR_PER_PAGE)->default_value(8), "Number of sectors in a flash page")
    ("lpgs_per_blk", po::value<long long>(&PAGES_PER_BLOCK)->default_value(1024), "Number of pages per flash block")
    ("lsize", po::value<long long>(&LOGICAL_FLASH_SIZE)->default_value(134217728), "Total logical size of the flash")
    ("op", po::value<int>(&OP_PERCENTAGE)->default_value(7), "Percentage of over provisioning size, 1 block is provisioned if smaller than block size")
    ("cpu", po::value<int>(&cpu_num)->default_value(2), "number of cpu threads")
    ("gc_th", po::value<int>(&GC_THRESHOLD)->default_value(-1), "Percentage threshold of full blocks' invalid pages to trigger GC. Set to -1 will only do GC at last free block")
    ("gc_type", po::value<int>(&GC_TYPE)->default_value(0), "GC types 0: Gready 1: fifo 2: Sliding Window")
    ("gc_win_size", po::value<int>(&GC_WINDOW_SIZE)->default_value(0), "Set when gc_type is 2: sliding window algorithm");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        cout << desc << "\n";
        exit(0);
    }

    if (vm.count("input_file"))
    {
        cout << "BLktrace file path was set to "
             << blktrace_file_path << ".\n";
    }
    else
    {
        cout << "input_file was not set.\n";
        exit(0);
    }
    if (vm.count("log_file"))
    {
        cout << "Simulation result file path was set to "
             << log_file_path << ".\n";
    }
    else
    {
        cout << "log_file was not set.\n";
        exit(0);
    }
    if (vm.count("lsec_size"))
    {
        cout << "Controller Sector Size was set to "
             << SECTOR_SIZE << ".\n";
    }
    if (vm.count("lsecs_per_pg"))
    {
        cout << "Number of sectors in a flash page was set to "
             << SECTOR_PER_PAGE << ".\n";
    }
    if (vm.count("lpgs_per_blk"))
    {
        cout << "Number of pages per flash block was set to "
             << PAGES_PER_BLOCK << ".\n";
    }
    if (vm.count("lsize"))
    {
        cout << "Total logical size of the flash was set to "
             << LOGICAL_FLASH_SIZE << ".\n";
    }
    if (vm.count("op"))
    {
        cout << "Percentage of over provisioning size was set to "
             << OP_PERCENTAGE << ".\n";
    }
    if (vm.count("cpu"))
    {
        cout << "Number of cpu threads was set to "
             << cpu_num << ".\n";
    }
    if (vm.count("gc_th"))
    {
        cout << "Percentage threshold of invalid pages to trigger GC was set to "
             << GC_THRESHOLD << ".\n";
    }
    if (vm.count("gc_type"))
    {
        cout << "GC types was set to "
             << GC_TYPE << ".\n";
    }
    if (vm.count("gc_win_size") && GC_WINDOW_SIZE != 0)
    {
        cout << "GC window size was set to "
             << GC_WINDOW_SIZE << ".\n";
    }
    PAGE_SIZE = SECTOR_SIZE * SECTOR_PER_PAGE;
    BLOCK_SIZE = PAGE_SIZE * PAGES_PER_BLOCK;
    BLOCKS_PER_LOGICAL_FLASH = LOGICAL_FLASH_SIZE / BLOCK_SIZE;
    OP_REGION = max(1, (int)(BLOCKS_PER_LOGICAL_FLASH * OP_PERCENTAGE / 100)) * BLOCK_SIZE;
    LOGICAL_PAGE = LOGICAL_FLASH_SIZE / PAGE_SIZE;
    FLASH_SIZE = LOGICAL_FLASH_SIZE + OP_REGION;
    BLOCKS_PER_FLASH = FLASH_SIZE / BLOCK_SIZE;
    PAGES_PER_FLASH = FLASH_SIZE / PAGE_SIZE;
}

char parse_blktrace_line(ifstream *input_stream, int *cpu_id, long long *start_page, long long *num_page, string *action, long long *current_line_num)
{
    string RWBS;
    char op_code;
    string line;
    regex cpu_id_regex("^(?:\\s+)?(?:\\S+\\s+){1}(\\d+)");
    regex action_regex("^(?:\\s+)?(?:\\S+\\s+){5}(\\w+)");
    regex RWBS_regex("^(?:\\s+)?(?:\\S+\\s+){6}(\\w+)");
    regex start_sec_regex("^(?:\\s+)?(?:\\S+\\s+){7}(\\d+)");
    regex num_sec_regex("^(?:\\s+)?(?:\\S+\\s+){9}(\\d+)");
    cmatch regex_result;

    if (getline(*input_stream, line))
    {   
        try{
            (*current_line_num)++;
            regex_search(line.c_str(), regex_result, cpu_id_regex);
            *cpu_id = stoi(regex_result[1]);
            regex_search(line.c_str(), regex_result, action_regex);
            *action = regex_result[1];
            regex_search(line.c_str(), regex_result, RWBS_regex);
            RWBS = regex_result[1];
            op_code = RWBS[0];
            if((*action)[0] == 'U' || op_code == 'F'){
                return op_code;
            }
            if(*action == "C" && RWBS == "WFS"){
                return op_code;
            }
            regex_search(line.c_str(), regex_result, start_sec_regex);
            *start_page = stoll(regex_result[1]) / SECTOR_PER_PAGE;
            if(*start_page == 0){
                return 'F';
            }
            regex_search(line.c_str(), regex_result, num_sec_regex);
            *num_page = ceil(stod(regex_result[1]) / SECTOR_PER_PAGE);
            
        }
        catch (const exception & ex)
        {      
            cout << "[ERROR] parse_blktrace_line: blktrace file format is incorrect!" << endl;
            cout <<  "Line "<< *current_line_num << " :"  << line.c_str() << endl;
            exit(0);
        }

        try
        {
            if (*start_page > LOGICAL_PAGE)
            {  
                throw string("[ERROR] parse_blktrace_line: page number exceeds total logical flash size!");
            }
        }
        catch (string err_message)
        {
            cout << err_message << endl;
            exit(0);
        }
        
    }
    else
    {
        printf("End of the file!\n");
    }
    
    return op_code;
}

void print_stat_summary()
{   
    float waf, gc_avg_runtime;
    
    cout << "Total Stats\n";
    
    if (total_stat.write_cnt != 0)
        waf = (float)(total_stat.write_cnt + total_stat.copyback_cnt) / total_stat.write_cnt;
    else
        waf = 0;
    
    if (total_stat.gc_cnt != 0)
        gc_avg_runtime = (float)(total_stat.gc_run_time) / total_stat.gc_cnt;
    else
        gc_avg_runtime = 0;
    cout << format(" %-20s %-20s %-20s %-20s %-40s %-20s %-40s\n")  % "Read Count" % "Write Count" % "Discard Count" % "GC Count" % "GC Average Runtime(ms)" % "Copyback Count" % "Write Amplification Factor";
    cout << format(" %-20lld %-20lld %-20lld %-20lld %-40.5f %-20lld %-40.2f\n")  % total_stat.read_cnt % total_stat.write_cnt % total_stat.discard_cnt % total_stat.gc_cnt % gc_avg_runtime % total_stat.copyback_cnt % waf;
    cout << endl;
    
}

void print_progress(long long current_line_num, long long total_line_num)
{
    int bar_width = 70;
    float progress = (float) current_line_num / total_line_num;
    cout << "Running Simulation: [";
    int pos = bar_width * progress;
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) cout << "=";
        else if (i == pos) cout << ">";
        else cout << " ";
    }
    cout << fixed << setprecision(4);
    cout << "] " << setw(6) << progress * 100.0 << " %\r";
    cout.flush();
    
}

long long get_num_of_lines_in_file(string file_path)
{
    ifstream file_to_count(file_path);   
    string line;
	long long count=0; 

	if(file_to_count.is_open()) 
	{
		while(file_to_count.peek()!=EOF)
		{
			getline(file_to_count, line);
			count++;
		}
		file_to_count.close();
	}
    return count;
}

void write_stat_file(string file_path)
{
    ofstream log_file;
    try{
        log_file.open(file_path);
    }catch(exception) {
        cout << "[ERROR] Failed to open log file\n" << endl;
        exit(0);
    }

    float waf, gc_avg_runtime;

    log_file << "Total Stats\n";

    if (total_stat.write_cnt != 0)
        waf = (float)(total_stat.write_cnt + total_stat.copyback_cnt) / total_stat.write_cnt;
    else
        waf = 0;

    if (total_stat.gc_cnt != 0)
        gc_avg_runtime = (float)(total_stat.gc_run_time) / total_stat.gc_cnt;
    else
        gc_avg_runtime = 0;
        

    log_file << format(" %-20s %-20s %-20s %-20s %-40s %-20s %-40s\n")  % "Read Count" % "Write Count" % "Discard Count" % "GC Count" % "GC Average Runtime(ms)" % "Copyback Count" % "Write Amplification Factor";
    log_file << format(" %-20lld %-20lld %-20lld %-20lld %-40.5f %-20lld %-40.2f\n")  % total_stat.read_cnt % total_stat.write_cnt % total_stat.discard_cnt % total_stat.gc_cnt % gc_avg_runtime % total_stat.copyback_cnt % waf;

    log_file << "Stats By CPU\n";
    for(int cpu_id = 0; cpu_id < cpu_num; cpu_id++){
        log_file << format("CPU%d:\n") % cpu_id;
        log_file << format(" %-20s %-20s %-20s\n")  % "Read Count" % "Write Count" % "Discard Count";
        log_file << format(" %-20lld %-20lld %-20lld\n")  % cpu_stat[cpu_id].read_cnt % cpu_stat[cpu_id].write_cnt % cpu_stat[cpu_id].discard_cnt;
    }

    log_file << "Block Stats\n";
    log_file << format(" %-20s %-20s %-20s\n")  % "Block Number" % "Invalid Count" % "Erase Count";
    for(int block_num = 0; block_num < BLOCKS_PER_FLASH; block_num++){
        log_file << format(" %-20d %-20d %-20d\n")  % block_num % block_map[block_num].invalid_cnt % block_map[block_num].erase_cnt;
    }

    log_file.close();

}

int main(int argc, char *argv[])
{

    int cpu_id = 0;
    long long start_page, num_page;
    char op_code;
    string action;
    ifstream input_stream;
    config_init(argc, argv);
    ftl_init();

    try{
        input_stream.open(blktrace_file_path);
    } catch(exception) {
        cout << "[ERROR] Failed to open blktrace file\n" << endl;
        exit(0);
    }

    try{
        if(!input_stream.good())
            throw string("[ERROR] blktrace file is not valid\n");
    } catch(string err_message) {
        cout << err_message << endl;
        exit(0);
    }

    long long total_line_num = get_num_of_lines_in_file(blktrace_file_path);
    long long current_line_num = 0;

    while (input_stream.peek() != EOF && input_stream.peek() != 67)
    {
        op_code = parse_blktrace_line(&input_stream, &cpu_id, &start_page, &num_page, &action, &current_line_num);
        if (action == "D")
        {
            switch (op_code)
            {
            case 'W':
                for (int page_offset = 0; page_offset < num_page; page_offset++)
                    ftl_write(start_page + page_offset, cpu_id);
                break;
            case 'D':
                for (int page_offset = 0; page_offset < num_page; page_offset++)
                    ftl_discard(start_page + page_offset, cpu_id);
                break;
            case 'R':
                for (int page_offset = 0; page_offset < num_page; page_offset++)
                    ftl_read(start_page + page_offset, cpu_id);
                break;
            default:
                break;
            }
        }
        print_progress(current_line_num, total_line_num);
    }
    print_progress(1, 1);
    input_stream.close();
    cout << "Simulation Finished!" << endl;
    print_stat_summary();
    write_stat_file(log_file_path);
    ftl_close();

    return 0;
}

# SSD Simulator
This is a flash-based page-mapping SSD simulator, which gets a block trace log file and relevant configs as input and outputs the desired statistics. It is authored by Hoaran Liu and Hossein Ghotbaddini in Dec 2021 as the course project for Topics in Storage Systems course (CSC2233) at the University of Toronto (Instructor -- Prof. Bianca Schroeder).
## Setup Instruction -- Ubuntu
*  Clone this branch to a local directory
  ``` 
  git clone https://github.com/HosseinGhotbaddini/ssd_simulator.git
  ```
*  Navigate to the project directory
  ``` 
  cd ssd_simulator
  ```
*  Install required compiler and libraries
  ``` 
  sudo apt install build-essential libboost-system-dev libboost-thread-dev libboost-program-options-dev libboost-test-dev
  ```
*  Compile and build
  ``` 
  cmake .
  make
  ```
*  Run with `-h` to see available options
  ``` 
  ./ssd_simulator -h
  ```
* Available Options
  ```
  -h [ --help ]              produce help message
  -i [ --input_file ] arg    Path to blktrace file to simulate
  -o [ --log_file ] arg      Path to log file to output simulation result
  --lsec_size arg (=512)     Controller Sector Size
  --lsecs_per_pg arg (=8)    Number of sectors in a flash page
  --lpgs_per_blk arg (=1024) Number of pages per flash block
  --lsize arg (=134217728)   Total logical size of the flash
  --op arg (=7)              Percentage of over provisioning size, 1 block is 
                             provisioned if smaller than block size
  --cpu arg (=2)             number of cpu threads
  --gc_th arg (=-1)          Percentage threshold of full blocks' invalid pages
                             to trigger GC. Set to -1 will only do GC at last 
                             free block
  --gc_type arg (=0)         GC types 0: Gready 1: fifo 2: Sliding Window
  --gc_win_size arg (=0)     Set when gc_type is 2: sliding window algorithm
  ```


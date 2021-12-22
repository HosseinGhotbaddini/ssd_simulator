

for op in 7 14 28
  do 
    for gc_type in 0 1
      do 
        ./ssd_simulator -i blktrace_256mb_randrw_cpu0-1.out -o result_256mb_randrw_cpu0-1_th-1_type$gc_type-op$op.txt --cpu 2 --lsize 268435456 --gc_type $gc_type --op $op
    done
done
      
for gc_win_size in {44..64..4}
  do 
    ./ssd_simulator -i blktrace_256mb_randrw_cpu0-1.out -o result_256mb_randrw_cpu0-1_th-1_type2_win$gc_win_size.txt --cpu 2 --lsize 268435456 --gc_type 2 --gc_win_size $gc_win_size
done


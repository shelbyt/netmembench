# 2 cores
CORES="0xaa"
#CORE_ARRAY=("0xa" "0xaa")
MEM_ARRAY=(0 1 2 4 8 16 32 64)
MEM_LOOP_INDEX=${#MEM_ARRAY[@]-1}

#for i in `seq 0 $MEM_LOOP_INDEX`
for i in `seq 0 $MEM_LOOP_INDEX`
do
    for j in `seq 0 3`
    do 
        echo $i
        sed -i "52s/.*/#define MEM_ACCESS_PER_PACKET ${MEM_ARRAY[$i]}/" reflect.c
        make clean; make
        sudo timeout -s SIGINT 30 perf stat -e  r81d0 -e r82d0 -e r20d1 -e r01d3 -e r41d0 ./build/app/reflect -c $CORES -n 4 -w 81:00.0 |& tee 64_${MEM_ARRAY[$i]}_${CORES}_${j}
        echo "GO"
        sleep 2
done
done 

#echo "240================"
#perf stat -a -e  r81d0 -e r82d0 -e r20d1 -e r01d3 -e r41d0  timeout 240 ./build/app/reflect -c 0x1 -n 4 -w 04:00.0
#sleep 5

perf stat -a -e  r81d0 -e r82d0 -e r20d1 -e r01d3 -e r41d0  timeout 20 ./build/app/reflect -c 0x1 -n 4 -w 04:00.0

#sleep 5
#
#perf stat -a -e  r81d0 -e r82d0 -e r20d1 -e r01d3 -e r41d0  timeout 180 ./build/app/reflect -c 0x1 -n 4 -w 04:00.0
#
#sleep 5
#
#perf stat -a -e  r81d0 -e r82d0 -e r20d1 -e r01d3 -e r41d0  timeout 150 ./build/app/reflect -c 0x1 -n 4 -w 04:00.0
#
#sleep 5
#
#perf stat -a -e  r81d0 -e r82d0 -e r20d1 -e r01d3 -e r41d0  timeout 120 ./build/app/reflect -c 0x1 -n 4 -w 04:00.0
#
#sleep 5
#
#perf stat -a -e  r81d0 -e r82d0 -e r20d1 -e r01d3 -e r41d0  timeout 90 ./build/app/reflect -c 0x1 -n 4 -w 04:00.0
#
#sleep 5
#
#perf stat -a -e  r81d0 -e r82d0 -e r20d1 -e r01d3 -e r41d0  timeout 60 ./build/app/reflect -c 0x1 -n 4 -w 04:00.0
#
#sleep 5
#
#perf stat -a -e  r81d0 -e r82d0 -e r20d1 -e r01d3 -e r41d0  timeout 30 ./build/app/reflect -c 0x1 -n 4 -w 04:00.0
#

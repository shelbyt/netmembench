cmd_l3fwd_lpm.o = gcc -Wp,-MD,./.l3fwd_lpm.o.d.tmp  -m64 -pthread  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_FSGSBASE -DRTE_MACHINE_CPUFLAG_F16C -DRTE_MACHINE_CPUFLAG_AVX2  -I/var/lib/dpdk/examples/l3fwd/build/include -I/var/lib/dpdk/x86_64-native-linuxapp-gcc/include -include /var/lib/dpdk/x86_64-native-linuxapp-gcc/include/rte_config.h -I/var/lib/dpdk/examples/l3fwd -O3  -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition -Wpointer-arith -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral -Wformat-security -Wundef -Wwrite-strings    -o l3fwd_lpm.o -c /var/lib/dpdk/examples/l3fwd/l3fwd_lpm.c 

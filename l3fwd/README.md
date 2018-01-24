To run this stuff:

Compile as normal on DPDK 16.11

To run:

`sudo ./build/l3fwd -l 6 -n 4 -- -p 0x3 -P -L --config "(0,0,6),(1,0,6)" --parse-ptype --table-size 65536`

Change the `--table-size` argument as appropriate

By default, the major part of the IPv4 address is expected to be `8`. So, rules are enumerated from `8.0.0.0` forward.

Thus, in pktgen, you want to run the following:

`set 0 dst ip 8.0.0.0`
`enable 0 random`
`set 0 rnd 0 30 ........_........_XXXXXXXX_XXXXXXXX`

The number of `X`s is dependent on the number of rules you put in `--table-size`. You want `log_2(table_size)` `X`s inside of the command above, as that will match the possible IP range to the number of rules that were added. So in the case above, the table size of 65536 matches the 16 `X`s that are present (as `2^16 = 65536`).



EM stuff:
Using DPDK 17.08:

`export RTE_SDK=/var/lib/dpdk-17.08`

DPDK dir is now dpdk-17.08, l3fwd app in there is modified
Command to run:

`sudo ./build/l3fwd -l 4,6 -n 4 -- -p 0x3 -P -E --config "(0,0,6),(1,0,6)" --parse-ptype --hash-entry-num 0x100000`

That's 2^20 entries

For this, it flips between 1.0.0.0 and 3.0.0.0. So in pktgen, the command becomes:

`set 0 rnd 0 30 ......X._.....XXX_XXXXXXXX_XXXXXXXX`

That's still 20 `X`'s, but one of them is now where the subnet flip is

Pktgen command:

`sudo ./app/x86_64-native-linuxapp-gcc/pktgen -l 1,2,3,4,5 -n 4 --proc-type auto --file-prefix pg -- -N -T -P --crc-strip -m "[2:3].0" -m "[4:5].1" -f themes/black-yellow2.theme`
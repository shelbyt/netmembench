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

In pktgen:
set 0 type ipv4
set 0 proto udp
set 0 src ip 7.7.7.7/24
set 0 dst ip 1.0.0.0
set 0 sport 7777
set 0 dport 8888



======================
### OLD EXPERIMENT, IGNORE

Setting up rate experiment:

Running l3fwd:
`sudo ./build/l3fwd -l 3,5,7,9,11,13,15,17,19 -n 4 -w 81:00.0 -w 81:00.1 -- -p 0x3 -P -E --config "(0,0,5),(0,1,7),(0,2,9),(0,3,11),(1,0,13),(1,1,15),(1,2,17),(1,3,19)" --parse-ptype --hash-entry-num 0x10000 --eth-dest=0,3c:fd:fe:a5:c2:c8 --eth-dest=1,3c:fd:fe:a5:c2:c9`
^ the MAC addrs need to match the appropriate ports on the client machine

Running pktgen:
`sudo ./app/x86_64-native-linuxapp-gcc/pktgen -l 3,5,7,9,11,13,15,17,19 -n 4 --proc-type auto --file-prefix pg -w 81:00.0 -w 81:00.1 -- -N -T --crc-strip -m "[5:7].0, [9:11].0, [13:15].1, [17:19].1" -f themes/black-yellow.theme`
^ promisc flag is gone

set 0 type ipv4
set 0 proto udp
set 0 src ip 7.7.7.7/24
set 0 dst ip 1.0.0.0
set 0 sport 7777
set 0 dport 8888

set 1 type ipv4
set 1 proto udp
set 1 src ip 7.7.7.7/24
set 1 dst ip 3.0.0.0
set 1 sport 7777
set 1 dport 8888

enable 0 random
enable 1 random

set 0 rnd 0 30 ........_........._.XXXXXXX_XXXXXXXX
set 1 rnd 0 30 ........_........._.XXXXXXX_XXXXXXXX
^ for 2^16 entries

Installing LUA lib:
sudo apt install lua-socket
sudo ln -s /usr/lib/x86_64-linux-gnu/lua /usr/local/lib/lua
sudo ln -s /usr/share/lua/ /usr/local/share/lua

Running the LUA script:
lua 'f, e = loadfile("scripts/drop_rate.lua"); f();'

files are in /tmp/pktgen_loss_pX.out where X is the port ID (0 or 1)

## CEASE IGNORE
======================
Setting up rate experiment:

compiling l3fwd:
`USER_FLAGS+=-DEXP_WRITE_PORT_STATS make`

Running l3fwd on 17:
`sudo ./build/l3fwd -l 3,5,7,9,11 -n 4 -w 81:00.0 -w 81:00.1 -- -p 0x3 -E --config "(0,0,5),(0,1,7),(1,0,9),(1,1,11)" --parse-ptype --hash-entry-num 0x10000 --eth-dest=0,3c:fd:fe:a8:a4:50 --eth-dest=1,3c:fd:fe:a8:a4:51`
^ the MAC addrs need to match the appropriate ports on the client machine

Running pktgen on 15:
`sudo ./app/x86_64-native-linuxapp-gcc/pktgen -l 3,5,7,9,11,13,15,17,19 -n 4 --proc-type auto --file-prefix pg -w 81:00.0 -w 81:00.1 -- -N -T --crc-strip -m "[5:7].0, [9:11].0, [13:15].1, [17:19].1" -f themes/white-black.theme`
^ promisc flag is gone

set 0 type ipv4
set 0 proto udp
set 0 src ip 7.7.7.7/24
set 0 dst ip 3.0.0.0
set 0 sport 7777
set 0 dport 8888
set 0 dst mac 3C:FD:FE:A5:C2:C8
set 0 size 192

set 1 type ipv4
set 1 proto udp
set 1 src ip 7.7.7.7/24
set 1 dst ip 1.0.0.0
set 1 sport 7777
set 1 dport 8888
set 1 dst mac 3C:FD:FE:A5:C2:C9
set 1 size 192

set all rate 50

enable all random
set all rnd 0 30 ........_........._.XXXXXXX_XXXXXXXX

^ for 2^16 entries

file is /tmp/pktgen_loss.out
format is time_since_start,delta_since_last,delta_recv_0,delta_missed_0,delta_recv_1,delta_missed_1,delta_recv_total,delta_missed_total

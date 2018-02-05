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


==========================
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


Getting mem experiment set up:

There's a few `#define`s that set this up. The `make` command looks like this:

`USER_FLAGS+="-DEXP_MEM_ACCESS_IN_THREAD -DEXP_NUM_ACCESS_PER_FWD=4096 -DEXP_MEM_ARR_SIZE=4194304" make`

^ that gets a 4MB array with 4096 lookups per batch of packets. Set the batch size in pktgen appropriately to get number of access/packet.

There's two other defines in `l3fwd.h` that control initial offset per lcore and the stride size.

You can combine that define with the one for stats dumping; they don't affect one another.




===============
RPC Experiments
===============
RPC requires BESS, a separate program installed on 15/17 as a different user, `rotor`.

`ssh rotor@b09-15`
`ssh rotor@b09-17`

After logging in, there's a different DPDK library that needs to be used for BESS.
`RTE_SDK` and `RTE_TARGET` are already set, so don't worry about those.
The DPDK library is in `~/bess/deps/dpdk-17.11`.
In order, do the following:

1. Unbind the NICs from the IGB UIO driver.
2. Insert the IGB UIO module (this will remove the old module as well).
3. Map hugepages (this will unmap the old hugepages as well). 16 per NUMA domain.
4. Bind the NICs to the IGB UIO driver.

Next, go into `~/bess` and run `./bessctl/bessctl`.
This is the central command line interface to control BESS.
This allows you to run scripts that contain the setup for BESS to use.

Once in this, you will either see `localhost:10514 $` or `<disconnected> $`.
The former means `bessd` is already running, and you may want to `daemon stop` or `daemon reset` to get it to a clean state.
The latter means the daemon is stopped, and you'll need to run the following:

`daemon start -c 47`

That'll start up the daemon on core 47 (NUMA domain 1).
Next, we want to run the actual script that will execute the experiment.
_You must run the server script first_ on `b09-17`.

On b09-17: `run cacheproj_server`. *Wait for this to finish!*

On b09-15: `run cacheproj_client`

This should get the experiment running. BESS will send until you tell it to stop, but there's a couple ways to get statistics:

`monitor port dev0` and `show port dev0` will query the port `dev0` (there's `dev1` as well).
The former will print statistics for the last second about throughput and drops, and the latter will just print NIC stat counters.

To get histogram data, run the following:

`command module measure0 get_summary MeasureCommandGetSummaryArg {'latency_percentiles': [50.0, 90.0, 99.0]}`

This will get the histogram summary for the module `measure0`. There's one measure module for each client (see below for more info).
If you want to clear the histogram, add `'clear': True` to the dictionary at the end of the command.

The scripts have a number of configurable options to set the number of clients, array size, number of memory lookups per packet, etc.
These are all selected when the `run` command is entered, but will choose defaults if nothing is specified.
The script will print all the variables you can set when you run it (with a warning saying it used the default).
To set a variable, do something like this:

`run cacheproj_server BESS_PKT_SIZE=192, BESS_NUM_SERVERS=6, BESS_NUM_UPDATES=4`

That'll set up 6 servers (3 per port) with 4 updates from memory per packet, and a packet size of 192 bytes.
There's some restrictions on the variables, but the script checks these for you to make sure nothing's wrong.
_However, make sure `BESS_ARR_SIZE` is the same on both machines._ Also, it's good for it to be a power of 2.

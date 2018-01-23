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

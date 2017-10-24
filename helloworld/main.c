/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

#include <inttypes.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>

#include <rte_cycles.h>


#define RX_RING_SIZE 128
#define TX_RING_SIZE 512
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32


int main(int argc, char **argv)
{
    int ret;
    int i;
    int r_int; 
    unsigned lcore_id;
    struct rte_mempool *mbuf_pool;
    struct rte_mbuf *bufs[BURST_SIZE];

    srand(time(NULL));

    // Using chars because guarenteed to be 1 byte 
    char *data; 
    // Allocate 1024*1024-> 1mb * 500 which is larger than cache size
    int bytes = (1024*1024*500);
    data = (char *) malloc(bytes);

    ret = rte_eal_init(argc, argv);
    if (ret < 0) 
        rte_panic("Cannot init EAL\n");

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
            MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, 
            rte_socket_id());

    uint64_t start = rte_get_tsc_cycles();
    for(i=0; i < 10000000; i++) {
        rte_pktmbuf_alloc_bulk(mbuf_pool, bufs, BURST_SIZE);
        //r_int = rand() % 400000000;
        //data[r_int] = 'c';
        //r_int = rand() % 400000000;
        //data[r_int] = 'c';
        //r_int = rand() % 400000000;
        //data[r_int] = 'c';
        //r_int = rand() % 400000000;
        //data[r_int] = 'c';
        //r_int = rand() % 400000000;
        //data[r_int] = 'c';
        rte_pktmbuf_free(*bufs);
    }
    uint64_t end = rte_get_tsc_cycles();

    printf("lcore %d:Set Cost %" PRIu64 " cycles, and in %lf sec\n",
            0, end-start, ((double)(end-start))/rte_get_tsc_hz());

    return 0;
}

/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_config.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <unistd.h>
#include <signal.h>

#define PRINTME 10

#define RX_RING_SIZE 256
#define TX_RING_SIZE 512

#define RX_NUM_PACKETS 10000000


#define NUM_MBUFS (256*1024)//262144// 
#define MBUF_CACHE_SIZE 256

#define PORT_ID 0
#define QUEUE_PER_CORE 5
#define PKRQ_HWQ_IN_BURST 64
#define BURST_SIZE PKRQ_HWQ_IN_BURST
#define MEM_ACCESS_PER_PACKET 10
#define MEM_ACESS_PER_BURST (BURST_SIZE * MEM_ACCESS_PER_PACKET)

//We will iterate over 12 cores to find which are active
#define RTE_MAX_CORES 24
#define PACKET_SIZE 1536

#define DEFAULT_PKT_BURST   BURST_SIZE   /* Increasing this number consumes memory very fast */ //64
#define DEFAULT_RX_DESC     (DEFAULT_PKT_BURST*8*2) //1024
#define DEFAULT_TX_DESC     (DEFAULT_RX_DESC*2) //2048

//Also known as nb_mbufs
//#define MAX_MBUFS_PER_PORT  (DEFAULT_TX_DESC*8)/* number of buffers to support per port */ //16384
#define MAX_MBUFS_PER_PORT  (DEFAULT_TX_DESC*2)/* number of buffers to support per port */ //4096
//#define MAX_MBUFS_PER_PORT  (DEFAULT_TX_DESC*4)/* number of buffers to support per port */ //8192
#define MAX_SPECIAL_MBUFS   64
#define aMBUF_CACHE_SIZE     (MAX_MBUFS_PER_PORT/8)
#define DEFAULT_PRIV_SIZE   0
#define CACHE_SIZE       ((MAX_MBUFS_PER_PORT > RTE_MEMPOOL_CACHE_MAX_SIZE)?RTE_MEMPOOL_CACHE_MAX_SIZE:MAX_MBUFS_PER_PORT)
#define MBUF_SIZE (RTE_MBUF_DEFAULT_BUF_SIZE + DEFAULT_PRIV_SIZE) /* See: http://dpdk.org/dev/patchwork/patch/4479/ */

uint32_t map_lcore_to_queue[RTE_MAX_CORES];
uint32_t map_lcore_to_mpps[RTE_MAX_CORES] = {0};
char *r_mem_chunk;
char *tmp_mem_chunk;

static volatile int keepRunning = 1;

static struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .mq_mode = ETH_MQ_RX_RSS,
        .max_rx_pkt_len = ETHER_MAX_LEN,
        .split_hdr_size = 0,
        .header_split   = 0, /**< Header Split disabled */
        .hw_ip_checksum = 0, /**< IP checksum offload enabled */
        .hw_vlan_filter = 0, /**< VLAN filtering disabled */
        .jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
        .hw_strip_crc   = 0, /**< CRC stripped by hardware */
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
            .rss_hf = ETH_RSS_IP,
        },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    },
};

volatile struct app_stats {
        struct {
                uint64_t rx_pkts;
                uint64_t enqueue_pkts;
                uint64_t enqueue_failed_pkts;
        } rx __rte_cache_aligned;
        struct {
                uint64_t dequeue_pkts;
                uint64_t enqueue_pkts;
                uint64_t enqueue_failed_pkts;
        } wkr __rte_cache_aligned;
        struct {
                uint64_t dequeue_pkts;
                /* Too early pkts transmitted directly w/o reordering */
                uint64_t early_pkts_txtd_woro;
                /* Too early pkts failed from direct transmit */
                uint64_t early_pkts_tx_failed_woro;
                uint64_t ro_tx_pkts;
                uint64_t ro_tx_failed_pkts;
        } tx __rte_cache_aligned;
} app_stats;

    static inline void __attribute__((always_inline))
rte_pktmbuf_free_bulk(struct rte_mbuf *m_list[], int16_t npkts)
{
    while (npkts--)
        rte_pktmbuf_free(*m_list++);
}


static void
print_stats(void)
{
        const uint8_t nb_ports = rte_eth_dev_count();
        unsigned i;
        struct rte_eth_stats eth_stats;
        printf("\nRX thread stats:\n");
        printf(" - Pkts rxd:                            %"PRIu64"\n",
                                                app_stats.rx.rx_pkts);
        printf(" - Pkts enqd to workers ring:           %"PRIu64"\n",
                                                app_stats.rx.enqueue_pkts);
        printf("\nWorker thread stats:\n");
        printf(" - Pkts deqd from workers ring:         %"PRIu64"\n",
                                                app_stats.wkr.dequeue_pkts);
        printf(" - Pkts enqd to tx ring:                %"PRIu64"\n",
                                                app_stats.wkr.enqueue_pkts);
        printf(" - Pkts enq to tx failed:               %"PRIu64"\n",
                                                app_stats.wkr.enqueue_failed_pkts);
        printf("\nTX stats:\n");
        printf(" - Pkts deqd from tx ring:              %"PRIu64"\n",
                                                app_stats.tx.dequeue_pkts);
        printf(" - Ro Pkts transmitted:                 %"PRIu64"\n",
                                                app_stats.tx.ro_tx_pkts);
        printf(" - Ro Pkts tx failed:                   %"PRIu64"\n",
                                                app_stats.tx.ro_tx_failed_pkts);
        printf(" - Pkts transmitted w/o reorder:        %"PRIu64"\n",
                                                app_stats.tx.early_pkts_txtd_woro);
        printf(" - Pkts tx failed w/o reorder:          %"PRIu64"\n",
                                                app_stats.tx.early_pkts_tx_failed_woro);
        for (i = 0; i < nb_ports; i++) {
                rte_eth_stats_get(i, &eth_stats);
                printf("\nPort %u stats:\n", i);
                printf(" - Pkts in:   %"PRIu64"\n", eth_stats.ipackets);
                printf(" - Pkts out:  %"PRIu64"\n", eth_stats.opackets);
                printf(" - In Errs:   %"PRIu64"\n", eth_stats.ierrors);
                printf(" - Out Errs:  %"PRIu64"\n", eth_stats.oerrors);
                printf(" - Mbuf Errs: %"PRIu64"\n", eth_stats.rx_nombuf);
        }
}


static void print_eth_info(uint8_t port_id){
    struct rte_eth_stats stats;
    struct rte_eth_link link;

    rte_eth_link_get_nowait(port_id, &link);
    printf("Link status for port %u:\n", port_id);
    printf("link_speed=%u\tlink_duplex=%u\tlink_autoneg=%u\tlink_status=%u\n",
        link.link_speed, link.link_duplex, link.link_autoneg, link.link_status);

    rte_eth_stats_get(port_id, &stats);
    printf("Stats for port %u:\n", port_id);
    printf("ipackets=%"PRIu64"\topackets=%"PRIu64"\n", stats.ipackets, stats.opackets);
    printf("ibytes=%"PRIu64"\tobytes=%"PRIu64"\n", stats.ibytes, stats.obytes);
    printf("imissed=%"PRIu64"\n", stats.imissed);
    printf("ierrors=%"PRIu64"\toerrors=%"PRIu64"\n", stats.ierrors, stats.oerrors);
    printf("rx_nombuf=%"PRIu64"\n", stats.rx_nombuf);
    int i;

    for(i =0; i < 1; i++){

    printf("q__ipackets=%"PRIu64"\topackets=%"PRIu64"\n", stats.q_ipackets[i], stats.q_opackets[i]);
    printf("q__ibytes=%"PRIu64"\tobytes=%"PRIu64"\n", stats.q_ibytes[i], stats.q_obytes[i]);
    printf("q__errors=%"PRIu64"\n", stats.q_errors[i]);
    printf("\n\n");
    }


    
}


  void ClearScreen()
    {
    int n;
    for (n = 0; n < 10; n++)
      printf( "\n\n\n\n\n\n\n\n\n\n" );
    }

/*Total Usable lcores*/
    static inline
uint32_t total_num_lcores(void)
{
    uint32_t total = 0;
    uint32_t i;
    for (i = 0; i < RTE_MAX_LCORE; i++)
    {
        if ( rte_lcore_is_enabled(i) )
        {
            total += 1;
        }
    }
    return total;
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
    static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    int retval;
    uint16_t q;
    int count;
    if (port >= rte_eth_dev_count())
        return -1;

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port,total_num_lcores() , 1, &port_conf);
    if (retval != 0)
        return retval;

    printf("Rx Ethernet port [%d]\n \
            \t RX_RING_SIZE [%d]\n \
            \t Socket_id [%d]\n",
            port, RX_RING_SIZE, rte_eth_dev_socket_id(port));

    count = 0;
    // TODO (shelbyt): Check retval
    for (q = 0; q < RTE_MAX_CORES; q++) {
        if (rte_lcore_is_enabled(q)) {
            printf(">>Setup RX Core [%d]<<\n",q);
            map_lcore_to_queue[q] = count;
            count++;
            retval = rte_eth_rx_queue_setup(
                    port, //ethernet port
                    map_lcore_to_queue[q],
                    RX_RING_SIZE,
                    rte_eth_dev_socket_id(port), //socket id for pot 0
                    NULL, //TODO(shelbyt): No specific RX config but may help)
                   mbuf_pool);
        }
    }

    printf(">>Setup TX Core<<\n");
    // TODO (shelbyt): Check retval
    retval = rte_eth_tx_queue_setup(
            port, //ethernet port
            0,//queue id
            TX_RING_SIZE,
            rte_eth_dev_socket_id(port),
            NULL);

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    /* Display the port MAC address. */
    struct ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
            " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
            (unsigned)port,
            addr.addr_bytes[0], addr.addr_bytes[1],
            addr.addr_bytes[2], addr.addr_bytes[3],
            addr.addr_bytes[4], addr.addr_bytes[5]);

    /* Enable RX in promiscuous mode for the Ethernet device. */
    rte_eth_promiscuous_enable(port);

    return 0;
}

    static __attribute__((unused)) int
slave_bmain(__attribute__((unused)) void *arg)
{
    time_t endwait;
    time_t start = time(NULL);
    time_t seconds = 30; // end loop after this time has elapsed

    endwait = start + seconds;

    printf("start time is : %s", ctime(&start));


    const uint8_t nb_ports = rte_eth_dev_count();
    uint8_t port;
    int queue_id = map_lcore_to_queue[rte_lcore_id()];

    printf("Slave: Core [%d], Queue[%d]\n",rte_lcore_id(), queue_id );

    /*
     * Check that the port is on the same NUMA node as the polling thread
     * for best performance.
     */
    for (port = 0; port < nb_ports; port++)
        if (rte_eth_dev_socket_id(port) > 0 &&
                rte_eth_dev_socket_id(port) !=
                (int)rte_socket_id())
            printf("WARNING, port %u is on remote NUMA node to "
                    "polling thread.\n\tPerformance will "
                    "not be optimal.\n", port);

    printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
            rte_lcore_id());

    uint64_t total_packets  = 0;
    uint64_t real_time_cyc = 0;
    uint64_t r_int_array[MEM_ACESS_PER_BURST];
    /*Count a number of packets and measure time once completed*/

#if 0
    while(start < endwait) {
#endif
#if 0
    while (total_packets < RX_NUM_PACKETS) {
#endif

#if 1
        int memory_array_index = 0;
        int stride_size = 1024*1024*1;
        int mem_size = 1024*1024*1000;
        int i = 0;
        char print_array_val_0;
        char print_array_val_1;
        char print_array_val_2;
        char print_array_val_3;

        while(1){


            if(keepRunning == 0){
                break;
            }
#endif
        /*Initialize an array with random values that will be accessed
         * in the random access array. Two different rand() calls are used
         * to ensure that the values are sufficiently far away so the mem
         * chunks are not cached*/
#if 0
        int i;
        for(i=0; i < MEM_ACESS_PER_BURST-1; i=i+2){
            r_int_array[i] = (rand() % 400000000);
            r_int_array[i+1] = (rand() % 40000);
        } 
#endif
        //printf("rand() = %d\n", rand() % 400000000);
        //printf("rand() = %d\n", rand() % 40000);

        //uint64_t in_start  = rte_get_tsc_cycles();
        /* The current device the link is plugged into PORT_ID*/

        struct rte_mbuf *bufs[BURST_SIZE];
        const uint16_t nb_rx = rte_eth_rx_burst(PORT_ID, queue_id,
                bufs, BURST_SIZE);

        //print_stats();
        //print_eth_info(0);
        //ClearScreen();
        //printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\r");
        //fflush(stdout);
        



        /****Memory Access**********/

#if 1
        for(i = 0; i < nb_rx*15; i++){
        r_mem_chunk[memory_array_index] = 'c';
        memory_array_index = (memory_array_index+stride_size)%mem_size;
        }
#endif

#if 0
        for (i = 0; i < 32; i++) {
        //tmp_mem_chunk = (char*) malloc(1024*1024*1);
        }
#endif

# if 0

        print_array_val_0 = r_mem_chunk[(memory_array_index+stride_size)%mem_size];
        memory_array_index =(memory_array_index + stride_size) %mem_size;

        print_array_val_1 = r_mem_chunk[(memory_array_index+stride_size)%mem_size];
        memory_array_index =(memory_array_index + stride_size) %mem_size;

        print_array_val_2 = r_mem_chunk[(memory_array_index+stride_size)%mem_size];
        memory_array_index =(memory_array_index + stride_size) %mem_size;

        print_array_val_3 = r_mem_chunk[(memory_array_index+stride_size)%mem_size];
        memory_array_index =(memory_array_index + stride_size) %mem_size;

#endif



#if 0
        for(i=0; i < MEM_ACESS_PER_BURST; i++){
            r_mem_chunk[r_int_array[i]] = 'c';
        }
#endif
        /***************************/
        rte_pktmbuf_free_bulk(bufs,nb_rx);
        //uint64_t in_end = rte_get_tsc_cycles();

        //total_packets += nb_rx;
        //real_time_cyc += in_end - in_start;

        //printf("rx return is %d core/queue [%d]\n", nb_rx, rte_lcore_id());

        //if (unlikely(nb_rx == 0))
        //    continue;
        //start = time(NULL);
    }
        real_time_cyc = 0;
        printf("no-delte-me->%p\n", r_mem_chunk);

    float real_time_sec = ((double)(real_time_cyc)/(rte_get_tsc_hz()));
    printf("(Optimistic) Total packets [%" PRIu64 "] in %lf sec: PPS = [%0.lf]\n",
            total_packets, real_time_sec, ((float)(total_packets)/(real_time_sec)));

    /*Store total packet count in array to be totaled at the end*/
    map_lcore_to_mpps[rte_lcore_id()] = ((float)(total_packets)/real_time_sec);
    return 0;
}

void intHandler(int dummy) {
    keepRunning = 0;
    //print_eth_info(0);
}



int main(int argc, char *argv[])
{
    printf("%d\n",PRINTME);
    exit(0);

    //signal(SIGINT, intHandler);

    struct rte_mempool *mbuf_pool;
    unsigned nb_ports;
    uint8_t portid;
    uint32_t id_core;
    srand(time(NULL));

    /* Initialize the Environment Abstraction Layer (EAL). */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    argc -= ret;
    argv += ret;

    // Allocate 1024*1024-> 1mb * 1024 which is larger than cache size
    int bytes = (1024*1024*1024);

    //TODO(shelbyt): Change to rte_malloc currently segfaults using rtemalloc
    r_mem_chunk = (char*) malloc(bytes);

    //Fill array with random shit so we can access later-/ 
    //---------------------------------------------------/
    static const char alphanum[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUIWXYZ"
        "abcdefghipqrstuvwxyz";

    for (int i = 0; i < bytes; ++i) {
        r_mem_chunk[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    r_mem_chunk[bytes] = 0;
    printf("mem chunk random %c\n",r_mem_chunk[rand()%bytes]);


    
    
    
    //---------------------------------------------------/
    
    

    /* Check that there is an even number of ports to send/receive on. */
    nb_ports = rte_eth_dev_count();

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", MAX_MBUFS_PER_PORT,
            CACHE_SIZE,DEFAULT_PRIV_SIZE , MBUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize all ports. */
    for (portid = 0; portid < nb_ports; portid++)
        if (port_init(portid, mbuf_pool) != 0)
            rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n",
                    portid);


#if 0
    id_core = rte_get_next_lcore(id_core, 1, 1);
    printf("id_Core ->  %d \n", id_core);

    printf("Using %d lcores\n", rte_lcore_count());
#endif

    RTE_LCORE_FOREACH_SLAVE(id_core) {
        rte_eal_remote_launch(slave_bmain, NULL, id_core);
    }
    slave_bmain(NULL);
    //rte_eal_remote_launch(slave_main, NULL, 1);

    rte_eal_mp_wait_lcore();


    int i;
    int pps = 0;
    for(i = 0; i < RTE_MAX_CORES; i++) {
        pps+=map_lcore_to_mpps[i];
    }
    printf("\n\tTotal MPPS -> [%d]\n", pps/1000000);
    //print_stats();
    print_eth_info(0);

    return 0;
}

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
#include <rte_ip.h>

#define MEM_ACCESS_PER_PACKET 32
#define MEM_ACCESS_PER_PACKET 32

#define MEM_ACCESS_PER_PACKET 32

#define RX_RING_SIZE 256
#define TX_RING_SIZE 512

#define RX_NUM_PACKETS 10000000


#define NUM_MBUFS (256*1024)//262144// 
#define MBUF_CACHE_SIZE 256

#define PORT_ID 0
#define QUEUE_PER_CORE 5
#define PKRQ_HWQ_IN_BURST 32
#define BURST_SIZE PKRQ_HWQ_IN_BURST
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


    printf("ipackets=%"PRIu64"\n", stats.ipackets);
    printf("ibytes=%"PRIu64"\n", stats.ibytes);
    //printf("ipackets=%"PRIu64"\topackets=%"PRIu64"\n", stats.ipackets, stats.opackets);
    //printf("ibytes=%"PRIu64"\tobytes=%"PRIu64"\n", stats.ibytes, stats.obytes);
    printf("imissed=%"PRIu64"\n", stats.imissed);
    printf("ierrors=%"PRIu64"\toerrors=%"PRIu64"\n", stats.ierrors, stats.oerrors);
    printf("rx_nombuf=%"PRIu64"\n", stats.rx_nombuf);
    int i;

    for(i =0; i < 10; i++){

        printf("q__ipackets=%"PRIu64"\topackets=%"PRIu64"\n", stats.q_ipackets[i], stats.q_opackets[i]);
        printf("q__ibytes=%"PRIu64"\tobytes=%"PRIu64"\n", stats.q_ibytes[i], stats.q_obytes[i]);
        printf("q__errors=%"PRIu64"\n", stats.q_errors[i]);
        printf("\n\n");
    }



}

#if 0
void ClearScreen()
{
    int n;
    for (n = 0; n < 10; n++)
        printf( "\n\n\n\n\n\n\n\n\n\n" );
}
#endif

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

    const uint8_t nb_ports = rte_eth_dev_count();
    uint8_t port;
    struct rte_mbuf *pkts_burst[BURST_SIZE];
    struct rte_mbuf  *mb;
    struct ether_hdr *eth_hdr;
    struct ether_addr addr;

    uint16_t nb_rx;
    uint16_t nb_tx;
    uint64_t ol_flags=0;
    int i = 0;

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

    while(1){
        /*
         * Receive a burst of packets and forward them.
         */
        nb_rx = rte_eth_rx_burst(PORT_ID, queue_id, pkts_burst,
                BURST_SIZE);
        if (unlikely(nb_rx == 0))
            return;

        total_packets += nb_rx;
        for (i = 0; i < nb_rx; i++) {
            if (likely(i < nb_rx - 1))
                rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[i + 1],
                            void *));
            mb = pkts_burst[i];
            eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr *);

            /* Swap dest and src mac addresses. */
            ether_addr_copy(&eth_hdr->d_addr, &addr);
            ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
            ether_addr_copy(&addr, &eth_hdr->s_addr);

            mb->ol_flags = ol_flags;
            mb->l2_len = sizeof(struct ether_hdr);
            mb->l3_len = sizeof(struct ipv4_hdr);
            //mb->vlan_tci = txp->tx_vlan_id;
            //mb->vlan_tci_outer = txp->tx_vlan_id_outer;
        }
        nb_tx = rte_eth_tx_burst(PORT_ID, queue_id, pkts_burst, nb_rx);
#if 0
        /*
         * Retry if necessary
         */
        if (unlikely(nb_tx < nb_rx) && fs->retry_enabled) {
            retry = 0;
            while (nb_tx < nb_rx && retry++ < burst_tx_retry_num) {
                rte_delay_us(burst_tx_delay_time);
                nb_tx += rte_eth_tx_burst(fs->tx_port, fs->tx_queue,
                        &pkts_burst[nb_tx], nb_rx - nb_tx);
            }
        }
        fs->tx_packets += nb_tx;

        if (unlikely(nb_tx < nb_rx)) {
            fs->fwd_dropped += (nb_rx - nb_tx);
            do {
                rte_pktmbuf_free(pkts_burst[nb_tx]);
            } while (++nb_tx < nb_rx);
        }
#endif
    rte_pktmbuf_free_bulk(pkts_burst,nb_rx);
    //rte_pktmbuf_free(pkts_burst[nb_tx]);
    }




return 0;

}



void intHandler(int dummy) {
    keepRunning = 0;
    //print_eth_info(0);
}



int main(int argc, char *argv[])
{


    struct rte_mempool *mbuf_pool;
    unsigned nb_ports;
    uint8_t portid;
    uint32_t id_core;

    /* Initialize the Environment Abstraction Layer (EAL). */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    argc -= ret;
    argv += ret;


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

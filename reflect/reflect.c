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

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define RX_RING_SIZE 256
#define TX_RING_SIZE 512

#define NUM_MBUFS (256*1024)//65534// 
#define MBUF_CACHE_SIZE 256
#define BURST_SIZE 32

#define QUEUE_PER_CORE 5
#define TOTAL_QUEUES QUEUE_PER_CORE * rte_lcore_count()

//We will iterate over 12 cores to find which are active
#define RTE_MAX_CORES 12 

#if 1
uint32_t map_lcore_to_queue[RTE_MAX_CORES];

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
#endif


/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
    static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
#if 0
    const uint16_t rx_rings = 1, tx_rings = 1;
#endif

    const uint16_t rx_rings = rte_lcore_count(), tx_rings = rte_lcore_count();
    int retval;
    uint16_t q;
    uint16_t lcore_id;
    int count;
    if (port >= rte_eth_dev_count())
        return -1;

    printf("config start\n");
    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    printf("Rx Ethernet port [%d]\n \
            \t RX_RING_SIZE [%d]\n \
            \t Socket_id [%d]\n",
            port, RX_RING_SIZE, rte_eth_dev_socket_id(port));

    printf("config end\n");

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
    struct slave_args *s_args = (struct slave_args*)arg;
    int queue_id = map_lcore_to_queue[rte_lcore_id()];
    printf("Slave: Core [%d], Queue[%d]\n",rte_lcore_id(), queue_id );


    printf("**NB_PORTS is %d\n*", rte_eth_dev_count());

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

    /* Run until the application is quit or killed. */
    for (;;) {


        /* Get burst of RX packets, from first port of pair. */
        struct rte_mbuf *bufs[BURST_SIZE];
        const uint16_t nb_rx = rte_eth_rx_burst(port ^ 1, queue_id,
                bufs, BURST_SIZE);
        //printf("rx return is %d core/queue [%d]\n", nb_rx, rte_lcore_id());


        if (unlikely(nb_rx == 0))
            continue;

        /* Send burst of TX packets, to second port of pair. */
        const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, queue_id,
                bufs, nb_rx);

        /* Free any unsent packets. */
        if (unlikely(nb_tx < nb_rx)) {
            uint16_t buf;
            for (buf = nb_tx; buf < nb_rx; buf++)
                rte_pktmbuf_free(bufs[buf]);
        }
    }
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;
    unsigned nb_ports;
    uint8_t portid;
    uint32_t id_core;
    struct rte_eth_dev_info dev_info;


    /* Initialize the Environment Abstraction Layer (EAL). */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    argc -= ret;
    argv += ret;

    /* Check that there is an even number of ports to send/receive on. */
    nb_ports = rte_eth_dev_count();

    printf("\nPorts are  %d\n", rte_eth_dev_count());

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
            MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

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
    //lcore_main();

    return 0;
}

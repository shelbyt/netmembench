#ifndef __DPDK_CUSTOM_UTILS_H_
#define __DPDK_CUSTOM_UTILS_H_

#include <rte_ethdev.h>
#include <rte_errno.h>
#include <inttypes.h>
#include <time.h>

#define bswap_64(x) \
({ \
    uint64_t __x = (x); \
    ((uint64_t)( \
        (uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000000000ffULL) << 56) | \
        (uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
        (uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
        (uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
        (uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
        (uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
        (uint64_t)(((uint64_t)(__x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
        (uint64_t)(((uint64_t)(__x) & (uint64_t)0xff00000000000000ULL) >> 56) )); \
})

#undef htonll
#undef ntohll

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define htonll(x) bswap_64(x)
    #define ntohll(x) bswap_64(x)
#else
    #define htonll(x) (x)
    #define ntohll(x) (x)
#endif

static inline __attribute__((always_inline))
uint64_t get_time_ns(void)
{
    struct timespec tp;

    if (unlikely(clock_gettime(CLOCK_REALTIME, &tp) != 0))
        return -1;

    return (1000000000) * (uint64_t)tp.tv_sec + tp.tv_nsec;
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
}

static void print_rss_conf(uint8_t port_id){
    int ret, i;
    const uint8_t rss_key_max_len = 100;
    uint8_t rss_key[rss_key_max_len];
    struct rte_eth_rss_conf rss_conf = { .rss_key = rss_key,
                                         .rss_key_len = rss_key_max_len,
                                         .rss_hf = -1, };
    memset(rss_key, 0, rss_key_max_len);

    if((ret = rte_eth_dev_rss_hash_conf_get(port_id, &rss_conf)) < 0){
        fprintf(stderr, "%s: failed to get RSS configuration\n", __func__);
        fprintf(stderr, "%s: %s\n", __func__, rte_strerror(ret));
        return;
    }

    printf("Port %u RSS config:\n", port_id);
    printf("rss_key=");
    for(i = 0; i < rss_conf.rss_key_len; i++)
        printf("%x", rss_conf.rss_key[i]);
    printf("\trss_hf=%"PRIu64"\n", rss_conf.rss_hf);
}

static int set_rss_conf(uint8_t port_id, struct rte_eth_rss_conf *rss_conf){
    int ret;
    if((ret = rte_eth_dev_rss_hash_update(port_id, rss_conf)) < 0){
        fprintf(stderr, "%s: failed to set RSS configuration\n", __func__);
        fprintf(stderr, "%s: %s\n", __func__, rte_strerror(ret));
    }
    return ret;
}

// DPDK 17.08.0 or above only
static int check_queue_sizes(uint8_t port_id, uint16_t nb_tx_queues, uint16_t nb_rx_queues){
    uint16_t test_nb_tx = nb_tx_queues;
    uint16_t test_nb_rx = nb_rx_queues;
    if(rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &test_nb_rx, &test_nb_tx))
        fprintf(stderr, "Failed to check queue sizes!\n");
    return(nb_rx_queues == test_nb_rx && nb_tx_queues == test_nb_tx);
}

#endif

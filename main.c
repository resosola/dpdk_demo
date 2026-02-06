// Copyright 2024 DPDK Demo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_kni.h>
#include <rte_string_fns.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

// Filter rule key structure (matches eBPF version)
struct filter_key {
    uint8_t rule_type;   // 0 = dst_ip+dst_port, 1 = src_ip+src_port
    uint8_t pad[3];
    uint32_t ip;         // IP address (network byte order)
    uint16_t port;       // Port (network byte order)
    uint8_t pad2[2];
};

// Filter rule entry
struct filter_rule_entry {
    struct filter_key key;
    uint8_t action;      // 0 = allow, 1 = forward
    struct filter_rule_entry *next;
};

// Global variables
static volatile bool force_quit = false;
static struct filter_rule_entry *filter_rules = NULL;
static uint16_t port_id = 0;
static struct rte_mempool *mbuf_pool = NULL;
static struct rte_kni *kni = NULL;
static const char *forward_ifname = NULL;
static int tun_fd = -1;
static int raw_sock = -1;

// Statistics
static uint64_t total_packets = 0;
static uint64_t forwarded_packets = 0;
static uint64_t kni_packets = 0;

// Signal handler
static void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nSignal %d received, preparing to exit...\n", signum);
        force_quit = true;
    }
}

// Add filter rule
static int add_filter_rule(uint8_t rule_type, uint32_t ip, uint16_t port)
{
    struct filter_rule_entry *rule = calloc(1, sizeof(*rule));
    if (!rule) {
        fprintf(stderr, "Error: failed to allocate memory for filter rule\n");
        return -1;
    }

    rule->key.rule_type = rule_type;
    rule->key.ip = ip;
    rule->key.port = port;
    rule->action = 1;  // 1 = forward
    rule->next = filter_rules;
    filter_rules = rule;

    struct in_addr addr;
    addr.s_addr = ip;
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));

    if (rule_type == 0) {
        printf("Added filter rule: forward packets to %s:%u\n",
               ip_str, ntohs(port));
    } else {
        printf("Added filter rule: forward packets from %s:%u\n",
               ip_str, ntohs(port));
    }

    return 0;
}

// Check if packet matches filter rule
static bool match_filter_rule(uint32_t src_ip, uint32_t dst_ip,
                               uint16_t src_port, uint16_t dst_port)
{
    struct filter_rule_entry *rule = filter_rules;
    while (rule) {
        if (rule->key.rule_type == 0) {
            // dst_ip + dst_port
            if (rule->key.ip == dst_ip && rule->key.port == dst_port) {
                return true;
            }
        } else {
            // src_ip + src_port
            if (rule->key.ip == src_ip && rule->key.port == src_port) {
                return true;
            }
        }
        rule = rule->next;
    }
    return false;
}

// Open TUN device
static int open_tun_device(const char *tun_name)
{
    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        fprintf(stderr, "Error: cannot open TUN device: %s\n", strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if (tun_name) {
        strncpy(ifr.ifr_name, tun_name, IFNAMSIZ);
    }

    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
        close(fd);
        fprintf(stderr, "Error: ioctl TUNSETIFF failed: %s\n", strerror(errno));
        return err;
    }

    return fd;
}

// Create raw socket for sending packets to physical NIC
static int create_raw_socket(const char *ifname)
{
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        fprintf(stderr, "Error: failed to create raw socket: %s\n", strerror(errno));
        return -1;
    }

    // Bind to interface
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex(ifname);
    if (sll.sll_ifindex == 0) {
        close(sock);
        fprintf(stderr, "Error: failed to get interface index for %s\n", ifname);
        return -1;
    }

    if (bind(sock, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        close(sock);
        fprintf(stderr, "Error: failed to bind raw socket: %s\n", strerror(errno));
        return -1;
    }

    return sock;
}

// Forward packet to TUN device
static int forward_to_tun(struct rte_mbuf *m)
{
    if (tun_fd < 0) {
        return -1;
    }

    // Extract IP packet (skip Ethernet header)
    struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
                                                             sizeof(struct rte_ether_hdr));
    uint16_t ip_len = rte_be_to_cpu_16(ipv4_hdr->total_length);

    // Write IP packet to TUN device
    ssize_t n = write(tun_fd, ipv4_hdr, ip_len);
    if (n < 0) {
        fprintf(stderr, "Error: failed to write to TUN device: %s\n", strerror(errno));
        return -1;
    }

    if (n != ip_len) {
        fprintf(stderr, "Warning: partial write to TUN device (wrote %zd of %u bytes)\n",
                n, ip_len);
    }

    return 0;
}

// Forward packet to physical NIC using raw socket
static int forward_to_nic(struct rte_mbuf *m)
{
    if (raw_sock < 0) {
        return -1;
    }

    // Send entire packet (including Ethernet header)
    void *pkt_data = rte_pktmbuf_mtod(m, void *);
    uint16_t pkt_len = rte_pktmbuf_pkt_len(m);

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_IP);
    sll.sll_ifindex = if_nametoindex(forward_ifname);
    if (sll.sll_ifindex == 0) {
        return -1;
    }

    ssize_t n = sendto(raw_sock, pkt_data, pkt_len, 0,
                       (struct sockaddr *)&sll, sizeof(sll));
    if (n < 0) {
        fprintf(stderr, "Error: failed to send to NIC: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

// KNI request handler - process packets from kernel
static int kni_request_handler(struct rte_kni *kni, __rte_unused void *arg)
{
    struct rte_mbuf *m;
    unsigned int nb_rx;
    unsigned int nb_tx;

    // Receive packets from KNI (packets injected by kernel)
    nb_rx = rte_kni_rx_burst(kni, &m, 1);
    if (nb_rx > 0) {
        // Forward packets back to the NIC (send out)
        nb_tx = rte_eth_tx_burst(port_id, 0, &m, nb_rx);
        if (nb_tx < nb_rx) {
            // Free dropped packets
            unsigned int i;
            for (i = nb_tx; i < nb_rx; i++) {
                rte_pktmbuf_free(&m);
            }
        }
    }

    return 0;
}

// Process packets
static int lcore_main(__rte_unused void *arg)
{
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx;
    uint16_t nb_tx;
    uint16_t i;

    printf("Core %u processing packets.\n", rte_lcore_id());

    while (!force_quit) {
        // Handle KNI requests first
        if (kni) {
            kni_request_handler(kni, NULL);
        }

        // Receive packets from NIC
        nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);
        if (unlikely(nb_rx == 0)) {
            // Small delay to avoid busy waiting
            rte_pause();
            continue;
        }

        total_packets += nb_rx;

        for (i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = bufs[i];
            struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
            struct rte_ipv4_hdr *ipv4_hdr;
            struct rte_tcp_hdr *tcp_hdr = NULL;
            struct rte_udp_hdr *udp_hdr = NULL;
            uint32_t src_ip, dst_ip;
            uint16_t src_port = 0, dst_port = 0;
            uint8_t protocol;
            bool should_forward = false;

            // Check if it's an IPv4 packet
            if (eth_hdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
                // Not IPv4, send to KNI
                if (kni) {
                    nb_tx = rte_kni_tx_burst(kni, &m, 1);
                    if (nb_tx == 0) {
                        rte_pktmbuf_free(m);
                    } else {
                        kni_packets++;
                    }
                } else {
                    rte_pktmbuf_free(m);
                }
                continue;
            }

            // Get IP header
            ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
                                               sizeof(struct rte_ether_hdr));

            // Only process TCP and UDP
            protocol = ipv4_hdr->next_proto_id;
            if (protocol != IPPROTO_TCP && protocol != IPPROTO_UDP) {
                // Not TCP/UDP, send to KNI
                if (kni) {
                    nb_tx = rte_kni_tx_burst(kni, &m, 1);
                    if (nb_tx == 0) {
                        rte_pktmbuf_free(m);
                    } else {
                        kni_packets++;
                    }
                } else {
                    rte_pktmbuf_free(m);
                }
                continue;
            }

            // Extract IP addresses
            src_ip = ipv4_hdr->src_addr;
            dst_ip = ipv4_hdr->dst_addr;

            // Extract ports
            if (protocol == IPPROTO_TCP) {
                tcp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_tcp_hdr *,
                                                 sizeof(struct rte_ether_hdr) +
                                                 (ipv4_hdr->version_ihl & 0x0F) * 4);
                src_port = tcp_hdr->src_port;
                dst_port = tcp_hdr->dst_port;
            } else {
                udp_hdr = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *,
                                                 sizeof(struct rte_ether_hdr) +
                                                 (ipv4_hdr->version_ihl & 0x0F) * 4);
                src_port = udp_hdr->src_port;
                dst_port = udp_hdr->dst_port;
            }

            // Check filter rules
            should_forward = match_filter_rule(src_ip, dst_ip, src_port, dst_port);

            if (should_forward) {
                // Forward packet
                if (forward_ifname && strncmp(forward_ifname, "tun", 3) == 0) {
                    // Forward to TUN device
                    if (forward_to_tun(m) == 0) {
                        forwarded_packets++;
                    }
                } else if (forward_ifname) {
                    // Forward to physical NIC
                    if (forward_to_nic(m) == 0) {
                        forwarded_packets++;
                    }
                }
                rte_pktmbuf_free(m);
            } else {
                // Send to KNI (inject back to kernel stack)
                if (kni) {
                    nb_tx = rte_kni_tx_burst(kni, &m, 1);
                    if (nb_tx == 0) {
                        rte_pktmbuf_free(m);
                    } else {
                        kni_packets++;
                    }
                } else {
                    rte_pktmbuf_free(m);
                }
            }
        }
    }

    return 0;
}

// Print usage
static void print_usage(const char *prog_name)
{
    printf("Usage: %s [EAL OPTIONS] -- [OPTIONS]\n", prog_name);
    printf("\n");
    printf("Options:\n");
    printf("  -i, --interface <port_id>   DPDK port ID (required)\n");
    printf("  -f, --forward <ifname>      Interface to forward filtered packets to (required)\n");
    printf("  -d, --dst <ip:port>         Filter destination IP:port (can be specified multiple times)\n");
    printf("  -s, --src <ip:port>         Filter source IP:port (can be specified multiple times)\n");
    printf("  -k, --kni <ifname>          KNI interface name (optional, for injecting packets to kernel)\n");
    printf("  -h, --help                  Show this help message\n");
    printf("\n");
    printf("EAL Options:\n");
    printf("  --lcores <lcores>           CPU cores to use\n");
    printf("  -m <memory>                 Memory to allocate\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.182.128:7777\n", prog_name);
    printf("  %s -l 0-3 -n 4 -- -i 0 -f ens35 -s 192.168.182.1:8752 -k vEth0\n", prog_name);
}

// Initialize port
static int port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf;
    const uint16_t rx_rings = 1, tx_rings = 1;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    int retval;
    uint16_t q;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    memset(&port_conf, 0, sizeof(struct rte_eth_conf));

    retval = rte_eth_dev_info_get(port, &dev_info);
    if (retval != 0) {
        printf("Error during getting device (port %u) info: %s\n",
               port, strerror(-retval));
        return retval;
    }

    if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
        port_conf.txmode.offloads |=
            RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                                        rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    /* Allocate and set up 1 TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                        rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0)
            return retval;
    }

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    /* Display the port MAC address. */
    struct rte_ether_addr addr;
    retval = rte_eth_macaddr_get(port, &addr);
    if (retval != 0)
        return retval;

    printf("Port %u MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           port,
           addr.addr_bytes[0], addr.addr_bytes[1],
           addr.addr_bytes[2], addr.addr_bytes[3],
           addr.addr_bytes[4], addr.addr_bytes[5]);

    /* Enable RX in promiscuous mode for the Ethernet device. */
    retval = rte_eth_promiscuous_enable(port);
    if (retval != 0)
        return retval;

    return 0;
}

// Initialize KNI
static int init_kni(uint16_t port_id, const char *kni_name)
{
    struct rte_kni_conf conf;
    struct rte_kni_ops ops;
    struct rte_eth_dev_info dev_info;

    memset(&conf, 0, sizeof(conf));
    memset(&ops, 0, sizeof(ops));

    rte_eth_dev_info_get(port_id, &dev_info);

    strncpy(conf.name, kni_name, RTE_KNI_NAMESIZE - 1);
    conf.name[RTE_KNI_NAMESIZE - 1] = '\0';
    conf.force_bind = 1;
    conf.group_id = port_id;
    conf.mbuf_size = RTE_MBUF_DEFAULT_BUF_SIZE;

    ops.port_id = port_id;
    ops.change_mtu = NULL;
    ops.config_network_if = NULL;
    ops.config_mac_address = NULL;
    ops.config_promiscusity = NULL;

    kni = rte_kni_alloc(mbuf_pool, &conf, &ops);
    if (!kni) {
        fprintf(stderr, "Error: failed to create KNI interface\n");
        return -1;
    }

    printf("Created KNI interface: %s\n", kni_name);
    return 0;
}

int main(int argc, char **argv)
{
    int ret, nb_ports;
    unsigned lcore_id;
    const char *kni_name = NULL;
    #define MAX_FILTER_RULES 64
    struct {
        uint8_t rule_type;
        uint32_t ip;
        uint16_t port;
    } filter_rules_args[MAX_FILTER_RULES];
    int filter_rule_count = 0;

    /* Initialize EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_panic("Cannot init EAL\n");

    argc -= ret;
    argv += ret;

    /* Parse application arguments */
    static const struct option long_options[] = {
        {"interface", required_argument, NULL, 'i'},
        {"forward", required_argument, NULL, 'f'},
        {"dst", required_argument, NULL, 'd'},
        {"src", required_argument, NULL, 's'},
        {"kni", required_argument, NULL, 'k'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:f:d:s:k:h", long_options, NULL)) != -1) {
        switch (opt) {
        case 'i':
            port_id = (uint16_t)atoi(optarg);
            break;
        case 'f':
            forward_ifname = optarg;
            break;
        case 'd': {
            // Parse dst_ip:port format
            if (filter_rule_count >= MAX_FILTER_RULES) {
                fprintf(stderr, "Error: maximum %d rules allowed\n", MAX_FILTER_RULES);
                return 1;
            }
            char *colon = strchr(optarg, ':');
            if (!colon) {
                fprintf(stderr, "Error: invalid format for -d, expected IP:port\n");
                return 1;
            }
            *colon = '\0';
            struct in_addr addr;
            if (inet_pton(AF_INET, optarg, &addr) != 1) {
                fprintf(stderr, "Error: invalid IP address: %s\n", optarg);
                return 1;
            }
            int port_val = atoi(colon + 1);
            if (port_val <= 0 || port_val > 65535) {
                fprintf(stderr, "Error: invalid port number: %s\n", colon + 1);
                return 1;
            }
            filter_rules_args[filter_rule_count].rule_type = 0;  // dst_ip+dst_port
            filter_rules_args[filter_rule_count].ip = addr.s_addr;
            filter_rules_args[filter_rule_count].port = htons((uint16_t)port_val);
            filter_rule_count++;
            break;
        }
        case 's': {
            // Parse src_ip:port format
            if (filter_rule_count >= MAX_FILTER_RULES) {
                fprintf(stderr, "Error: maximum %d rules allowed\n", MAX_FILTER_RULES);
                return 1;
            }
            char *colon = strchr(optarg, ':');
            if (!colon) {
                fprintf(stderr, "Error: invalid format for -s, expected IP:port\n");
                return 1;
            }
            *colon = '\0';
            struct in_addr addr;
            if (inet_pton(AF_INET, optarg, &addr) != 1) {
                fprintf(stderr, "Error: invalid IP address: %s\n", optarg);
                return 1;
            }
            int port_val = atoi(colon + 1);
            if (port_val <= 0 || port_val > 65535) {
                fprintf(stderr, "Error: invalid port number: %s\n", colon + 1);
                return 1;
            }
            filter_rules_args[filter_rule_count].rule_type = 1;  // src_ip+src_port
            filter_rules_args[filter_rule_count].ip = addr.s_addr;
            filter_rules_args[filter_rule_count].port = htons((uint16_t)port_val);
            filter_rule_count++;
            break;
        }
        case 'k':
            kni_name = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (forward_ifname == NULL) {
        fprintf(stderr, "Error: forward interface name is required (-f option)\n");
        print_usage(argv[0]);
        return 1;
    }

    if (filter_rule_count == 0) {
        fprintf(stderr, "Error: no filter rules specified. Use -d or -s to specify rules.\n");
        return 1;
    }

    /* Check that there is an even number of ports to send/receive on. */
    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0)
        rte_panic("No Ethernet ports - bye\n");

    if (port_id >= nb_ports) {
        fprintf(stderr, "Error: port ID %u is invalid (available ports: 0-%d)\n",
                port_id, nb_ports - 1);
        return 1;
    }

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());

    if (mbuf_pool == NULL)
        rte_panic("Cannot create mbuf pool\n");

    /* Initialize all ports. */
    if (port_init(port_id, mbuf_pool) != 0)
        rte_panic("Cannot init port %" PRIu16 "\n", port_id);

    /* Initialize KNI if requested */
    if (kni_name) {
        if (init_kni(port_id, kni_name) != 0) {
            fprintf(stderr, "Warning: KNI initialization failed, continuing without KNI\n");
        }
    }

    /* Open forward interface */
    if (strncmp(forward_ifname, "tun", 3) == 0) {
        // TUN device
        tun_fd = open_tun_device(forward_ifname);
        if (tun_fd < 0) {
            fprintf(stderr, "Error: failed to open TUN device %s\n", forward_ifname);
            return 1;
        }
        printf("Opened TUN device %s for forwarding\n", forward_ifname);
    } else {
        // Physical NIC - use raw socket
        raw_sock = create_raw_socket(forward_ifname);
        if (raw_sock < 0) {
            fprintf(stderr, "Error: failed to create raw socket for %s\n", forward_ifname);
            return 1;
        }
        printf("Created raw socket for %s\n", forward_ifname);
    }

    /* Add filter rules */
    for (int i = 0; i < filter_rule_count; i++) {
        add_filter_rule(filter_rules_args[i].rule_type,
                       filter_rules_args[i].ip,
                       filter_rules_args[i].port);
    }

    /* Set signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Launch per-lcore init on every lcore */
    rte_eal_mp_remote_launch(lcore_main, NULL, CALL_MAIN);
    
    /* Run on main core too */
    lcore_main(NULL);
    
    /* Wait for all lcores to finish */
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (rte_eal_wait_lcore(lcore_id) < 0)
            return -1;
    }

    /* Print statistics */
    printf("\nStatistics:\n");
    printf("  Total packets: %" PRIu64 "\n", total_packets);
    printf("  Forwarded packets: %" PRIu64 "\n", forwarded_packets);
    printf("  KNI packets (to kernel): %" PRIu64 "\n", kni_packets);

    /* Cleanup */
    if (kni) {
        rte_kni_release(kni);
    }
    if (tun_fd >= 0) {
        close(tun_fd);
    }
    if (raw_sock >= 0) {
        close(raw_sock);
    }

    /* Clean up the EAL */
    rte_eal_cleanup();

    return 0;
}

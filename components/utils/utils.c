#include "utils.h"

const char *UTILS_TAG = "==> utils";

void print_route_table(void)
{
    struct netif *netif;
    printf("Listing network interfaces...\n");
    for (netif = netif_list; netif != NULL; netif = netif->next)
    {
        printf("------------------------\n");
        printf("IPv4 route table for %s:\n", netif->name);
        printf("ip4: %s\n", ip4addr_ntoa(&netif->ip_addr.u_addr));
        printf("netmask: %s\n", ip4addr_ntoa(&netif->netmask.u_addr));
        printf("gw: %s\n", ip4addr_ntoa(&netif->gw.u_addr));
        printf("ip6 linklocal: %s\n", ip6addr_ntoa(&netif->ip6_addr->u_addr.ip6) );
    }
    printf("------------------------\n");
}

struct pbuf * udp_create_test_packet(u16_t length, u16_t dst_port, u16_t src_port, u32_t dst_addr, u32_t src_addr)
{
    err_t err;
    u8_t ret;
    struct udp_hdr *uh;
    struct ip_hdr *ih;
    struct pbuf *p;
    
    const u8_t test_data[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_POOL);
    if (p == NULL)
    {
        return NULL;
    }
    err = pbuf_take(p, test_data, length);

    /* add UDP header */
    pbuf_add_header(p, sizeof(struct udp_hdr));
    uh = (struct udp_hdr *)p->payload;
    uh->chksum = 0;
    uh->dest = lwip_htons(dst_port);
    uh->src = lwip_htons(src_port);
    uh->len = lwip_htons(p->tot_len);
    /* add IPv4 header */
    pbuf_add_header(p, sizeof(struct ip_hdr));
    ih = (struct ip_hdr *)p->payload;
    memset(ih, 0, sizeof(*ih));
    ih->dest.addr = dst_addr;
    ih->src.addr = src_addr;
    ih->_len = lwip_htons(p->tot_len);
    ih->_ttl = 32;
    ih->_proto = IP_PROTO_UDP;
    IPH_VHL_SET(ih, 4, sizeof(struct ip_hdr) / 4);
    IPH_CHKSUM_SET(ih, inet_chksum(ih, sizeof(struct ip_hdr)));
    return p;
}

struct pbuf * udp_create_test_packet_(u16_t length, u16_t dst_port, u16_t src_port, u32_t dst_addr, u32_t src_addr, u8_t * test_data)
{
    err_t err;
    u8_t ret;
    struct udp_hdr *uh;
    struct ip_hdr *ih;
    struct pbuf *p;
    
    if (test_data==NULL){
        return NULL;
    }

    p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_POOL);
    if (p == NULL)
    {
        return NULL;
    }
    err = pbuf_take(p, test_data, length);

    /* add UDP header */
    pbuf_add_header(p, sizeof(struct udp_hdr));
    uh = (struct udp_hdr *)p->payload;
    uh->chksum = 0;
    uh->dest = lwip_htons(dst_port);
    uh->src = lwip_htons(src_port);
    uh->len = lwip_htons(p->tot_len);
    /* add IPv4 header */
    pbuf_add_header(p, sizeof(struct ip_hdr));
    ih = (struct ip_hdr *)p->payload;
    memset(ih, 0, sizeof(*ih));
    ih->dest.addr = dst_addr;
    ih->src.addr = src_addr;
    ih->_len = lwip_htons(p->tot_len);
    ih->_ttl = 32;
    ih->_proto = IP_PROTO_UDP;
    IPH_VHL_SET(ih, 4, sizeof(struct ip_hdr) / 4);
    IPH_CHKSUM_SET(ih, inet_chksum(ih, sizeof(struct ip_hdr)));
    return p;
}


struct pbuf *udp_create_test_packet_ipv6(u16_t length, u16_t dst_port, u16_t src_port, const ip6_addr_t *dst_addr, const ip6_addr_t *src_addr)
{
    err_t err;
    u8_t ret;
    struct udp_hdr *uh;
    struct ip6_hdr *ih;
    struct pbuf *p;
    const u8_t test_data[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

    p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_POOL);
    if (p == NULL)
    {
        return NULL;
    }
    err = pbuf_take(p, test_data, length);

    /* add UDP header */
    pbuf_add_header(p, sizeof(struct udp_hdr));
    uh = (struct udp_hdr *)p->payload;
    uh->chksum = 0;
    uh->dest = lwip_htons(dst_port);
    uh->src = lwip_htons(src_port);
    uh->len = lwip_htons(p->tot_len);
    /* add IPv6 header */
    pbuf_add_header(p, IP6_HLEN);
    ih = (struct ip6_hdr *)p->payload;
    memset(ih, 0, sizeof(*ih));
    ip6_addr_copy_to_packed(ih->src, *src_addr);
    ip6_addr_copy_to_packed(ih->dest, *dst_addr);
    IP6H_PLEN_SET(ih, (u16_t)(p->tot_len - IP6_HLEN));
    u8_t hl = 256;
    IP6H_HOPLIM_SET(ih, hl);
    IP6H_NEXTH_SET(ih, IP6_NEXTH_UDP);
    IP6H_VTCFL_SET(ih, 6, 8, 0);
    // ip6_debug_print(p);
    return p;
}


struct pbuf *udp_create_test_packet_ipv6_(u16_t length, u16_t dst_port, u16_t src_port, const ip6_addr_t *dst_addr, const ip6_addr_t *src_addr, u8_t test_data)
{
    err_t err;
    u8_t ret;
    struct udp_hdr *uh;
    struct ip6_hdr *ih;
    struct pbuf *p;
    if (test_data==NULL){
        return NULL;
    }
    p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_POOL);
    if (p == NULL)
    {
        return NULL;
    }
    err = pbuf_take(p, test_data, length);

    /* add UDP header */
    pbuf_add_header(p, sizeof(struct udp_hdr));
    uh = (struct udp_hdr *)p->payload;
    uh->chksum = 0;
    uh->dest = lwip_htons(dst_port);
    uh->src = lwip_htons(src_port);
    uh->len = lwip_htons(p->tot_len);
    /* add IPv6 header */
    pbuf_add_header(p, IP6_HLEN);
    ih = (struct ip6_hdr *)p->payload;
    memset(ih, 0, sizeof(*ih));
    ip6_addr_copy_to_packed(ih->src, *src_addr);
    ip6_addr_copy_to_packed(ih->dest, *dst_addr);
    IP6H_PLEN_SET(ih, (u16_t)(p->tot_len - IP6_HLEN));
    u8_t hl = 256;
    IP6H_HOPLIM_SET(ih, hl);
    IP6H_NEXTH_SET(ih, IP6_NEXTH_UDP);
    IP6H_VTCFL_SET(ih, 6, 8, 0);
    // ip6_debug_print(p);
    return p;
}


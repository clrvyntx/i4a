#include "config_portal/configuration_ap_filter.h"

#include "lwip/esp_netif_net_stack.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"

/* Called in the TCP/IP context, so the interface address can be read safely.
 * Unlike the global IPv4 hook, this receives a complete Ethernet frame. */
static err_t configuration_ap_ethernet_input(struct pbuf *p, struct netif *inp)
{
    struct eth_hdr ethernet;
    if (pbuf_copy_partial(p, &ethernet, SIZEOF_ETH_HDR, 0) != SIZEOF_ETH_HDR) {
        goto drop;
    }

    if (ethernet.type == PP_HTONS(ETHTYPE_ARP)) {
        return ethernet_input(p, inp);
    }

    if (ethernet.type == PP_HTONS(ETHTYPE_IP)) {
        struct ip_hdr header;
        if (pbuf_copy_partial(p, &header, IP_HLEN, SIZEOF_ETH_HDR) != IP_HLEN) {
            goto drop;
        }

        ip4_addr_t destination;
        ip4_addr_copy(destination, header.dest);
        /* Local portal traffic and DHCP broadcasts pass to normal lwIP
         * validation. All other destinations are discarded before routing. */
        if (ip4_addr_isbroadcast(&destination, inp) ||
            (netif_is_up(inp) &&
             !ip4_addr_isany_val(*netif_ip4_addr(inp)) &&
             ip4_addr_eq(&destination, netif_ip4_addr(inp)))) {
            return ethernet_input(p, inp);
        }
    }

drop:
    /* Consume silently; ERR_OK prevents the Wi-Fi driver freeing it again. */
    pbuf_free(p);
    return ERR_OK;
}

static err_t configuration_ap_input(struct pbuf *p, struct netif *inp)
{
    /* Preserve tcpip_input's queue/locking and error ownership semantics. */
    return tcpip_inpkt(p, inp, configuration_ap_ethernet_input);
}

err_t configuration_ap_filter_init(struct netif *netif)
{
    err_t err = wlanif_init_ap(netif);
    if (err == ERR_OK) {
        /* netif_add sets input before invoking this initializer. Install here
         * on every AP start, before the interface can receive packets. */
        netif->input = configuration_ap_input;
    }
    return err;
}

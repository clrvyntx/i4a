#include "lwip_custom_hooks.h"


bool get_higher_priority_mask(ip4_addr_t* mask1, ip4_addr_t* mask2)
{
    return ntohl(mask1->addr) > ntohl(mask2->addr)? 1: 0;
}

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest)
{
    struct netif *netif = NULL;
    int n_netif = 0;
    ip4_addr_t* higher_priority_mask=NULL; 
    ip4_addr_t* higher_priority_gateway=NULL; 
    struct netif *higher_priority_netif = NULL;
    if ((dest != NULL) && !ip4_addr_isany(dest)) {
        /* iterate through netifs */
        for (netif = netif_list; netif != NULL; netif = netif->next) {
            /* is the netif up, does it have a link and a valid address? */
            if (ip4_addr_netcmp(dest, netif_ip4_addr(netif), netif_ip4_netmask(netif))) {
                if (netif_is_up(netif) && netif_is_link_up(netif)) {
                    if(n_netif ==0){
                        higher_priority_mask = netif_ip4_netmask(netif);
                        higher_priority_gateway = netif_ip4_gw(netif);
                        higher_priority_netif = netif;
                        n_netif = n_netif +1;

                    }else{
                        if(get_higher_priority_mask(netif_ip4_netmask(netif), higher_priority_mask)){
                            higher_priority_mask = netif_ip4_netmask(netif);
                            higher_priority_gateway = netif_ip4_gw(netif);
                            higher_priority_netif = netif;
                            n_netif = n_netif +1;
                        }
                    }
                }
            }
        }
    }

    for (netif = netif_list; netif != NULL; netif = netif->next) {
        /* is the netif up, does it have a link and a valid address? */
        if (netif_is_up(netif) && netif_is_link_up(netif) && !ip4_addr_isany_val(*netif_ip4_addr(netif))) {
            if (ip4_addr_cmp(netif_ip4_gw(higher_priority_netif), netif_ip4_addr(netif))) {
                return netif;
            }
        }
    }

    /* destination IP is broadcast IP? */
    if ((src != NULL) && !ip4_addr_isany(src)) {
        /* iterate through netifs */
        for (netif = netif_list; netif != NULL; netif = netif->next) {
            /* is the netif up, does it have a link and a valid address? */
            if (netif_is_up(netif) && netif_is_link_up(netif) && !ip4_addr_isany_val(*netif_ip4_addr(netif))) {
                /* source IP matches? */
                if (ip4_addr_cmp(src, netif_ip4_addr(netif))) {
                    /* return netif on which to forward IP packet */
                    return netif;
                }
            }
        }
    }
    return netif;
}

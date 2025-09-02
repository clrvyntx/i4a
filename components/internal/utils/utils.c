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
        printf("ip4: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
        printf("netmask: %s\n", ip4addr_ntoa(netif_ip4_netmask(netif)));
        printf("gw: %s\n", ip4addr_ntoa(netif_ip4_gw(netif)));
    }
    printf("------------------------\n");
}

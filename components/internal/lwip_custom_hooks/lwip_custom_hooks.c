#include "lwip_custom_hooks.h"
#include "esp_log.h"

static const char *TAG = "custom_ip4_route_src_hook";

bool get_higher_priority_mask(ip4_addr_t* mask1, ip4_addr_t* mask2)
{
    return ntohl(mask1->addr) > ntohl(mask2->addr);
}

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest)
{
    struct netif *netif = NULL;
    struct netif *higher_priority_netif = NULL;

    if ((dest != NULL) && !ip4_addr_isany(dest)) {
        ESP_LOGD(TAG, "Evaluando rutas para destino: %s", ip4addr_ntoa(dest));

        for (netif = netif_list; netif != NULL; netif = netif->next) {
            if (!netif_is_up(netif) || !netif_is_link_up(netif)) continue;
            if (!ip4_addr_netcmp(dest, netif_ip4_addr(netif), netif_ip4_netmask(netif))) continue;

            ESP_LOGD(TAG, "Candidato: %s, mascara: %s",
                     ip4addr_ntoa(netif_ip4_addr(netif)),
                     ip4addr_ntoa(netif_ip4_netmask(netif)));

            if (higher_priority_netif == NULL ||
                get_higher_priority_mask(netif_ip4_netmask(netif), netif_ip4_netmask(higher_priority_netif))) {
                ESP_LOGD(TAG, " -> Nueva mejor interfaz encontrada");
                higher_priority_netif = netif;
            }
        }
    }

    if (higher_priority_netif != NULL) {
        ESP_LOGD(TAG, "Buscando interfaz con gateway coincidente: %s",
                 ip4addr_ntoa(netif_ip4_gw(higher_priority_netif)));

        for (netif = netif_list; netif != NULL; netif = netif->next) {
            if (netif_is_up(netif) && netif_is_link_up(netif) &&
                !ip4_addr_isany_val(*netif_ip4_addr(netif)) &&
                ip4_addr_cmp(netif_ip4_gw(higher_priority_netif), netif_ip4_addr(netif))) {
                ESP_LOGD(TAG, "Interfaz seleccionada (por gateway): %s", ip4addr_ntoa(netif_ip4_addr(netif)));
                return netif;
            }
        }
    } else {
        ESP_LOGW(TAG, "No se encontró interfaz con red coincidente.");
    }

    if ((src != NULL) && !ip4_addr_isany(src)) {
        ESP_LOGD(TAG, "Buscando interfaz por IP de origen: %s", ip4addr_ntoa(src));

        for (netif = netif_list; netif != NULL; netif = netif->next) {
            if (netif_is_up(netif) && netif_is_link_up(netif) &&
                !ip4_addr_isany_val(*netif_ip4_addr(netif)) &&
                ip4_addr_cmp(src, netif_ip4_addr(netif))) {
                ESP_LOGD(TAG, "Interfaz seleccionada (por origen): %s", ip4addr_ntoa(netif_ip4_addr(netif)));
                return netif;
            }
        }
    }

    ESP_LOGW(TAG, "No se encontró ninguna interfaz para enrutar el paquete.");
    return NULL;
}

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/esp_netif_net_stack.h"
#include "dhcpserver/dhcpserver.h"

#include "config_portal/config_portal.h"
#include "portal_settings/portal_settings.h"
#include "reset_manager/reset_manager.h"
#include "task_config.h"
#include "wifi_credentials/wifi_credentials.h"
#include "config_portal/configuration_ap_filter.h"
#include "captive_dns.h"

#define BOOT_BUTTON_GPIO GPIO_NUM_0

#define BUTTON_POLL_INTERVAL_MS 50
#define BUTTON_DEBOUNCE_MS 150
#define FACTORY_RESET_HOLD_MS 6000
#define PORTAL_TIMEOUT_MS (5 * 60 * 1000)
#define SESSION_TIMEOUT_MS PORTAL_TIMEOUT_MS
#define RESTART_DELAY_MS 1500
#define RESET_BROADCAST_WAIT_MS 2500

#define CONFIG_PORTAL_BOOT_CONFIGURATION_MAGIC UINT32_C(0x43464743)
#define CONFIG_PORTAL_BOOT_ORIENTATION_MAGIC UINT32_C(0x4346474F)
#define CONFIG_PORTAL_AP_SSID "ComNetAR_Config"
#define CONFIG_PORTAL_AP_CHANNEL 1
#define CONFIG_PORTAL_AP_MAX_CONNECTIONS 4
#define CONFIG_PORTAL_IP "192.168.4.1"

#define LOGIN_MAX_FAILED_ATTEMPTS 5
#define LOGIN_LOCKOUT_MS 30000

#define FORM_BODY_MAX_LENGTH 511
#define COOKIE_HEADER_MAX_LENGTH 255
#define SESSION_TOKEN_BYTES 16
#define SESSION_TOKEN_HEX_LENGTH (SESSION_TOKEN_BYTES * 2)
#define POWER_OPTIONS_HTML_LENGTH 768
#define USER_PORTAL_PAGE_HTML_LENGTH 5000
#define PORTAL_PAGE_HTML_LENGTH 12000

#define SESSION_COOKIE_NAME "i4a_session"

static const char *TAG = "config_portal";

static httpd_handle_t portal_server = NULL;
static TaskHandle_t button_task_handle = NULL;
static esp_netif_t *portal_netif = NULL;
static int64_t portal_started_at_us = 0;
static volatile bool restart_pending = false;
static bool portal_mode_active = false;

RTC_NOINIT_ATTR static uint32_t config_portal_next_boot_marker;

static void config_portal_stop(void);

static char session_token[SESSION_TOKEN_HEX_LENGTH + 1] = {0};
static char csrf_token[SESSION_TOKEN_HEX_LENGTH + 1] = {0};
static char user_csrf_token[SESSION_TOKEN_HEX_LENGTH + 1] = {0};
static int64_t session_expires_at_us = 0;
static uint8_t failed_login_attempts = 0;
static int64_t login_blocked_until_us = 0;

typedef struct {
    uint8_t quarter_dbm;
    const char *label;
} power_option_t;

static const power_option_t POWER_OPTIONS[] = {
    {8, "2 dBm"},
    {20, "5 dBm"},
    {28, "7 dBm"},
    {34, "8 dBm"},
    {44, "11 dBm"},
    {52, "13 dBm"},
    {56, "14 dBm"},
    {60, "15 dBm"},
    {66, "16 dBm"},
    {72, "18 dBm"},
    {80, "20 dBm"},
};

static const char LOGIN_HTML[] =
    "<!doctype html>"
    "<html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR - Acceso administrativo</title>"
    "<style>"
    ":root{color-scheme:light;font-family:system-ui,-apple-system,sans-serif}"
    "body{margin:0;background:#eef3f8;color:#17212b;min-height:100vh;display:grid;place-items:center}"
    "main{box-sizing:border-box;width:min(92vw,430px);background:#fff;padding:28px;border-radius:18px;box-shadow:0 14px 40px #17324d24}"
    "h1{margin:0 0 8px;color:#075985;font-size:1.65rem}p{line-height:1.45;color:#475569}"
    "label{display:block;margin-top:18px;font-weight:650}input{box-sizing:border-box;width:100%;margin-top:7px;padding:12px;border:1px solid #94a3b8;border-radius:9px;font-size:1rem}"
    "button{width:100%;margin-top:24px;padding:13px;border:0;border-radius:9px;background:#0369a1;color:#fff;font-weight:700;font-size:1rem}"
    "small{display:block;margin-top:18px;color:#64748b}"
    "</style></head><body><main>"
    "<h1>Acceso administrativo</h1>"
    "<p>Ingrese la contrasena del portal para administrar este nodo.</p>"
    "<form method=\"post\" action=\"/admin/login\">"
    "<label for=\"admin_password\">Contrasena</label>"
    "<input id=\"admin_password\" name=\"admin_password\" type=\"password\" minlength=\"8\" maxlength=\"63\" required autofocus autocomplete=\"current-password\">"
    "<button type=\"submit\">Ingresar</button>"
    "</form>"
    "<small>El portal se cierra automaticamente despues de cinco minutos.</small>"
    "</main></body></html>";

static const char LOGIN_FAILED_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR</title><style>body{font-family:system-ui;background:#eef3f8;color:#17212b;display:grid;place-items:center;min-height:100vh;margin:0}"
    "main{width:min(88vw,420px);background:white;padding:28px;border-radius:18px;box-shadow:0 14px 40px #17324d24}h1{color:#b91c1c}a{color:#0369a1}</style>"
    "</head><body><main><h1>Acceso denegado</h1>"
    "<p>La contrasena no es valida.</p><p><a href=\"/admin\">Volver a intentar</a></p>"
    "</main></body></html>";

static const char LOGIN_BLOCKED_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR</title><style>body{font-family:system-ui;margin:2rem;color:#17212b}a{color:#0369a1}</style>"
    "</head><body><main><h1>Acceso temporalmente bloqueado</h1>"
    "<p>Se alcanzaron cinco intentos fallidos. Espere 30 segundos antes de volver a intentar.</p>"
    "<p><a href=\"/admin\">Volver</a></p></main></body></html>";

static const char USER_PORTAL_HTML_TEMPLATE[] =
    "<!doctype html>"
    "<html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR - Configurar Wi-Fi</title>"
    "<style>"
    ":root{color-scheme:light;font-family:system-ui,-apple-system,sans-serif}"
    "body{margin:0;background:#eef3f8;color:#17212b;min-height:100vh;display:grid;place-items:center}"
    "main{box-sizing:border-box;width:min(92vw,480px);background:#fff;padding:28px;border-radius:18px;box-shadow:0 14px 40px #17324d24}"
    "h1{margin:0 0 8px;color:#075985;font-size:1.75rem}h2{font-size:1.15rem;margin:28px 0 0}"
    "p{line-height:1.45;color:#475569}label{display:block;margin-top:16px;font-weight:650}"
    "input{box-sizing:border-box;width:100%%;margin-top:7px;padding:12px;border:1px solid #94a3b8;border-radius:9px;font-size:1rem}"
    "button{width:100%%;margin-top:22px;padding:13px;border:0;border-radius:9px;background:#0369a1;color:#fff;font-weight:700;font-size:1rem}"
    "button.danger{background:#b91c1c}.check{display:flex;gap:8px;align-items:center;font-weight:400}.check input{width:auto;margin:0}"
    "hr{border:0;border-top:1px solid #cbd5e1;margin:30px 0 0}small{display:block;margin-top:18px;color:#64748b}"
    "</style></head><body><main>"
    "<h1>Red Wi-Fi ComNetAR</h1>"
    "<p>Desde aqui puede cambiar la contrasena utilizada para conectarse a la red.</p>"
    "<form method=\"post\" action=\"/save\">"
    "<input type=\"hidden\" name=\"csrf\" value=\"%s\">"
    "<label for=\"wifi_password\">Nueva contrasena</label>"
    "<input id=\"wifi_password\" name=\"password\" type=\"password\" minlength=\"8\" maxlength=\"63\" required autocomplete=\"new-password\">"
    "<label for=\"wifi_confirm\">Confirmar contrasena</label>"
    "<input id=\"wifi_confirm\" name=\"confirm\" type=\"password\" minlength=\"8\" maxlength=\"63\" required autocomplete=\"new-password\">"
    "<label class=\"check\"><input id=\"show_wifi\" type=\"checkbox\">Mostrar contrasena</label>"
    "<button type=\"submit\">Guardar y aplicar a la red</button></form>"
    "<p><a href=\"/admin\" style=\"display:block;text-align:center;padding:13px;border-radius:9px;background:#0369a1;color:white;text-decoration:none\">Acceder a configuraci&oacute;n</a></p>"
    "<hr><h2>Restablecer red ComNetAR</h2>"
    "<p>Elimina la contrasena Wi-Fi y deja ComNetAR como una red abierta.</p>"
    "<form method=\"post\" action=\"/factory-reset\" onsubmit=\"return confirm('Confirma el restablecimiento de la red?')\">"
    "<input type=\"hidden\" name=\"csrf\" value=\"%s\"><input type=\"hidden\" name=\"confirm_reset\" value=\"yes\">"
    "<button class=\"danger\" type=\"submit\">Restablecer red</button></form>"
    "<small>Este portal se cierra automaticamente despues de cinco minutos.</small>"
    "<script>document.getElementById('show_wifi').onchange=e=>{const t=e.target.checked?'text':'password';document.getElementById('wifi_password').type=t;document.getElementById('wifi_confirm').type=t};</script>"
    "</main></body></html>";

static const char ADMIN_PORTAL_HTML_TEMPLATE[] =
    "<!doctype html>"
    "<html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR - Administracion</title>"
    "<style>"
    ":root{color-scheme:light;font-family:system-ui,-apple-system,sans-serif}"
    "body{margin:0;background:#eef3f8;color:#17212b;min-height:100vh;padding:28px 0}"
    "main{box-sizing:border-box;width:min(92vw,560px);margin:auto;background:#fff;padding:28px;border-radius:18px;box-shadow:0 14px 40px #17324d24}"
    "h1{margin:0 0 8px;color:#075985;font-size:1.75rem}h2{font-size:1.2rem;margin:26px 0 0}"
    "p{line-height:1.45;color:#475569}label{display:block;margin-top:14px;font-weight:650}"
    "input,select{box-sizing:border-box;width:100%%;margin-top:7px;padding:11px;border:1px solid #94a3b8;border-radius:9px;font-size:1rem;background:#fff}"
    "button{width:100%%;margin-top:20px;padding:12px;border:0;border-radius:9px;background:#0369a1;color:#fff;font-weight:700;font-size:1rem}"
    "button.danger{background:#b91c1c}button.restart{background:#92400e}button.secondary{background:#475569}hr{border:0;border-top:1px solid #cbd5e1;margin:28px 0 0}"
    "small{display:block;margin-top:14px;color:#64748b}.grid{display:grid;grid-template-columns:1fr 1fr;gap:0 16px}"
    ".warning{padding:12px;border-radius:9px;background:#fef3c7;color:#92400e;font-weight:650}"
    ".check{display:flex;gap:8px;align-items:center;font-weight:400}.check input{width:auto;margin:0}"
    "@media(max-width:480px){.grid{grid-template-columns:1fr}}"
    "</style></head><body><main>"
    "<h1>Administracion ComNetAR</h1>"
    "<p>Esta seccion esta reservada para administrar las antenas y el acceso administrativo.</p>"
    "%s"
    "<h2>Potencia de antenas direccionales</h2>"
    "<p>Limita la potencia de transmision durante el funcionamiento normal.</p>"
    "<form method=\"post\" action=\"/admin/antenna-power\">"
    "<input type=\"hidden\" name=\"csrf\" value=\"%s\">"
    "<div class=\"grid\">"
    "<label for=\"power_north\">Norte<select id=\"power_north\" name=\"power_north\">%s</select></label>"
    "<label for=\"power_south\">Sur<select id=\"power_south\" name=\"power_south\">%s</select></label>"
    "<label for=\"power_east\">Este<select id=\"power_east\" name=\"power_east\">%s</select></label>"
    "<label for=\"power_west\">Oeste<select id=\"power_west\" name=\"power_west\">%s</select></label>"
    "</div><button type=\"submit\">Guardar potencias</button></form>"
    "<hr><h2>Limite minimo de RSSI</h2>"
    "<p>Una red con una senal inferior al limite configurado se ignora. Use -128 dBm para aceptar cualquier nivel.</p>"
    "<form method=\"post\" action=\"/admin/antenna-rssi\">"
    "<input type=\"hidden\" name=\"csrf\" value=\"%s\">"
    "<div class=\"grid\">"
    "<label for=\"rssi_north\">Norte (dBm)<input id=\"rssi_north\" name=\"rssi_north\" type=\"number\" min=\"-128\" max=\"-1\" value=\"%d\" required></label>"
    "<label for=\"rssi_south\">Sur (dBm)<input id=\"rssi_south\" name=\"rssi_south\" type=\"number\" min=\"-128\" max=\"-1\" value=\"%d\" required></label>"
    "<label for=\"rssi_east\">Este (dBm)<input id=\"rssi_east\" name=\"rssi_east\" type=\"number\" min=\"-128\" max=\"-1\" value=\"%d\" required></label>"
    "<label for=\"rssi_west\">Oeste (dBm)<input id=\"rssi_west\" name=\"rssi_west\" type=\"number\" min=\"-128\" max=\"-1\" value=\"%d\" required></label>"
    "</div><button type=\"submit\">Guardar limites RSSI</button></form>"
    "<form method=\"post\" action=\"/admin/apply-settings\" onsubmit=\"return confirm('Se reiniciaran los cinco ESP32. Desea continuar?')\">"
    "<input type=\"hidden\" name=\"csrf\" value=\"%s\">"
    "<button class=\"restart\" type=\"submit\">Aplicar configuracion y reiniciar nodo</button></form>"
    "<hr><h2>Orientacion de antenas</h2>"
    "<p>Reinicia los cinco ESP32 en el modo especial de medicion RSSI. No se inicia el ruteo normal.</p>"
    "<form method=\"post\" action=\"/admin/orientation-mode\" onsubmit=\"return confirm('Reiniciar los cinco ESP32 en modo orientacion?')\">"
    "<input type=\"hidden\" name=\"csrf\" value=\"%s\">"
    "<button class=\"restart\" type=\"submit\">Iniciar modo orientacion</button></form>"
    "<hr><h2>Contrasena administrativa</h2>"
    "<p>Al cambiarla se cerrara la sesion actual.</p>"
    "<form method=\"post\" action=\"/admin/password\">"
    "<input type=\"hidden\" name=\"csrf\" value=\"%s\">"
    "<label for=\"current_admin_password\">Contrasena actual</label>"
    "<input id=\"current_admin_password\" name=\"current_password\" type=\"password\" minlength=\"8\" maxlength=\"63\" required autocomplete=\"current-password\">"
    "<label for=\"new_admin_password\">Nueva contrasena</label>"
    "<input id=\"new_admin_password\" name=\"new_password\" type=\"password\" minlength=\"8\" maxlength=\"63\" required autocomplete=\"new-password\">"
    "<label for=\"confirm_admin_password\">Confirmar contrasena</label>"
    "<input id=\"confirm_admin_password\" name=\"confirm_password\" type=\"password\" minlength=\"8\" maxlength=\"63\" required autocomplete=\"new-password\">"
    "<label class=\"check\"><input id=\"show_admin\" type=\"checkbox\">Mostrar contrasenas</label>"
    "<button type=\"submit\">Cambiar contrasena administrativa</button></form>"
    "<form method=\"post\" action=\"/admin/logout\">"
    "<input type=\"hidden\" name=\"csrf\" value=\"%s\">"
    "<button class=\"secondary\" type=\"submit\">Cerrar sesion</button></form>"
    "<small>El portal y la sesion se cierran automaticamente despues de cinco minutos.</small>"
    "<script>"
    "document.getElementById('show_admin').onchange=e=>{const t=e.target.checked?'text':'password';document.getElementById('current_admin_password').type=t;document.getElementById('new_admin_password').type=t;document.getElementById('confirm_admin_password').type=t};"
    "</script></main></body></html>";

static const char WIFI_SUCCESS_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR</title><style>body{font-family:system-ui;background:#eef3f8;color:#17212b;display:grid;place-items:center;min-height:100vh;margin:0}"
    "main{width:min(88vw,420px);background:white;padding:28px;border-radius:18px;box-shadow:0 14px 40px #17324d24}h1{color:#166534}</style>"
    "</head><body><main><h1>Contrasena Wi-Fi guardada</h1>"
    "<p>El nodo se reiniciara en modo normal. Vuelva a conectarse usando la nueva contrasena.</p>"
    "</main></body></html>";

static const char POWER_SUCCESS_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR</title><style>body{font-family:system-ui;margin:2rem;color:#17212b}a{color:#0369a1}</style>"
    "</head><body><main><h1>Potencias guardadas</h1>"
    "<p>Los cuatro valores quedaron almacenados en memoria no volatil. Para aplicarlos, vuelva al portal y use Aplicar configuracion y reiniciar nodo.</p>"
    "<p><a href=\"/admin\">Volver al portal administrativo</a></p></main></body></html>";

static const char RSSI_SUCCESS_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR</title><style>body{font-family:system-ui;margin:2rem;color:#17212b}a{color:#0369a1}</style>"
    "</head><body><main><h1>Limites RSSI guardados</h1>"
    "<p>Los cuatro limites quedaron almacenados en memoria no volatil. Para aplicarlos, vuelva al portal y use Aplicar configuracion y reiniciar nodo.</p>"
    "<p><a href=\"/admin\">Volver al portal administrativo</a></p></main></body></html>";

static const char SETTINGS_RESTART_STARTED_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR</title><style>body{font-family:system-ui;margin:2rem;color:#17212b}</style>"
    "</head><body><main><h1>Configuracion guardada</h1>"
    "<p>El nodo se reiniciara en modo normal y distribuira las potencias y los limites RSSI en el broadcast inicial.</p>"
    "</main></body></html>";

static const char ORIENTATION_RESTART_STARTED_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR</title><style>body{font-family:system-ui;margin:2rem;color:#17212b}</style>"
    "</head><body><main><h1>Modo orientacion programado</h1>"
    "<p>Los cinco ESP32 se reiniciaran. Luego conectese a Orientacion_Antenas.</p>"
    "</main></body></html>";

static const char ADMIN_PASSWORD_SUCCESS_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR</title><style>body{font-family:system-ui;margin:2rem;color:#17212b}a{color:#0369a1}</style>"
    "</head><body><main><h1>Contrasena administrativa guardada</h1>"
    "<p>La sesion anterior fue cerrada. Ingrese nuevamente con la nueva contrasena.</p>"
    "<p><a href=\"/admin\">Iniciar sesion</a></p></main></body></html>";

static const char FACTORY_RESET_SUCCESS_HTML[] =
    "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>ComNetAR</title><style>body{font-family:system-ui;background:#eef3f8;color:#17212b;display:grid;place-items:center;min-height:100vh;margin:0}"
    "main{width:min(88vw,420px);background:white;padding:28px;border-radius:18px;box-shadow:0 14px 40px #17324d24}h1{color:#166534}</style>"
    "</head><body><main><h1>Red restablecida</h1>"
    "<p>El nodo se reiniciara en modo normal con ComNetAR como red abierta. La clave administrativa y los ajustes de antenas se conservaron.</p>"
    "</main></body></html>";

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *cursor = (volatile uint8_t *)buffer;
    while (length-- > 0) {
        *cursor++ = 0;
    }
}

static bool constant_time_token_equal(const char *left, const char *right)
{
    uint8_t difference = 0;
    for (size_t index = 0; index < SESSION_TOKEN_HEX_LENGTH; index++) {
        difference |= (uint8_t)left[index] ^ (uint8_t)right[index];
    }
    return difference == 0;
}

static void set_security_headers(httpd_req_t *request)
{
    httpd_resp_set_hdr(request, "Cache-Control", "no-store, no-cache, must-revalidate");
    httpd_resp_set_hdr(request, "Pragma", "no-cache");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(request, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(
        request, "Content-Security-Policy",
        "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; "
        "form-action 'self'; base-uri 'none'; frame-ancestors 'none'");
}

static esp_err_t send_html(httpd_req_t *request, const char *status,
                           const char *html)
{
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    set_security_headers(request);
    return httpd_resp_send(request, html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_redirect(httpd_req_t *request, const char *location)
{
    httpd_resp_set_status(request, "303 See Other");
    httpd_resp_set_hdr(request, "Location", location);
    set_security_headers(request);
    return httpd_resp_send(request, NULL, 0);
}

static esp_err_t send_error_page_to(httpd_req_t *request, const char *status,
                                    const char *message,
                                    const char *return_path)
{
    char response[768];
    int length = snprintf(
        response, sizeof(response),
        "<!doctype html><html lang=\"es\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>ComNetAR</title></head><body style=\"font-family:system-ui;margin:2rem\">"
        "<h1>No se pudo completar</h1><p>%s</p><p><a href=\"%s\">Volver</a></p>"
        "</body></html>", message, return_path);

    if (length < 0 || length >= (int)sizeof(response)) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Unable to build response");
    }

    return send_html(request, status, response);
}

static esp_err_t send_user_error_page(httpd_req_t *request,
                                      const char *status,
                                      const char *message)
{
    return send_error_page_to(request, status, message, "/");
}

static esp_err_t send_admin_error_page(httpd_req_t *request,
                                       const char *status,
                                       const char *message)
{
    return send_error_page_to(request, status, message, "/admin");
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool url_decode(const char *input, size_t input_length,
                       char *output, size_t output_size)
{
    size_t output_length = 0;

    for (size_t index = 0; index < input_length; index++) {
        unsigned char decoded;

        if (input[index] == '+') {
            decoded = ' ';
        } else if (input[index] == '%') {
            if (index + 2 >= input_length) {
                return false;
            }

            int high = hex_value(input[index + 1]);
            int low = hex_value(input[index + 2]);
            if (high < 0 || low < 0) {
                return false;
            }

            decoded = (unsigned char)((high << 4) | low);
            index += 2;
        } else {
            decoded = (unsigned char)input[index];
        }

        if (decoded == '\0' || output_length + 1 >= output_size) {
            return false;
        }

        output[output_length++] = (char)decoded;
    }

    output[output_length] = '\0';
    return true;
}

static bool form_get_value(const char *body, const char *field_name,
                           char *output, size_t output_size)
{
    size_t field_name_length = strlen(field_name);
    const char *field = body;

    while (*field != '\0') {
        const char *separator = strchr(field, '&');
        size_t pair_length = separator == NULL
            ? strlen(field)
            : (size_t)(separator - field);
        const char *equals = memchr(field, '=', pair_length);

        if (equals != NULL &&
            (size_t)(equals - field) == field_name_length &&
            memcmp(field, field_name, field_name_length) == 0) {
            const char *encoded_value = equals + 1;
            size_t encoded_length = pair_length - (size_t)(encoded_value - field);
            return url_decode(encoded_value, encoded_length, output, output_size);
        }

        if (separator == NULL) {
            break;
        }
        field = separator + 1;
    }

    return false;
}

static esp_err_t receive_form_body(httpd_req_t *request, char *body,
                                   size_t body_size)
{
    if (request->content_len <= 0 ||
        request->content_len >= body_size ||
        request->content_len > FORM_BODY_MAX_LENGTH) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t total_received = 0;
    while (total_received < request->content_len) {
        int received = httpd_req_recv(request, body + total_received,
                                      request->content_len - total_received);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            return ESP_FAIL;
        }
        total_received += (size_t)received;
    }
    body[total_received] = '\0';
    return ESP_OK;
}

static void generate_hex_token(char output[SESSION_TOKEN_HEX_LENGTH + 1])
{
    static const char HEX[] = "0123456789abcdef";
    uint8_t random_bytes[SESSION_TOKEN_BYTES];
    esp_fill_random(random_bytes, sizeof(random_bytes));

    for (size_t index = 0; index < sizeof(random_bytes); index++) {
        output[index * 2] = HEX[random_bytes[index] >> 4];
        output[index * 2 + 1] = HEX[random_bytes[index] & 0x0f];
    }
    output[SESSION_TOKEN_HEX_LENGTH] = '\0';
    secure_zero(random_bytes, sizeof(random_bytes));
}

static void invalidate_session(void)
{
    secure_zero(session_token, sizeof(session_token));
    secure_zero(csrf_token, sizeof(csrf_token));
    session_expires_at_us = 0;
}

static void create_session(void)
{
    generate_hex_token(session_token);
    generate_hex_token(csrf_token);
    session_expires_at_us = esp_timer_get_time() +
        (int64_t)SESSION_TIMEOUT_MS * 1000;
}

static bool get_session_cookie(httpd_req_t *request,
                               char output[SESSION_TOKEN_HEX_LENGTH + 1])
{
    size_t cookie_length = httpd_req_get_hdr_value_len(request, "Cookie");
    if (cookie_length == 0 || cookie_length > COOKIE_HEADER_MAX_LENGTH) {
        return false;
    }

    char cookie[COOKIE_HEADER_MAX_LENGTH + 1];
    if (httpd_req_get_hdr_value_str(request, "Cookie", cookie,
                                     sizeof(cookie)) != ESP_OK) {
        return false;
    }

    const char *cursor = cookie;
    const size_t name_length = strlen(SESSION_COOKIE_NAME);
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == ';') {
            cursor++;
        }

        const char *equals = strchr(cursor, '=');
        if (equals == NULL) {
            break;
        }
        const char *separator = strchr(equals + 1, ';');
        size_t current_name_length = (size_t)(equals - cursor);
        size_t value_length = separator == NULL
            ? strlen(equals + 1)
            : (size_t)(separator - (equals + 1));

        if (current_name_length == name_length &&
            memcmp(cursor, SESSION_COOKIE_NAME, name_length) == 0 &&
            value_length == SESSION_TOKEN_HEX_LENGTH) {
            memcpy(output, equals + 1, SESSION_TOKEN_HEX_LENGTH);
            output[SESSION_TOKEN_HEX_LENGTH] = '\0';
            secure_zero(cookie, sizeof(cookie));
            return true;
        }

        if (separator == NULL) {
            break;
        }
        cursor = separator + 1;
    }

    secure_zero(cookie, sizeof(cookie));
    return false;
}

static bool request_is_authenticated(httpd_req_t *request)
{
    int64_t now_us = esp_timer_get_time();
    if (session_expires_at_us == 0 || now_us >= session_expires_at_us) {
        invalidate_session();
        return false;
    }

    char request_token[SESSION_TOKEN_HEX_LENGTH + 1] = {0};
    bool found = get_session_cookie(request, request_token);
    bool authenticated = found &&
        constant_time_token_equal(request_token, session_token);
    secure_zero(request_token, sizeof(request_token));
    return authenticated;
}

static esp_err_t send_authentication_required(httpd_req_t *request)
{
    httpd_resp_set_hdr(
        request, "Set-Cookie",
        SESSION_COOKIE_NAME "=; Path=/admin; Max-Age=0; HttpOnly; SameSite=Strict");
    return send_html(request, "401 Unauthorized", LOGIN_HTML);
}

static bool form_has_valid_csrf(
    const char *body,
    const char expected_token[SESSION_TOKEN_HEX_LENGTH + 1])
{
    char submitted_token[SESSION_TOKEN_HEX_LENGTH + 1] = {0};
    bool found = form_get_value(body, "csrf", submitted_token,
                                sizeof(submitted_token));
    bool valid = found &&
        constant_time_token_equal(submitted_token, expected_token);
    secure_zero(submitted_token, sizeof(submitted_token));
    return valid;
}

static esp_err_t send_user_portal_page(httpd_req_t *request)
{
    char *page = malloc(USER_PORTAL_PAGE_HTML_LENGTH);
    if (page == NULL) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Unable to allocate Wi-Fi portal page");
    }

    int length = snprintf(
        page, USER_PORTAL_PAGE_HTML_LENGTH, USER_PORTAL_HTML_TEMPLATE,
        user_csrf_token, user_csrf_token);
    if (length < 0 || length >= USER_PORTAL_PAGE_HTML_LENGTH) {
        free(page);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Unable to build Wi-Fi portal page");
    }

    esp_err_t err = send_html(request, "200 OK", page);
    free(page);
    return err;
}

static bool build_power_options(char *output, size_t output_size,
                                uint8_t selected_power)
{
    size_t used = 0;
    output[0] = '\0';

    for (size_t index = 0;
         index < sizeof(POWER_OPTIONS) / sizeof(POWER_OPTIONS[0]); index++) {
        int written = snprintf(
            output + used, output_size - used,
            "<option value=\"%u\"%s>%s</option>",
            POWER_OPTIONS[index].quarter_dbm,
            POWER_OPTIONS[index].quarter_dbm == selected_power
                ? " selected"
                : "",
            POWER_OPTIONS[index].label);
        if (written < 0 || (size_t)written >= output_size - used) {
            return false;
        }
        used += (size_t)written;
    }

    return true;
}

static esp_err_t send_admin_portal_page(httpd_req_t *request)
{
    portal_antenna_power_config_t powers =
        portal_settings_get_antenna_powers();
    portal_antenna_rssi_config_t rssi_thresholds =
        portal_settings_get_antenna_rssi_thresholds();
    char options[PORTAL_SETTINGS_ANTENNA_COUNT]
                [POWER_OPTIONS_HTML_LENGTH];

    for (size_t index = 0; index < PORTAL_SETTINGS_ANTENNA_COUNT; index++) {
        if (!build_power_options(options[index], sizeof(options[index]),
                                 powers.quarter_dbm[index])) {
            return httpd_resp_send_err(request,
                                       HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "Unable to build power options");
        }
    }

    const char *initial_password_warning =
        portal_settings_is_using_initial_password()
        ? "<p class=\"warning\">La contrasena administrativa inicial sigue activa. Cambiela antes del uso regular.</p>"
        : "";

    char *page = malloc(PORTAL_PAGE_HTML_LENGTH);
    if (page == NULL) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Unable to allocate portal page");
    }

    int length = snprintf(
        page, PORTAL_PAGE_HTML_LENGTH, ADMIN_PORTAL_HTML_TEMPLATE,
        initial_password_warning,
        csrf_token,
        options[PORTAL_ANTENNA_NORTH],
        options[PORTAL_ANTENNA_SOUTH],
        options[PORTAL_ANTENNA_EAST],
        options[PORTAL_ANTENNA_WEST],
        csrf_token,
        (int)rssi_thresholds.dbm[PORTAL_ANTENNA_NORTH],
        (int)rssi_thresholds.dbm[PORTAL_ANTENNA_SOUTH],
        (int)rssi_thresholds.dbm[PORTAL_ANTENNA_EAST],
        (int)rssi_thresholds.dbm[PORTAL_ANTENNA_WEST],
        csrf_token,
        csrf_token,
        csrf_token,
        csrf_token);

    if (length < 0 || length >= PORTAL_PAGE_HTML_LENGTH) {
        free(page);
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Unable to build portal page");
    }

    esp_err_t err = send_html(request, "200 OK", page);
    free(page);
    return err;
}

static esp_err_t portal_get_handler(httpd_req_t *request)
{
    return send_user_portal_page(request);
}

static esp_err_t portal_admin_get_handler(httpd_req_t *request)
{
    if (!request_is_authenticated(request)) {
        return send_html(request, "200 OK", LOGIN_HTML);
    }
    return send_admin_portal_page(request);
}

static esp_err_t portal_login_handler(httpd_req_t *request)
{
    int64_t now_us = esp_timer_get_time();
    if (now_us < login_blocked_until_us) {
        return send_html(request, "429 Too Many Requests", LOGIN_BLOCKED_HTML);
    }

    char body[FORM_BODY_MAX_LENGTH + 1] = {0};
    if (receive_form_body(request, body, sizeof(body)) != ESP_OK) {
        return send_admin_error_page(
            request, "400 Bad Request",
            "El formulario recibido tiene un tamano invalido.");
    }

    char password[PORTAL_SETTINGS_ADMIN_PASSWORD_MAX_LENGTH + 1] = {0};
    bool parsed = form_get_value(body, "admin_password", password,
                                 sizeof(password));
    bool verified = parsed &&
        portal_settings_verify_admin_password(password);
    secure_zero(password, sizeof(password));
    secure_zero(body, sizeof(body));

    if (!verified) {
        failed_login_attempts++;
        if (failed_login_attempts >= LOGIN_MAX_FAILED_ATTEMPTS) {
            failed_login_attempts = 0;
            login_blocked_until_us = now_us +
                (int64_t)LOGIN_LOCKOUT_MS * 1000;
            ESP_LOGW(TAG, "Administrator login temporarily blocked");
            return send_html(request, "429 Too Many Requests",
                             LOGIN_BLOCKED_HTML);
        }

        ESP_LOGW(TAG, "Invalid administrator login attempt");
        return send_html(request, "401 Unauthorized", LOGIN_FAILED_HTML);
    }

    failed_login_attempts = 0;
    login_blocked_until_us = 0;
    create_session();

    char cookie[128];
    int cookie_length = snprintf(
        cookie, sizeof(cookie),
        SESSION_COOKIE_NAME "=%s; Path=/admin; Max-Age=300; HttpOnly; SameSite=Strict",
        session_token);
    if (cookie_length < 0 || cookie_length >= (int)sizeof(cookie)) {
        invalidate_session();
        return send_admin_error_page(request, "500 Internal Server Error",
                                     "No se pudo iniciar la sesion.");
    }

    httpd_resp_set_hdr(request, "Set-Cookie", cookie);
    ESP_LOGI(TAG, "Administrator session started");
    return send_redirect(request, "/admin");
}

static bool validate_authenticated_form(httpd_req_t *request, char *body,
                                         size_t body_size,
                                         esp_err_t *response_result)
{
    if (!request_is_authenticated(request)) {
        *response_result = send_authentication_required(request);
        return false;
    }
    if (receive_form_body(request, body, body_size) != ESP_OK) {
        *response_result = send_admin_error_page(
            request, "400 Bad Request",
            "El formulario recibido tiene un tamano invalido.");
        return false;
    }
    if (!form_has_valid_csrf(body, csrf_token)) {
        secure_zero(body, body_size);
        *response_result = send_admin_error_page(
            request, "403 Forbidden",
            "La sesion del formulario no es valida.");
        return false;
    }
    return true;
}

static bool validate_user_form(httpd_req_t *request, char *body,
                               size_t body_size,
                               esp_err_t *response_result)
{
    if (receive_form_body(request, body, body_size) != ESP_OK) {
        *response_result = send_user_error_page(
            request, "400 Bad Request",
            "El formulario recibido tiene un tamano invalido.");
        return false;
    }
    if (!form_has_valid_csrf(body, user_csrf_token)) {
        secure_zero(body, body_size);
        *response_result = send_user_error_page(
            request, "403 Forbidden",
            "El formulario ya no es valido. Vuelva a abrir el portal.");
        return false;
    }
    return true;
}

static void delayed_node_restart_task(void *argument)
{
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));

    config_portal_stop();
    bool reset_broadcasted = false;
    for (size_t attempt = 0; attempt < 20 && !reset_broadcasted; attempt++) {
        reset_broadcasted = rm_broadcast_reset();
        if (!reset_broadcasted) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    if (!reset_broadcasted) {
        ESP_LOGW(TAG,
                 "Node reset broadcast failed; central startup will retry synchronization");
    }

    vTaskDelay(pdMS_TO_TICKS(RESET_BROADCAST_WAIT_MS));
    esp_restart();
}

static uint32_t boot_marker_for_mode(config_portal_boot_mode_t mode)
{
    switch (mode) {
        case CONFIG_PORTAL_BOOT_MODE_NORMAL:
            return 0;
        case CONFIG_PORTAL_BOOT_MODE_CONFIGURATION:
            return CONFIG_PORTAL_BOOT_CONFIGURATION_MAGIC;
        case CONFIG_PORTAL_BOOT_MODE_ORIENTATION:
            return CONFIG_PORTAL_BOOT_ORIENTATION_MAGIC;
        default:
            return UINT32_MAX;
    }
}

static esp_err_t schedule_node_restart(config_portal_boot_mode_t next_mode)
{
    if (restart_pending) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t next_boot_marker = boot_marker_for_mode(next_mode);
    if (next_boot_marker == UINT32_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    config_portal_next_boot_marker = next_boot_marker;
    restart_pending = true;
    BaseType_t task_created = xTaskCreate(
        delayed_node_restart_task,
        "portal_restart",
        TASK_CONFIG_PORTAL_APPLY_STACK,
        NULL,
        TASK_CONFIG_PORTAL_PRIORITY,
        NULL);

    if (task_created != pdPASS) {
        restart_pending = false;
        config_portal_next_boot_marker = 0;
        ESP_LOGE(TAG, "Unable to create delayed node restart task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t portal_save_handler(httpd_req_t *request)
{
    char body[FORM_BODY_MAX_LENGTH + 1] = {0};
    esp_err_t response_result = ESP_OK;
    if (!validate_user_form(request, body, sizeof(body), &response_result)) {
        return response_result;
    }

    if (restart_pending) {
        secure_zero(body, sizeof(body));
        return send_user_error_page(
            request, "409 Conflict",
            "Ya hay un reinicio del nodo en curso.");
    }

    char password[WIFI_CREDENTIALS_PASSWORD_MAX_LENGTH + 1] = {0};
    char confirmation[WIFI_CREDENTIALS_PASSWORD_MAX_LENGTH + 1] = {0};
    bool parsed = form_get_value(body, "password", password,
                                 sizeof(password)) &&
                  form_get_value(body, "confirm", confirmation,
                                 sizeof(confirmation));

    if (!parsed || strcmp(password, confirmation) != 0) {
        secure_zero(password, sizeof(password));
        secure_zero(confirmation, sizeof(confirmation));
        secure_zero(body, sizeof(body));
        return send_user_error_page(
            request, "400 Bad Request",
            parsed
            ? "Las contrasenas Wi-Fi no coinciden."
            : "No se recibieron ambas contrasenas Wi-Fi.");
    }

    if (!wifi_credentials_is_valid_password(password)) {
        secure_zero(password, sizeof(password));
        secure_zero(confirmation, sizeof(confirmation));
        secure_zero(body, sizeof(body));
        return send_user_error_page(
            request, "400 Bad Request",
            "La contrasena Wi-Fi debe tener entre 8 y 63 caracteres ASCII imprimibles.");
    }

    esp_err_t err = wifi_credentials_set_house_password(password);
    secure_zero(password, sizeof(password));
    secure_zero(confirmation, sizeof(confirmation));
    secure_zero(body, sizeof(body));

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi password update failed: %s",
                 esp_err_to_name(err));
        return send_user_error_page(
            request, "500 Internal Server Error",
            "La memoria no volatil rechazo el cambio. La clave anterior sigue activa.");
    }

    err = schedule_node_restart(CONFIG_PORTAL_BOOT_MODE_NORMAL);
    if (err != ESP_OK) {
        return send_user_error_page(
            request, "500 Internal Server Error",
            "La clave fue guardada, pero no se pudo programar el reinicio. Reinicie el nodo manualmente.");
    }

    ESP_LOGI(TAG, "Wi-Fi password saved; normal-mode restart scheduled");
    return send_html(request, "200 OK", WIFI_SUCCESS_HTML);
}

static bool form_get_power(const char *body, const char *field_name,
                           uint8_t *power)
{
    char value[4] = {0};
    if (!form_get_value(body, field_name, value, sizeof(value))) {
        return false;
    }

    errno = 0;
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' ||
        parsed < 0 || parsed > UINT8_MAX) {
        return false;
    }

    *power = (uint8_t)parsed;
    return portal_settings_is_valid_power_qdbm(*power);
}

static esp_err_t portal_antenna_power_handler(httpd_req_t *request)
{
    char body[FORM_BODY_MAX_LENGTH + 1] = {0};
    esp_err_t response_result = ESP_OK;
    if (!validate_authenticated_form(request, body, sizeof(body),
                                     &response_result)) {
        return response_result;
    }

    portal_antenna_power_config_t config = {0};
    bool parsed =
        form_get_power(body, "power_north",
                       &config.quarter_dbm[PORTAL_ANTENNA_NORTH]) &&
        form_get_power(body, "power_south",
                       &config.quarter_dbm[PORTAL_ANTENNA_SOUTH]) &&
        form_get_power(body, "power_east",
                       &config.quarter_dbm[PORTAL_ANTENNA_EAST]) &&
        form_get_power(body, "power_west",
                       &config.quarter_dbm[PORTAL_ANTENNA_WEST]);
    secure_zero(body, sizeof(body));

    if (!parsed) {
        return send_admin_error_page(
            request, "400 Bad Request",
            "Una o mas potencias no corresponden a un nivel admitido.");
    }

    esp_err_t err = portal_settings_set_antenna_powers(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Antenna power persistence failed: %s",
                 esp_err_to_name(err));
        return send_admin_error_page(
            request, "500 Internal Server Error",
            "La memoria no volatil rechazo las potencias nuevas.");
    }

    return send_html(request, "200 OK", POWER_SUCCESS_HTML);
}

static bool form_get_rssi_threshold(const char *body, const char *field_name,
                                    int8_t *rssi_threshold_dbm)
{
    char value[6] = {0};
    if (!form_get_value(body, field_name, value, sizeof(value))) {
        return false;
    }

    errno = 0;
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' ||
        !portal_settings_is_valid_rssi_threshold((int)parsed)) {
        return false;
    }

    *rssi_threshold_dbm = (int8_t)parsed;
    return true;
}

static esp_err_t portal_antenna_rssi_handler(httpd_req_t *request)
{
    char body[FORM_BODY_MAX_LENGTH + 1] = {0};
    esp_err_t response_result = ESP_OK;
    if (!validate_authenticated_form(request, body, sizeof(body),
                                     &response_result)) {
        return response_result;
    }

    portal_antenna_rssi_config_t config = {0};
    bool parsed =
        form_get_rssi_threshold(body, "rssi_north",
                                &config.dbm[PORTAL_ANTENNA_NORTH]) &&
        form_get_rssi_threshold(body, "rssi_south",
                                &config.dbm[PORTAL_ANTENNA_SOUTH]) &&
        form_get_rssi_threshold(body, "rssi_east",
                                &config.dbm[PORTAL_ANTENNA_EAST]) &&
        form_get_rssi_threshold(body, "rssi_west",
                                &config.dbm[PORTAL_ANTENNA_WEST]);
    secure_zero(body, sizeof(body));

    if (!parsed) {
        return send_admin_error_page(
            request, "400 Bad Request",
            "Cada limite RSSI debe estar entre -128 y -1 dBm.");
    }

    esp_err_t err = portal_settings_set_antenna_rssi_thresholds(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Antenna RSSI persistence failed: %s",
                 esp_err_to_name(err));
        return send_admin_error_page(
            request, "500 Internal Server Error",
            "La memoria no volatil rechazo los limites RSSI nuevos.");
    }

    return send_html(request, "200 OK", RSSI_SUCCESS_HTML);
}

static esp_err_t portal_apply_settings_handler(httpd_req_t *request)
{
    char body[FORM_BODY_MAX_LENGTH + 1] = {0};
    esp_err_t response_result = ESP_OK;
    if (!validate_authenticated_form(request, body, sizeof(body),
                                     &response_result)) {
        return response_result;
    }
    secure_zero(body, sizeof(body));

    esp_err_t err = schedule_node_restart(CONFIG_PORTAL_BOOT_MODE_NORMAL);
    if (err == ESP_ERR_INVALID_STATE) {
        return send_admin_error_page(
            request, "409 Conflict",
            "Ya hay un reinicio del nodo en curso.");
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to schedule configuration restart: %s",
                 esp_err_to_name(err));
        return send_admin_error_page(
            request, "500 Internal Server Error",
            "No se pudo programar el reinicio del nodo.");
    }

    ESP_LOGW(TAG, "Normal-mode restart scheduled with saved antenna settings");
    return send_html(request, "202 Accepted", SETTINGS_RESTART_STARTED_HTML);
}

static esp_err_t portal_orientation_mode_handler(httpd_req_t *request)
{
    char body[FORM_BODY_MAX_LENGTH + 1] = {0};
    esp_err_t response_result = ESP_OK;
    if (!validate_authenticated_form(request, body, sizeof(body),
                                     &response_result)) {
        return response_result;
    }
    secure_zero(body, sizeof(body));

    esp_err_t err = schedule_node_restart(
        CONFIG_PORTAL_BOOT_MODE_ORIENTATION);
    if (err == ESP_ERR_INVALID_STATE) {
        return send_admin_error_page(
            request, "409 Conflict",
            "Ya hay un reinicio del nodo en curso.");
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to schedule orientation-mode restart: %s",
                 esp_err_to_name(err));
        return send_admin_error_page(
            request, "500 Internal Server Error",
            "No se pudo programar el modo orientacion.");
    }

    ESP_LOGW(TAG, "Orientation-mode restart scheduled from portal");
    return send_html(request, "202 Accepted",
                     ORIENTATION_RESTART_STARTED_HTML);
}

static esp_err_t portal_admin_password_handler(httpd_req_t *request)
{
    char body[FORM_BODY_MAX_LENGTH + 1] = {0};
    esp_err_t response_result = ESP_OK;
    if (!validate_authenticated_form(request, body, sizeof(body),
                                     &response_result)) {
        return response_result;
    }

    char current_password[PORTAL_SETTINGS_ADMIN_PASSWORD_MAX_LENGTH + 1] = {0};
    char new_password[PORTAL_SETTINGS_ADMIN_PASSWORD_MAX_LENGTH + 1] = {0};
    char confirmation[PORTAL_SETTINGS_ADMIN_PASSWORD_MAX_LENGTH + 1] = {0};
    bool parsed = form_get_value(body, "current_password", current_password,
                                 sizeof(current_password)) &&
                  form_get_value(body, "new_password", new_password,
                                 sizeof(new_password)) &&
                  form_get_value(body, "confirm_password", confirmation,
                                 sizeof(confirmation));

    if (!parsed ||
        !portal_settings_verify_admin_password(current_password)) {
        secure_zero(current_password, sizeof(current_password));
        secure_zero(new_password, sizeof(new_password));
        secure_zero(confirmation, sizeof(confirmation));
        secure_zero(body, sizeof(body));
        return send_admin_error_page(
            request, "403 Forbidden",
            "La contrasena administrativa actual no es valida.");
    }

    if (strcmp(new_password, confirmation) != 0 ||
        !portal_settings_is_valid_admin_password(new_password)) {
        bool matches = strcmp(new_password, confirmation) == 0;
        secure_zero(current_password, sizeof(current_password));
        secure_zero(new_password, sizeof(new_password));
        secure_zero(confirmation, sizeof(confirmation));
        secure_zero(body, sizeof(body));
        return send_admin_error_page(
            request, "400 Bad Request",
            matches
            ? "La nueva contrasena debe tener entre 8 y 63 caracteres ASCII imprimibles."
            : "La nueva contrasena y su confirmacion no coinciden.");
    }

    esp_err_t err = portal_settings_set_admin_password(new_password);
    secure_zero(current_password, sizeof(current_password));
    secure_zero(new_password, sizeof(new_password));
    secure_zero(confirmation, sizeof(confirmation));
    secure_zero(body, sizeof(body));

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Administrator password update failed: %s",
                 esp_err_to_name(err));
        return send_admin_error_page(
            request, "500 Internal Server Error",
            "La memoria no volatil rechazo la nueva contrasena administrativa.");
    }

    invalidate_session();
    httpd_resp_set_hdr(
        request, "Set-Cookie",
        SESSION_COOKIE_NAME "=; Path=/admin; Max-Age=0; HttpOnly; SameSite=Strict");
    ESP_LOGI(TAG, "Administrator password changed; session invalidated");
    return send_html(request, "200 OK", ADMIN_PASSWORD_SUCCESS_HTML);
}

static esp_err_t portal_factory_reset_handler(httpd_req_t *request)
{
    char body[FORM_BODY_MAX_LENGTH + 1] = {0};
    esp_err_t response_result = ESP_OK;
    if (!validate_user_form(request, body, sizeof(body), &response_result)) {
        return response_result;
    }

    if (restart_pending) {
        secure_zero(body, sizeof(body));
        return send_user_error_page(
            request, "409 Conflict",
            "Ya hay un reinicio del nodo en curso.");
    }

    char confirmation[4] = {0};
    bool confirmed = form_get_value(body, "confirm_reset", confirmation,
                                    sizeof(confirmation)) &&
                     strcmp(confirmation, "yes") == 0;
    secure_zero(body, sizeof(body));

    if (!confirmed) {
        return send_user_error_page(
            request, "400 Bad Request",
            "La confirmacion del restablecimiento no es valida.");
    }

    esp_err_t err = wifi_credentials_factory_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Portal Wi-Fi reset failed: %s", esp_err_to_name(err));
        return send_user_error_page(
            request, "500 Internal Server Error",
            "La memoria no volatil rechazo el restablecimiento de la red.");
    }

    err = schedule_node_restart(CONFIG_PORTAL_BOOT_MODE_NORMAL);
    if (err != ESP_OK) {
        return send_user_error_page(
            request, "500 Internal Server Error",
            "La configuracion fue borrada, pero no se pudo programar el reinicio. Reinicie el nodo manualmente.");
    }

    ESP_LOGW(TAG, "Wi-Fi factory reset requested; normal-mode restart scheduled");
    return send_html(request, "200 OK", FACTORY_RESET_SUCCESS_HTML);
}

static esp_err_t portal_logout_handler(httpd_req_t *request)
{
    if (!request_is_authenticated(request)) {
        return send_redirect(request, "/admin");
    }

    char body[FORM_BODY_MAX_LENGTH + 1] = {0};
    if (receive_form_body(request, body, sizeof(body)) != ESP_OK ||
        !form_has_valid_csrf(body, csrf_token)) {
        secure_zero(body, sizeof(body));
        return send_admin_error_page(request, "403 Forbidden",
                                     "La sesion del formulario no es valida.");
    }
    secure_zero(body, sizeof(body));

    invalidate_session();
    httpd_resp_set_hdr(
        request, "Set-Cookie",
        SESSION_COOKIE_NAME "=; Path=/admin; Max-Age=0; HttpOnly; SameSite=Strict");
    return send_redirect(request, "/admin");
}

static esp_err_t portal_captive_redirect(httpd_req_t *request)
{
    return send_redirect(request, "http://" CONFIG_PORTAL_IP "/");
}

static esp_err_t register_portal_handlers(void)
{
    const httpd_uri_t handlers[] = {
        {
            .uri = "/",
            .method = HTTP_GET,
            .handler = portal_get_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/admin",
            .method = HTTP_GET,
            .handler = portal_admin_get_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/admin/login",
            .method = HTTP_POST,
            .handler = portal_login_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = portal_save_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/admin/antenna-power",
            .method = HTTP_POST,
            .handler = portal_antenna_power_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/admin/antenna-rssi",
            .method = HTTP_POST,
            .handler = portal_antenna_rssi_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/admin/apply-settings",
            .method = HTTP_POST,
            .handler = portal_apply_settings_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/admin/orientation-mode",
            .method = HTTP_POST,
            .handler = portal_orientation_mode_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/admin/password",
            .method = HTTP_POST,
            .handler = portal_admin_password_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/factory-reset",
            .method = HTTP_POST,
            .handler = portal_factory_reset_handler,
            .user_ctx = NULL,
        },
        {
            .uri = "/admin/logout",
            .method = HTTP_POST,
            .handler = portal_logout_handler,
            .user_ctx = NULL,
        },
    };

    for (size_t index = 0;
         index < sizeof(handlers) / sizeof(handlers[0]); index++) {
        esp_err_t err = httpd_register_uri_handler(portal_server,
                                                   &handlers[index]);
        if (err != ESP_OK) {
            return err;
        }
    }

    /* Last: covers Android, Apple and Windows probes and unknown HTTP paths. */
    const httpd_uri_t captive = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = portal_captive_redirect,
    };
    return httpd_register_uri_handler(portal_server, &captive);
}

static esp_err_t config_portal_start_http(void)
{
    if (portal_server != NULL) {
        return ESP_OK;
    }

    invalidate_session();
    generate_hex_token(user_csrf_token);
    failed_login_attempts = 0;
    login_blocked_until_us = 0;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;
    config.max_uri_handlers = 12;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&portal_server, &config);
    if (err != ESP_OK) {
        portal_server = NULL;
        secure_zero(user_csrf_token, sizeof(user_csrf_token));
        ESP_LOGE(TAG, "Unable to start configuration portal: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = register_portal_handlers();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to register portal handlers: %s",
                 esp_err_to_name(err));
        httpd_stop(portal_server);
        portal_server = NULL;
        secure_zero(user_csrf_token, sizeof(user_csrf_token));
        return err;
    }

    portal_started_at_us = esp_timer_get_time();

    esp_netif_ip_info_t ip_info;
    if (portal_netif != NULL &&
        esp_netif_get_ip_info(portal_netif, &ip_info) == ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi password portal enabled for five minutes at http://" IPSTR "/",
                  IP2STR(&ip_info.ip));
        ESP_LOGW(TAG, "Administrator portal available at http://" IPSTR "/admin",
                 IP2STR(&ip_info.ip));
    } else {
        ESP_LOGW(TAG, "Wi-Fi and administrator portals enabled for five minutes on HTTP port 80");
    }

    return ESP_OK;
}

static void config_portal_stop(void)
{
    captive_dns_stop();
    if (portal_server != NULL) {
        httpd_stop(portal_server);
        portal_server = NULL;
    }

    portal_started_at_us = 0;
    invalidate_session();
    secure_zero(user_csrf_token, sizeof(user_csrf_token));
    failed_login_attempts = 0;
    login_blocked_until_us = 0;
    ESP_LOGI(TAG, "Configuration portal and administrator session disabled");
}

static esp_err_t config_portal_start_access_point(void)
{
    static const esp_netif_netstack_config_t portal_stack = {
        .lwip = {
            .init_fn = configuration_ap_filter_init,
            .input_fn = wlanif_input,
        },
    };
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_WIFI_AP();
    netif_config.stack = &portal_stack;
    portal_netif = esp_netif_new(&netif_config);
    if (portal_netif == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_netif_attach_wifi_ap(portal_netif);
    if (err == ESP_OK) {
        err = esp_wifi_set_default_wifi_ap_handlers();
    }
    if (err != ESP_OK) {
        esp_netif_destroy_default_wifi(portal_netif);
        portal_netif = NULL;
        return err;
    }

    err = esp_netif_dhcps_stop(portal_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        return err;
    }

    esp_netif_ip_info_t ip_info = {
        .ip = {.addr = ESP_IP4TOADDR(192, 168, 4, 1)},
        .gw = {.addr = ESP_IP4TOADDR(192, 168, 4, 1)},
        .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 0)},
    };
    err = esp_netif_set_ip_info(portal_netif, &ip_info);
    if (err == ESP_OK) {
        esp_netif_dns_info_t dns_info = {
            .ip = {.type = IPADDR_TYPE_V4,
                   .u_addr.ip4.addr = ESP_IP4TOADDR(192, 168, 4, 1)},
        };
        dhcps_offer_t dns_offer = OFFER_DNS;
        err = esp_netif_dhcps_option(portal_netif, ESP_NETIF_OP_SET,
                                     ESP_NETIF_DOMAIN_NAME_SERVER,
                                     &dns_offer, sizeof(dns_offer));
        if (err == ESP_OK) {
            err = esp_netif_set_dns_info(portal_netif, ESP_NETIF_DNS_MAIN,
                                         &dns_info);
        }
    }
    if (err == ESP_OK) {
        err = esp_netif_dhcps_start(portal_netif);
    }
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.ap.ssid, CONFIG_PORTAL_AP_SSID,
           sizeof(CONFIG_PORTAL_AP_SSID));
    wifi_config.ap.ssid_len = strlen(CONFIG_PORTAL_AP_SSID);
    wifi_config.ap.channel = CONFIG_PORTAL_AP_CHANNEL;
    wifi_config.ap.max_connection = CONFIG_PORTAL_AP_MAX_CONNECTIONS;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    wifi_config.ap.pmf_cfg.required = false;

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err == ESP_OK) {
        ESP_LOGW(TAG, "Configuration AP started: SSID=%s IP=%s",
                 CONFIG_PORTAL_AP_SSID, CONFIG_PORTAL_IP);
    }
    return err;
}

static void request_configuration_mode(void)
{
    esp_err_t err = schedule_node_restart(
        CONFIG_PORTAL_BOOT_MODE_CONFIGURATION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to schedule configuration-mode restart: %s",
                  esp_err_to_name(err));
    }
}

static void restore_factory_credentials(void)
{
    esp_err_t err = wifi_credentials_factory_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi factory reset failed: %s", esp_err_to_name(err));
        return;
    }

    config_portal_next_boot_marker = 0;
    ESP_LOGW(TAG,
             "Wi-Fi factory reset complete; antenna and administrator settings preserved");
}

static void boot_button_task(void *argument)
{
    (void)argument;

    bool raw_pressed = gpio_get_level(BOOT_BUTTON_GPIO) == 0;
    bool stable_pressed = false;
    bool press_active = false;
    bool factory_reset_handled = false;
    int64_t raw_changed_at_us = esp_timer_get_time();
    int64_t pressed_at_us = 0;

    while (true) {
        int64_t now_us = esp_timer_get_time();
        bool sampled_pressed = gpio_get_level(BOOT_BUTTON_GPIO) == 0;

        if (portal_mode_active && !restart_pending &&
            now_us - portal_started_at_us >=
                (int64_t)PORTAL_TIMEOUT_MS * 1000) {
            ESP_LOGW(TAG,
                     "Configuration portal timeout; restarting in normal mode");
            schedule_node_restart(CONFIG_PORTAL_BOOT_MODE_NORMAL);
        }

        if (sampled_pressed != raw_pressed) {
            raw_pressed = sampled_pressed;
            raw_changed_at_us = now_us;
        }

        if (raw_pressed != stable_pressed &&
            now_us - raw_changed_at_us >=
                (int64_t)BUTTON_DEBOUNCE_MS * 1000) {
            stable_pressed = raw_pressed;

            if (stable_pressed) {
                press_active = true;
                factory_reset_handled = false;
                pressed_at_us = now_us;
            } else if (press_active) {
                if (!factory_reset_handled) {
                    if (portal_mode_active) {
                        ESP_LOGI(TAG,
                                 "Short BOOT press detected; leaving configuration mode");
                        schedule_node_restart(CONFIG_PORTAL_BOOT_MODE_NORMAL);
                    } else {
                        ESP_LOGI(TAG,
                                 "Short BOOT press detected; restarting in configuration mode");
                        request_configuration_mode();
                    }
                } else {
                    ESP_LOGI(TAG,
                             "BOOT released after factory reset; restarting in normal mode");
                    schedule_node_restart(CONFIG_PORTAL_BOOT_MODE_NORMAL);
                }

                press_active = false;
                factory_reset_handled = false;
                pressed_at_us = 0;
            }
        }

        if (stable_pressed && press_active && !factory_reset_handled &&
            now_us - pressed_at_us >=
                (int64_t)FACTORY_RESET_HOLD_MS * 1000) {
            factory_reset_handled = true;
            ESP_LOGW(TAG, "BOOT held for six seconds; resetting Wi-Fi credentials");
            restore_factory_credentials();
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
    }
}

static esp_err_t start_boot_button_task(void)
{
    if (button_task_handle != NULL) {
        return ESP_OK;
    }

    gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << BOOT_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&button_config);
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t task_created = xTaskCreatePinnedToCore(
        boot_button_task,
        "boot_button",
        TASK_CONFIG_PORTAL_STACK,
        NULL,
        TASK_CONFIG_PORTAL_PRIORITY,
        &button_task_handle,
        TASK_CONFIG_PORTAL_CORE);
    if (task_created != pdPASS) {
        button_task_handle = NULL;
        gpio_reset_pin(BOOT_BUTTON_GPIO);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Configuration BOOT-button monitor initialized");
    return ESP_OK;
}

config_portal_boot_mode_t config_portal_take_boot_mode_request(void)
{
    config_portal_boot_mode_t requested_mode =
        CONFIG_PORTAL_BOOT_MODE_NORMAL;
    if (config_portal_next_boot_marker ==
        CONFIG_PORTAL_BOOT_CONFIGURATION_MAGIC) {
        requested_mode = CONFIG_PORTAL_BOOT_MODE_CONFIGURATION;
    } else if (config_portal_next_boot_marker ==
               CONFIG_PORTAL_BOOT_ORIENTATION_MAGIC) {
        requested_mode = CONFIG_PORTAL_BOOT_MODE_ORIENTATION;
    }

    config_portal_next_boot_marker = 0;
    return requested_mode;
}

esp_err_t config_portal_start_button_monitor(void)
{
    portal_mode_active = false;
    restart_pending = false;
    return start_boot_button_task();
}

esp_err_t config_portal_run(void)
{
    portal_mode_active = true;
    restart_pending = false;

    esp_err_t err = portal_settings_init();
    if (err == ESP_OK) {
        err = config_portal_start_access_point();
    }
    if (err == ESP_OK) {
        err = config_portal_start_http();
    }
    if (err == ESP_OK) {
        err = captive_dns_start();
    }
    if (err == ESP_OK) {
        err = start_boot_button_task();
    }
    if (err != ESP_OK) {
        config_portal_stop();
        ESP_LOGE(TAG, "Unable to start configuration mode: %s",
                 esp_err_to_name(err));
    }
    return err;
}

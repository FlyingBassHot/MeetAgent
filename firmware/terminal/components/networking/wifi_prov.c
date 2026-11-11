#include "wifi_prov.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

static const char *TAG = "wifi_prov";

#define NVS_NAMESPACE "meet_wifi"
#define NVS_KEY_SSID "wifi_ssid"
#define NVS_KEY_PASS "wifi_pass"
#define NVS_KEY_NODE_URL "node_url"
#define NVS_KEY_PAIRING "pairing_code"

#define WIFI_CONNECTED_BIT (1u << 0)
#define WIFI_FAIL_BIT (1u << 1)
#define STA_MAX_ATTEMPTS 3
#define STA_TIMEOUT_MS 15000
#define NODE_CHECK_ATTEMPTS 3
#define PORTAL_BODY_MAX 1024

typedef struct {
    char ssid[33];
    char password[65];
    char node_url[128];
    char pairing_code[33];
    bool has_config;
} prov_config_t;

static prov_config_t s_config;
static meet_wifi_status_t s_status;
static SemaphoreHandle_t s_status_lock;
static EventGroupHandle_t s_wifi_events;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_server;
static char s_ap_ssid[33];
static bool s_wifi_started;
static bool s_initialized;

static esp_err_t portal_start(void);

static void copy_str(char *dst, size_t size, const char *src)
{
    if (dst == NULL || size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    size_t length = strlen(src);
    if (length >= size) {
        length = size - 1;
    }
    memcpy(dst, src, length);
    dst[length] = '\0';
}

static void status_take(void)
{
    if (s_status_lock != NULL) {
        (void)xSemaphoreTake(s_status_lock, portMAX_DELAY);
    }
}

static void status_give(void)
{
    if (s_status_lock != NULL) {
        (void)xSemaphoreGive(s_status_lock);
    }
}

static void status_set_state(meet_wifi_state_t state)
{
    status_take();
    s_status.state = state;
    status_give();
}

static void status_set_ip(const char *ip)
{
    status_take();
    copy_str(s_status.ip, sizeof(s_status.ip), ip);
    status_give();
}

static void status_set_config(const char *ssid, const char *node_url, bool has)
{
    status_take();
    copy_str(s_status.ssid, sizeof(s_status.ssid), ssid);
    copy_str(s_status.node_url, sizeof(s_status.node_url), node_url);
    s_status.has_config = has;
    status_give();
}

static void status_set_node(bool reachable, const char *name, const char *version,
                            int protocol_version, int schema_version)
{
    status_take();
    s_status.node_reachable = reachable;
    copy_str(s_status.node_name, sizeof(s_status.node_name), name);
    copy_str(s_status.node_version, sizeof(s_status.node_version), version);
    s_status.protocol_version = protocol_version;
    s_status.schema_version = schema_version;
    status_give();
}

const char *meet_wifi_state_name(meet_wifi_state_t state)
{
    switch (state) {
    case MEET_WIFI_STATE_BOOT:
        return "boot";
    case MEET_WIFI_STATE_AP:
        return "ap";
    case MEET_WIFI_STATE_AP_FALLBACK:
        return "ap_fallback";
    case MEET_WIFI_STATE_CONNECTING:
        return "connecting";
    case MEET_WIFI_STATE_CONNECTED:
        return "connected";
    case MEET_WIFI_STATE_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

static void nvs_read_string(nvs_handle_t handle, const char *key, char *out,
                            size_t size)
{
    if (size == 0) {
        return;
    }
    out[0] = '\0';
    size_t length = size;
    if (nvs_get_str(handle, key, out, &length) != ESP_OK) {
        out[0] = '\0';
    }
    out[size - 1] = '\0';
}

static void load_config(void)
{
    memset(&s_config, 0, sizeof(s_config));
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }
    nvs_read_string(handle, NVS_KEY_SSID, s_config.ssid, sizeof(s_config.ssid));
    nvs_read_string(handle, NVS_KEY_PASS, s_config.password,
                    sizeof(s_config.password));
    nvs_read_string(handle, NVS_KEY_NODE_URL, s_config.node_url,
                    sizeof(s_config.node_url));
    nvs_read_string(handle, NVS_KEY_PAIRING, s_config.pairing_code,
                    sizeof(s_config.pairing_code));
    nvs_close(handle);
    s_config.has_config = s_config.ssid[0] != '\0' &&
                          s_config.node_url[0] != '\0';
}

static esp_err_t save_config(const char *ssid, const char *password,
                             const char *node_url, const char *pairing_code)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(handle, NVS_KEY_SSID, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_PASS, password);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_NODE_URL, node_url);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, NVS_KEY_PAIRING, pairing_code);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err == ESP_OK) {
        copy_str(s_config.ssid, sizeof(s_config.ssid), ssid);
        copy_str(s_config.password, sizeof(s_config.password), password);
        copy_str(s_config.node_url, sizeof(s_config.node_url), node_url);
        copy_str(s_config.pairing_code, sizeof(s_config.pairing_code),
                 pairing_code);
        s_config.has_config = true;
        status_set_config(ssid, node_url, true);
    }
    return err;
}

static void build_ap_ssid(void)
{
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        copy_str(s_ap_ssid, sizeof(s_ap_ssid), "MeetAgent-0000");
        return;
    }
    (void)snprintf(s_ap_ssid, sizeof(s_ap_ssid), "MeetAgent-%02X%02X", mac[4],
                   mac[5]);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "STA started");
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *event =
                (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGW(TAG, "STA disconnected, reason=%d",
                     event != NULL ? event->reason : -1);
            if (s_wifi_events != NULL) {
                (void)xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
            }
        } else if (event_id == WIFI_EVENT_AP_START) {
            ESP_LOGI(TAG, "SoftAP started");
        } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            ESP_LOGI(TAG, "SoftAP client connected");
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            ESP_LOGI(TAG, "SoftAP client disconnected");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip[16] = {0};
        if (event != NULL) {
            (void)esp_ip4addr_ntoa(&event->ip_info.ip, ip, sizeof(ip));
        }
        ESP_LOGI(TAG, "STA got IP: %s", ip);
        status_set_ip(ip);
        if (s_wifi_events != NULL) {
            (void)xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
        }
    }
}

static esp_err_t wifi_core_init(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop failed: %s", esp_err_to_name(err));
        return err;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_sta_netif == NULL || s_ap_netif == NULL) {
        ESP_LOGE(TAG, "failed to create default Wi-Fi netifs");
        return ESP_FAIL;
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_storage failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     &wifi_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     &wifi_event_handler, NULL);
    return err;
}

static bool wifi_connect_sta(void)
{
    if (s_wifi_started) {
        (void)esp_wifi_stop();
        s_wifi_started = false;
    }

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "STA mode failed: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "STA start failed: %s", esp_err_to_name(err));
        return false;
    }
    s_wifi_started = true;
    (void)esp_netif_set_default_netif(s_sta_netif);

    for (int attempt = 1; attempt <= STA_MAX_ATTEMPTS; ++attempt) {
        (void)xEventGroupClearBits(s_wifi_events,
                                   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

        wifi_config_t sta_config;
        memset(&sta_config, 0, sizeof(sta_config));
        copy_str((char *)sta_config.sta.ssid, sizeof(sta_config.sta.ssid),
                 s_config.ssid);
        copy_str((char *)sta_config.sta.password,
                 sizeof(sta_config.sta.password), s_config.password);
        sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

        err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "STA config failed: %s", esp_err_to_name(err));
            return false;
        }
        err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "STA connect attempt %d failed to start: %s", attempt,
                     esp_err_to_name(err));
        }

        EventBits_t bits = xEventGroupWaitBits(
            s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
            pdMS_TO_TICKS(STA_TIMEOUT_MS));
        if ((bits & WIFI_CONNECTED_BIT) != 0) {
            esp_netif_ip_info_t ip_info;
            char ip[16] = {0};
            if (esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK) {
                (void)esp_ip4addr_ntoa(&ip_info.ip, ip, sizeof(ip));
            }
            status_set_ip(ip);
            ESP_LOGI(TAG, "STA connected to '%s', IP=%s (attempt %d)",
                     s_config.ssid, ip, attempt);
            esp_err_t portal_err = portal_start();
            if (portal_err != ESP_OK) {
                ESP_LOGW(TAG, "STA /status portal start failed: %s",
                         esp_err_to_name(portal_err));
            }
            return true;
        }

        ESP_LOGW(TAG, "STA attempt %d/%d failed", attempt, STA_MAX_ATTEMPTS);
        (void)esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
}

static bool fetch_node_info(char *response, size_t response_size)
{
    char base_url[192];
    copy_str(base_url, sizeof(base_url), s_config.node_url);
    size_t length = strlen(base_url);
    while (length > 0 && base_url[length - 1] == '/') {
        base_url[--length] = '\0';
    }

    char url[208];
    (void)snprintf(url, sizeof(url), "%s/api/v2/info", base_url);

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 5000,
        .buffer_size = 1024,
        .buffer_size_tx = 512,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGW(TAG, "Node HTTP client init failed");
        return false;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    int status = 0;
    int read = 0;
    if (err == ESP_OK) {
        (void)esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        if (status == 200) {
            int total = 0;
            while (total < (int)response_size - 1) {
                int received = esp_http_client_read(
                    client, response + total, (int)response_size - 1 - total);
                if (received <= 0) {
                    break;
                }
                total += received;
            }
            read = total;
        }
        (void)esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);

    if (read > 0) {
        response[read] = '\0';
        return true;
    }
    ESP_LOGW(TAG, "Node /api/v2/info failed: err=%s status=%d read=%d",
             esp_err_to_name(err), status, read);
    return false;
}

static void check_node_reachable(void)
{
    char response[1024];
    for (int attempt = 1; attempt <= NODE_CHECK_ATTEMPTS; ++attempt) {
        memset(response, 0, sizeof(response));
        if (fetch_node_info(response, sizeof(response))) {
            cJSON *root = cJSON_Parse(response);
            if (root != NULL) {
                cJSON *node = cJSON_GetObjectItemCaseSensitive(root, "node");
                cJSON *name = cJSON_GetObjectItemCaseSensitive(node, "name");
                cJSON *version =
                    cJSON_GetObjectItemCaseSensitive(node, "version");
                cJSON *protocol =
                    cJSON_GetObjectItemCaseSensitive(root, "protocol_version");
                cJSON *schema =
                    cJSON_GetObjectItemCaseSensitive(root, "schema_version");
                const char *name_str =
                    cJSON_IsString(name) ? name->valuestring : "";
                const char *version_str =
                    cJSON_IsString(version) ? version->valuestring : "";
                int protocol_value =
                    cJSON_IsNumber(protocol) ? protocol->valueint : 0;
                int schema_value =
                    cJSON_IsNumber(schema) ? schema->valueint : 0;
                status_set_node(true, name_str, version_str, protocol_value,
                                schema_value);
                ESP_LOGI(TAG,
                         "Node reachable: name=%s version=%s protocol=%d schema=%d",
                         name_str, version_str, protocol_value, schema_value);
                cJSON_Delete(root);
                return;
            }
            ESP_LOGW(TAG, "Node /api/v2/info returned invalid JSON");
        }
        if (attempt < NODE_CHECK_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
    status_set_node(false, "", "", 0, 0);
    ESP_LOGW(TAG, "Node unreachable after %d attempts; Wi-Fi remains connected",
             NODE_CHECK_ATTEMPTS);
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + c - 'a';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + c - 'A';
    }
    return -1;
}

static bool url_decode(const char *src, size_t length, char *dst,
                       size_t dst_size)
{
    size_t out = 0;
    for (size_t i = 0; i < length; ++i) {
        char c = src[i];
        if (c == '+') {
            c = ' ';
        } else if (c == '%') {
            if (i + 2 >= length) {
                return false;
            }
            int high = hex_value(src[i + 1]);
            int low = hex_value(src[i + 2]);
            if (high < 0 || low < 0) {
                return false;
            }
            c = (char)((high << 4) | low);
            i += 2;
        }
        if (out + 1 >= dst_size) {
            return false;
        }
        dst[out++] = c;
    }
    dst[out] = '\0';
    return true;
}

static bool form_get(const char *body, const char *key, char *out,
                     size_t out_size)
{
    size_t key_length = strlen(key);
    const char *cursor = body;
    if (out_size > 0) {
        out[0] = '\0';
    }
    while (cursor != NULL && *cursor != '\0') {
        const char *amp = strchr(cursor, '&');
        size_t pair_length = amp != NULL ? (size_t)(amp - cursor)
                                         : strlen(cursor);
        if (pair_length > key_length &&
            strncmp(cursor, key, key_length) == 0 &&
            cursor[key_length] == '=') {
            return url_decode(cursor + key_length + 1,
                              pair_length - key_length - 1, out, out_size);
        }
        if (amp == NULL) {
            break;
        }
        cursor = amp + 1;
    }
    return false;
}

static const char PORTAL_HTML[] =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>MeetAgent Setup</title>"
    "<style>body{font-family:system-ui,sans-serif;background:#080c0a;"
    "color:#d9ffc3;max-width:34rem;margin:2rem auto;padding:0 1rem}"
    "h1{color:#a7f36b}label{display:block;margin:1rem 0 .3rem}"
    "input{width:100%;box-sizing:border-box;padding:.6rem;background:#101813;"
    "color:#d9ffc3;border:1px solid #587364;border-radius:4px}"
    "button{margin-top:1.2rem;padding:.7rem 1.2rem;background:#a7f36b;"
    "color:#080c0a;border:0;border-radius:4px;font-weight:700}"
    "p.note{color:#587364;font-size:.9rem}</style></head><body>"
    "<h1>MeetAgent Terminal</h1>"
    "<p>Configure Wi-Fi and the MeetAgent Node.</p>"
    "<form method=\"post\" action=\"/save\">"
    "<label>Wi-Fi SSID<input name=\"ssid\" maxlength=\"32\" required></label>"
    "<label>Wi-Fi Password<input type=\"password\" name=\"password\" "
    "maxlength=\"64\"></label>"
    "<label>Node URL<input name=\"node_url\" maxlength=\"127\" "
    "placeholder=\"http://192.168.x.x:8787\" required></label>"
    "<label>Pairing Code (optional)<input name=\"pairing_code\" "
    "maxlength=\"32\"></label>"
    "<button type=\"submit\">Save &amp; Reboot</button>"
    "</form>"
    "<p class=\"note\">The device AP is open during setup. After saving, the "
    "device reboots and connects to your Wi-Fi. The Node must be on the same "
    "LAN. This first stage uses HTTP only.</p>"
    "</body></html>";

static esp_err_t portal_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t portal_status(httpd_req_t *req)
{
    meet_wifi_status_t status;
    meet_wifi_prov_get_status(&status);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_500(req);
    }
    cJSON_AddStringToObject(root, "wifi_state",
                            meet_wifi_state_name(status.state));
    cJSON_AddStringToObject(root, "ssid", status.ssid);
    cJSON_AddStringToObject(root, "ip", status.ip);
    cJSON_AddStringToObject(root, "node_url", status.node_url);
    cJSON_AddBoolToObject(root, "has_config", status.has_config);
    cJSON_AddBoolToObject(root, "node_reachable", status.node_reachable);
    cJSON_AddStringToObject(root, "node_name", status.node_name);
    cJSON_AddStringToObject(root, "node_version", status.node_version);
    cJSON_AddNumberToObject(root, "protocol_version",
                            status.protocol_version);
    cJSON_AddNumberToObject(root, "schema_version", status.schema_version);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, json);
    cJSON_free(json);
    return err;
}

static esp_err_t portal_save(httpd_req_t *req)
{
    meet_wifi_status_t status;
    meet_wifi_prov_get_status(&status);
    if (status.state != MEET_WIFI_STATE_AP &&
        status.state != MEET_WIFI_STATE_AP_FALLBACK) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN,
                            "Provisioning is only allowed over SoftAP");
        return ESP_FAIL;
    }

    if (req->content_len >= PORTAL_BODY_MAX) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Form too large");
        return ESP_FAIL;
    }

    char body[PORTAL_BODY_MAX];
    size_t total = 0;
    while (total < (size_t)req->content_len && total < sizeof(body) - 1) {
        int received =
            httpd_req_recv(req, body + total, sizeof(body) - 1 - total);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            break;
        }
        total += (size_t)received;
    }
    body[total] = '\0';

    if (total == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty form");
        return ESP_FAIL;
    }

    char ssid[33] = {0};
    char password[65] = {0};
    char node_url[128] = {0};
    char pairing_code[33] = {0};
    bool has_ssid = form_get(body, "ssid", ssid, sizeof(ssid));
    bool has_url = form_get(body, "node_url", node_url, sizeof(node_url));
    (void)form_get(body, "password", password, sizeof(password));
    (void)form_get(body, "pairing_code", pairing_code, sizeof(pairing_code));

    if (!has_ssid || ssid[0] == '\0' || !has_url || node_url[0] == '\0' ||
        strncmp(node_url, "http://", 7) != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "SSID and http:// Node URL are required");
        return ESP_FAIL;
    }

    esp_err_t err = save_config(ssid, password, node_url, pairing_code);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS save failed: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "provisioning saved: ssid='%s' node_url='%s' password=%s "
                  "pairing_code=%s",
             ssid, node_url, password[0] != '\0' ? "set" : "empty",
             pairing_code[0] != '\0' ? "set" : "empty");

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req,
                       "<!doctype html><meta charset=\"utf-8\">"
                       "<h1>Saved</h1><p>Device will reboot and connect.</p>");
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
    return ESP_OK;
}

static const httpd_uri_t URI_ROOT = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = portal_root,
};
static const httpd_uri_t URI_STATUS = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = portal_status,
};
static const httpd_uri_t URI_SAVE = {
    .uri = "/save",
    .method = HTTP_POST,
    .handler = portal_save,
};

static esp_err_t portal_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 4;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "portal HTTP server failed: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }
    (void)httpd_register_uri_handler(s_server, &URI_ROOT);
    (void)httpd_register_uri_handler(s_server, &URI_STATUS);
    (void)httpd_register_uri_handler(s_server, &URI_SAVE);
    ESP_LOGI(TAG, "portal ready: http://192.168.4.1/");
    return ESP_OK;
}

static esp_err_t wifi_start_ap(bool fallback)
{
    if (s_wifi_started) {
        (void)esp_wifi_stop();
        s_wifi_started = false;
    }

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AP mode failed: %s", esp_err_to_name(err));
        return err;
    }

    wifi_config_t ap_config;
    memset(&ap_config, 0, sizeof(ap_config));
    copy_str((char *)ap_config.ap.ssid, sizeof(ap_config.ap.ssid), s_ap_ssid);
    ap_config.ap.ssid_len = (uint8_t)strlen(s_ap_ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.pmf_cfg.required = false;

    err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AP config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AP start failed: %s", esp_err_to_name(err));
        return err;
    }
    s_wifi_started = true;
    (void)esp_netif_set_default_netif(s_ap_netif);
    status_set_state(fallback ? MEET_WIFI_STATE_AP_FALLBACK
                              : MEET_WIFI_STATE_AP);
    status_set_ip("192.168.4.1");
    ESP_LOGI(TAG, "SoftAP '%s' open, portal http://192.168.4.1/",
             s_ap_ssid);
    return portal_start();
}

static void refresh_sta_ip(void)
{
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_sta_netif, &ip_info) != ESP_OK) {
        return;
    }
    char ip[16] = {0};
    (void)esp_ip4addr_ntoa(&ip_info.ip, ip, sizeof(ip));

    status_take();
    bool changed = strcmp(s_status.ip, ip) != 0;
    if (changed) {
        copy_str(s_status.ip, sizeof(s_status.ip), ip);
    }
    status_give();
    if (changed) {
        ESP_LOGI(TAG, "STA IP refreshed: %s", ip);
    }
}

static void wifi_prov_task(void *arg)
{
    (void)arg;
    if (s_config.has_config) {
        status_set_state(MEET_WIFI_STATE_CONNECTING);
        if (wifi_connect_sta()) {
            status_set_state(MEET_WIFI_STATE_CONNECTED);
            check_node_reachable();

            /* Keep the STA alive across short DHCP leases / hotspot drops. */
            for (;;) {
                EventBits_t bits = xEventGroupWaitBits(
                    s_wifi_events, WIFI_FAIL_BIT, pdTRUE, pdFALSE,
                    pdMS_TO_TICKS(30000));
                if ((bits & WIFI_FAIL_BIT) == 0) {
                    refresh_sta_ip();
                    continue;
                }

                ESP_LOGW(TAG, "Wi-Fi link lost; reconnecting");
                status_set_state(MEET_WIFI_STATE_CONNECTING);
                if (!wifi_connect_sta()) {
                    ESP_LOGW(TAG, "reconnect failed after %d attempts",
                             STA_MAX_ATTEMPTS);
                    break;
                }
                status_set_state(MEET_WIFI_STATE_CONNECTED);
                check_node_reachable();
            }
        } else {
            ESP_LOGW(TAG, "STA failed %d times, falling back to SoftAP",
                     STA_MAX_ATTEMPTS);
        }
    }

    esp_err_t err = wifi_start_ap(s_config.has_config);
    if (err != ESP_OK) {
        status_set_state(MEET_WIFI_STATE_FAILED);
        ESP_LOGE(TAG, "Wi-Fi provisioning failed: %s", esp_err_to_name(err));
    }
    vTaskDelete(NULL);
}

esp_err_t meet_wifi_prov_start(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.state = MEET_WIFI_STATE_BOOT;

    s_status_lock = xSemaphoreCreateMutex();
    s_wifi_events = xEventGroupCreate();
    if (s_status_lock == NULL || s_wifi_events == NULL) {
        ESP_LOGE(TAG, "failed to allocate Wi-Fi sync primitives");
        return ESP_ERR_NO_MEM;
    }

    load_config();
    status_set_config(s_config.ssid, s_config.node_url, s_config.has_config);
    ESP_LOGI(TAG, "config: has_config=%d ssid='%s' node_url='%s' pairing_code=%s",
             s_config.has_config ? 1 : 0, s_config.ssid, s_config.node_url,
             s_config.pairing_code[0] != '\0' ? "set" : "empty");

    esp_err_t err = wifi_core_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi core init failed: %s", esp_err_to_name(err));
        return err;
    }

    build_ap_ssid();
    if (xTaskCreatePinnedToCore(wifi_prov_task, "wifi_prov", 8192, NULL, 4, NULL,
                                tskNO_AFFINITY) != pdPASS) {
        ESP_LOGE(TAG, "failed to create Wi-Fi provisioning task");
        return ESP_ERR_NO_MEM;
    }
    s_initialized = true;
    return ESP_OK;
}

void meet_wifi_prov_get_status(meet_wifi_status_t *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (s_status_lock == NULL) {
        out->state = MEET_WIFI_STATE_BOOT;
        return;
    }
    status_take();
    *out = s_status;
    status_give();
}

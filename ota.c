#include "ota.h"
#include <string.h>  
#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"

static const char *TAG = "ota";
// Boot Validation 
void ota_mgr_mark_app_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    
    // Factory partition does not use OTA 
    if (running && running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        ESP_LOGI(TAG, "Running factory image, skip mark-valid");
        return;
    }

    esp_ota_img_states_t state;
    esp_err_t err = esp_ota_get_state_partition(running, &state);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Can't get OTA state (%s)", esp_err_to_name(err));
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        //New Firmware booted sucessfully - cancel rollback
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "mark_app_valid_cancel_rollback -> %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "OTA state=%d, nothing to mark", (int)state);
    }
}
// OTA Download and Flash 
static void do_ota_from_url(const char *url)
{
    ESP_LOGW(TAG, "Starting OTA from URL: %s", url);
    // Configure HTTPS client with 15 second timeout 
    // crt_bundle_attach enables server certificate verification 
    esp_http_client_config_t http_cfg = {
        .url = url,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach, // requires cert bundle enabled
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };
    // handles download, flash and verification automatically
    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK) {
        ESP_LOGW(TAG, "OTA successful. Rebooting...");
        esp_restart(); // Bootloader will load new firmware on next boot 
    } else {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
    }
}
// JSON Command Parser
bool ota_mgr_handle_cmd_json(const char *json, int len)
{
    if (!json || len <= 0) return false;

    // Guard against oversized payloads 
    if (len > 1499) {
        ESP_LOGW(TAG, "Cmd JSON too large (%d), ignoring", len);
        return false;
    }
    // copy to stack buffer and null terminate for string functions 
    // MQTT payloads are NOT null terminated by default 
    char buf[1500];
    memcpy(buf, json, len);
    buf[len] = '\0';

    ESP_LOGI(TAG, "OTA cmd received: %s", buf);  // debug — confirm function is reached

    // Find "cmd" key
    const char *cmd_key = strstr(buf, "\"cmd\"");
    const char *urlk    = strstr(buf, "\"url\"");
    if (!cmd_key || !urlk) return false;

    // Extract cmd value — handles any spacing around colon
    const char *cmd_colon = strchr(cmd_key, ':');
    if (!cmd_colon) return false;
    const char *cv1 = strchr(cmd_colon, '"');   // opening quote of value
    if (!cv1) return false;
    const char *cv2 = strchr(cv1 + 1, '"');     // closing quote of value
    if (!cv2) return false;

    // Confirm cmd value is "ota"
    int cv_len = (int)(cv2 - (cv1 + 1));
    if (cv_len != 3 || memcmp(cv1 + 1, "ota", 3) != 0) return false;

    // Extract URL value — handles any spacing around colon
    const char *colon = strchr(urlk, ':');
    if (!colon) return false;
    const char *u1 = strchr(colon, '"');        // opening quote of URL
    if (!u1) return false;
    const char *u2 = strchr(u1 + 1, '"');       // closing quote of URL
    if (!u2) return false;

    int url_len = (int)(u2 - (u1 + 1));
    if (url_len <= 0 || url_len > 1400) return false;

    char url[1450];
    memcpy(url, u1 + 1, url_len);
    url[url_len] = '\0';

    do_ota_from_url(url);
    return true;
}
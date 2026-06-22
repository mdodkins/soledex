#include "wifi.h"

#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace pokedex {
namespace {

const char* TAG = "wifi";
constexpr int kConnectedBit = BIT0;
constexpr int kFailBit = BIT1;
constexpr int kMaxRetry = 8;

EventGroupHandle_t s_events = nullptr;
esp_ip4_addr_t s_ip = {};
int s_retries = 0;

void onEvent(void*, esp_event_base_t base, int32_t id, void* data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retries < kMaxRetry) {
      ++s_retries;
      ESP_LOGW(TAG, "disconnected, retry %d/%d", s_retries, kMaxRetry);
      esp_wifi_connect();
    } else {
      xEventGroupSetBits(s_events, kFailBit);
    }
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    auto* event = static_cast<ip_event_got_ip_t*>(data);
    s_ip = event->ip_info.ip;
    s_retries = 0;
    xEventGroupSetBits(s_events, kConnectedBit);
  }
}

// Tolerate the default event loop already existing (e.g. created by M5.begin()).
esp_err_t ensureDefaultEventLoop() {
  esp_err_t err = esp_event_loop_create_default();
  return err == ESP_ERR_INVALID_STATE ? ESP_OK : err;
}

}  // namespace

bool wifiConnect(const char* ssid, const char* password, std::string& ip_out,
                 int timeout_ms) {
  s_events = xEventGroupCreate();
  s_retries = 0;

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(ensureDefaultEventLoop());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

  esp_event_handler_instance_t any_id, got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &onEvent, nullptr, &any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &onEvent, nullptr, &got_ip));

  wifi_config_t wc = {};
  std::strncpy(reinterpret_cast<char*>(wc.sta.ssid), ssid,
               sizeof(wc.sta.ssid) - 1);
  std::strncpy(reinterpret_cast<char*>(wc.sta.password), password,
               sizeof(wc.sta.password) - 1);
  wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "connecting to '%s'...", ssid);
  EventBits_t bits = xEventGroupWaitBits(s_events, kConnectedBit | kFailBit,
                                         pdFALSE, pdFALSE,
                                         pdMS_TO_TICKS(timeout_ms));
  if (bits & kConnectedBit) {
    char buf[16];
    esp_ip4addr_ntoa(&s_ip, buf, sizeof(buf));
    ip_out = buf;
    ESP_LOGI(TAG, "connected, ip=%s", buf);
    return true;
  }
  ESP_LOGE(TAG, "connect failed (%s)", (bits & kFailBit) ? "auth/AP" : "timeout");
  return false;
}

}  // namespace pokedex

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "m5gfx_display.h"
#include "net.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "pokedex/battery.h"
#include "pokedex/card_browser.h"
#include "pokedex/card_prefetch.h"
#include "pokedex/deck.h"
#include "pokedex/deck_screen.h"
#include "pokedex/gesture.h"
#include "pokedex/hit_test.h"
#include "pokedex/home_screen.h"
#include "pokedex/keyboard.h"
#include "pokedex/png_info.h"
#include "pokedex/render.h"
#include "secrets.h"
#include "wifi.h"

// PNGs embedded into flash by the build (EMBED_FILES in CMakeLists.txt).
extern const uint8_t title_start[] asm("_binary_title_soledex_png_start");
extern const uint8_t title_end[] asm("_binary_title_soledex_png_end");
extern const uint8_t hero_start[] asm("_binary_jolteon_mid_png_start");
extern const uint8_t hero_end[] asm("_binary_jolteon_mid_png_end");
extern const uint8_t mic_start[] asm("_binary_mic_button_png_start");
extern const uint8_t mic_end[] asm("_binary_mic_button_png_end");
extern const uint8_t kbd_start[] asm("_binary_keyboard_button_png_start");
extern const uint8_t kbd_end[] asm("_binary_keyboard_button_png_end");
extern const uint8_t card_start[] asm("_binary_card_morelull_png_start");
extern const uint8_t card_end[] asm("_binary_card_morelull_png_end");

namespace {
const char* TAG = "pokedex";

constexpr uint32_t kSampleRate = 16000;
constexpr size_t kMaxSeconds = 8;
constexpr size_t kMaxSamples = kSampleRate * kMaxSeconds;
constexpr int kMaxCards = 60;  // pokemontcg.io page size (cards per search)

std::size_t len(const uint8_t* start, const uint8_t* end) {
  return static_cast<std::size_t>(end - start);
}

// ---- Deck (persisted in NVS) ------------------------------------------------
// The single deck the user builds. Cards are their image URLs; the model and
// its serialization are the host-tested pokedex::Deck. We persist the flat blob
// under one NVS key so the deck survives reboots.
pokedex::Deck s_deck;
// The deck lives in its own NVS partition (see partitions.csv) so it is never
// collateral when the shared system `nvs` (WiFi) is wiped on NO_FREE_PAGES or an
// NVS-format upgrade. initDeckStorage() must run once before load/save.
constexpr char kDeckNvsPartition[] = "deck_nvs";
constexpr char kDeckNvsNamespace[] = "pokedex";
constexpr char kDeckNvsKey[] = "deck";

// Bring up the dedicated deck partition. If it is brand new (first flash) or its
// pages are exhausted, format just THIS partition — never the system nvs.
void initDeckStorage() {
  esp_err_t err = nvs_flash_init_partition(kDeckNvsPartition);
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase_partition(kDeckNvsPartition));
    err = nvs_flash_init_partition(kDeckNvsPartition);
  }
  if (err != ESP_OK) ESP_LOGE(TAG, "deck NVS init failed: %s", esp_err_to_name(err));
}

void loadDeck() {
  nvs_handle_t h;
  if (nvs_open_from_partition(kDeckNvsPartition, kDeckNvsNamespace,
                              NVS_READONLY, &h) != ESP_OK) {
    return;
  }
  std::size_t need = 0;
  if (nvs_get_str(h, kDeckNvsKey, nullptr, &need) == ESP_OK && need > 0) {
    std::vector<char> buf(need);
    if (nvs_get_str(h, kDeckNvsKey, buf.data(), &need) == ESP_OK) {
      s_deck = pokedex::Deck::deserialize(std::string(buf.data()));
    }
  }
  nvs_close(h);
  ESP_LOGI(TAG, "loaded deck: %d card(s)", s_deck.size());
}

void saveDeck() {
  nvs_handle_t h;
  if (nvs_open_from_partition(kDeckNvsPartition, kDeckNvsNamespace,
                              NVS_READWRITE, &h) != ESP_OK) {
    return;
  }
  std::string blob = s_deck.serialize();
  nvs_set_str(h, kDeckNvsKey, blob.c_str());
  nvs_commit(h);
  nvs_close(h);
}

// ---- Status line + animated "busy" indicator -------------------------------
// All display writes go through s_disp_mtx so the background animator (busyTask)
// can update the dots during blocking HTTP calls without racing the main task.
SemaphoreHandle_t s_disp_mtx = nullptr;
volatile bool s_busy = false;
std::string s_busy_base;
uint32_t s_busy_color = 0;
int s_busy_dots = 1;

// Draw the bottom status strip. Caller must hold s_disp_mtx.
void drawStatusRaw(const char* msg, uint32_t color) {
  int h = M5.Display.height();
  int w = M5.Display.width();
  M5.Display.fillRect(0, h - 110, w, 110, 0x000000u);
  if (msg[0] != '\0') {
    M5.Display.setTextColor(color);
    M5.Display.setTextSize(5);
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString(msg, w / 2, h - 55);
  }
}

void showStatus(const char* msg, uint32_t color) {
  if (s_disp_mtx) xSemaphoreTake(s_disp_mtx, portMAX_DELAY);
  drawStatusRaw(msg, color);
  if (s_disp_mtx) xSemaphoreGive(s_disp_mtx);
}

// Animates "<base>" with 1..3 cycling dots while s_busy is set.
void busyTask(void*) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(350));
    if (!s_busy) continue;
    xSemaphoreTake(s_disp_mtx, portMAX_DELAY);
    if (s_busy) {  // re-check under the lock so we never draw after stopBusy()
      std::string m = s_busy_base + std::string(s_busy_dots, '.');
      drawStatusRaw(m.c_str(), s_busy_color);
      s_busy_dots = s_busy_dots >= 3 ? 1 : s_busy_dots + 1;
    }
    xSemaphoreGive(s_disp_mtx);
  }
}

// Show "<base>." now and animate the dots until stopBusy().
void startBusy(const char* base, uint32_t color) {
  s_busy_base = base;
  s_busy_color = color;
  s_busy_dots = 1;
  s_busy = true;
  showStatus((std::string(base) + ".").c_str(), color);
}
void stopBusy() {
  s_busy = false;
  // Fence: wait out any in-flight busyTask draw so the caller's subsequent
  // (unlocked) drawing — card images, the keyboard — can't race it.
  if (s_disp_mtx) {
    xSemaphoreTake(s_disp_mtx, portMAX_DELAY);
    xSemaphoreGive(s_disp_mtx);
  }
}

// Draw the "i / N" counter, large and centred in the top margin above the card.
void drawCounter(int index, int total) {
  std::string counter = std::to_string(index + 1) + " / " + std::to_string(total);
  M5.Display.setTextColor(0xFFFFFFu);
  M5.Display.setTextSize(5);
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString(counter.c_str(), M5.Display.width() / 2, 16);
}

// Downloaded card PNGs, keyed by image URL, kept in PSRAM (large allocations
// spill there automatically). std::map keeps references/pointers stable as
// other entries are inserted or evicted, so a pointer into the cache stays valid
// while we draw from it. Bounded to a small window around the current card by
// evictOutsideCache().
std::map<std::string, std::string> s_card_cache;

// Return the PNG bytes for `url`, downloading and caching them on a miss. A
// failed download is not cached (so it retries) and returns an empty string.
const std::string& loadCard(const std::string& url) {
  auto it = s_card_cache.find(url);
  if (it != s_card_cache.end()) return it->second;  // hit: no network

  std::string png;
  int status = pokedex::httpGet(url, png);
  if (status != 200 || png.empty()) {
    ESP_LOGE(TAG, "card GET %d (%u bytes)", status, (unsigned)png.size());
    static const std::string kEmpty;
    return kEmpty;
  }
  ESP_LOGI(TAG, "cached card %u bytes (%u resident, free_heap=%u)",
           (unsigned)png.size(), (unsigned)s_card_cache.size() + 1,
           (unsigned)esp_get_free_heap_size());
  return s_card_cache.emplace(url, std::move(png)).first->second;
}

// Drop cached cards outside the prefetch window around `index`, bounding memory.
void evictOutsideCache(const std::vector<std::string>& urls, int index) {
  std::set<std::string> keep;
  for (int j : pokedex::prefetchWindow(index, static_cast<int>(urls.size()),
                                       pokedex::kPrefetchBack,
                                       pokedex::kPrefetchAhead)) {
    keep.insert(urls[j]);
  }
  for (auto it = s_card_cache.begin(); it != s_card_cache.end();) {
    it = keep.count(it->first) ? std::next(it) : s_card_cache.erase(it);
  }
}

// Download the single highest-priority not-yet-cached card in the window around
// `index`. Returns true if it fetched one (i.e. there is more buffering to do).
// Called when idle so the next swipe lands on an already-resident card.
bool prefetchOne(const std::vector<std::string>& urls, int index) {
  for (int j : pokedex::prefetchWindow(index, static_cast<int>(urls.size()),
                                       pokedex::kPrefetchBack,
                                       pokedex::kPrefetchAhead)) {
    if (s_card_cache.find(urls[j]) == s_card_cache.end()) {
      loadCard(urls[j]);
      return true;
    }
  }
  return false;
}

// Draw a card PNG scaled-to-fit, centred, loading it from cache (or the network
// on a miss). Returns false on a download/decode failure. The "i / N" counter is
// shown first so the user sees which card is loading on a cache miss.
bool showCardImage(const std::string& url, int index, int total) {
  M5.Display.fillScreen(0x000000u);
  drawCounter(index, total);

  const std::string& png = loadCard(url);
  if (png.empty()) return false;
  const auto* bytes = reinterpret_cast<const uint8_t*>(png.data());
  const int disp_w = M5.Display.width();
  const int disp_h = M5.Display.height();

  // Scale to fit while preserving aspect, then centre by computing the top-left
  // ourselves (drawPng's datum arg doesn't position reliably here).
  float scale = 1.0f;
  int out_w = disp_w, out_h = disp_h;
  auto size = pokedex::pngSize(bytes, png.size());
  if (size && size->width > 0 && size->height > 0) {
    float sw = static_cast<float>(disp_w) / size->width;
    float sh = static_cast<float>(disp_h) / size->height;
    scale = sw < sh ? sw : sh;
    out_w = static_cast<int>(size->width * scale);
    out_h = static_cast<int>(size->height * scale);
  }
  const int x = (disp_w - out_w) / 2;
  const int y = (disp_h - out_h) / 2;

  M5.Display.drawPng(bytes, png.size(), x, y, 0, 0, 0, 0, scale, scale);
  drawCounter(index, total);  // redraw on top of the card image
  return true;
}

// Draw one of the rounded-rect deck buttons that overlay a card image, centring
// `label` in it. Matches the keyboard keys' look.
void drawDeckButton(pokedex::Point origin, const char* label, uint32_t fill,
                    int text_size) {
  const auto s = pokedex::kDeckButtonSize;
  M5.Display.fillRoundRect(origin.x, origin.y, s.width, s.height, 14, fill);
  M5.Display.setTextColor(0xFFFFFFu);
  M5.Display.setTextSize(text_size);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString(label, origin.x + s.width / 2, origin.y + s.height / 2);
}

// The "+ / Added" button, top-right: green "+" while the card can still be
// added, muted grey "Added" once it is in the deck.
void drawAddButton(const std::string& url) {
  bool added = s_deck.contains(url);
  drawDeckButton(pokedex::addButtonOrigin(M5.Display.width()),
                 pokedex::addButtonLabel(s_deck, url),
                 added ? 0x404040u : 0x107C10u, added ? 4 : 7);
}

// The "-" remove button, top-left, shown only while viewing the deck.
void drawRemoveButton() {
  drawDeckButton(pokedex::removeButtonOrigin(), "-", 0xA01010u, 7);
}

// True if a release at `last` (with no swipe) was a tap inside `origin`'s
// deck-button rectangle.
bool tappedButton(pokedex::Point origin, pokedex::Swipe swipe,
                  pokedex::Point last) {
  return swipe == pokedex::Swipe::None &&
         pokedex::contains(origin, pokedex::kDeckButtonSize, last);
}

// Show the result cards and let the user swipe: left/right between cards, up to
// return to the home screen. Tapping the top-right "+" adds the current card to
// the deck (button then reads "Added").
void runResultsBrowser(const std::vector<std::string>& urls) {
  const int total = static_cast<int>(urls.size());
  const pokedex::Point add_origin =
      pokedex::addButtonOrigin(M5.Display.width());
  pokedex::CardBrowser browser(total);
  showCardImage(urls[0], 0, total);
  drawAddButton(urls[0]);
  evictOutsideCache(urls, 0);

  bool prev_down = false;
  pokedex::Point start{0, 0}, last{0, 0};
  while (true) {
    M5.update();
    bool down = M5.Touch.getCount() > 0;
    auto t = M5.Touch.getDetail();
    if (down) {
      last = {t.x, t.y};
      if (!prev_down) start = last;
    } else if (prev_down) {
      pokedex::Swipe swipe = pokedex::detectSwipe(start, last, 60);
      if (tappedButton(add_origin, swipe, last)) {
        s_deck.add(urls[browser.index()]);
        saveDeck();
        drawAddButton(urls[browser.index()]);  // now "Added"
      } else {
        pokedex::BrowseAction act = browser.apply(swipe);
        if (act == pokedex::BrowseAction::GoHome) return;
        if (act == pokedex::BrowseAction::NextCard ||
            act == pokedex::BrowseAction::PrevCard) {
          showCardImage(urls[browser.index()], browser.index(), total);
          drawAddButton(urls[browser.index()]);
          evictOutsideCache(urls, browser.index());
        }
      }
    } else {
      // Idle (finger up): buffer the next card so the coming swipe is instant.
      prefetchOne(urls, browser.index());
    }
    prev_down = down;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Browse the deck the same way (swipe left/right, up to go home). Tapping the
// top-left "-" removes the current card; the deck shrinks live and we drop back
// home once it is empty.
void runDeckBrowser() {
  std::vector<std::string> urls = s_deck.cards();  // snapshot
  if (urls.empty()) {
    M5.Display.fillScreen(0x000000u);
    showStatus("Deck is empty", 0xFFD000u);
    vTaskDelay(pdMS_TO_TICKS(1500));
    return;
  }
  const pokedex::Point remove_origin = pokedex::removeButtonOrigin();
  int index = 0;
  pokedex::CardBrowser browser(static_cast<int>(urls.size()));
  showCardImage(urls[index], index, static_cast<int>(urls.size()));
  drawRemoveButton();
  evictOutsideCache(urls, index);

  bool prev_down = false;
  pokedex::Point start{0, 0}, last{0, 0};
  while (true) {
    M5.update();
    bool down = M5.Touch.getCount() > 0;
    auto t = M5.Touch.getDetail();
    if (down) {
      last = {t.x, t.y};
      if (!prev_down) start = last;
    } else if (prev_down) {
      pokedex::Swipe swipe = pokedex::detectSwipe(start, last, 60);
      if (tappedButton(remove_origin, swipe, last)) {
        s_deck.remove(urls[index]);
        saveDeck();
        urls.erase(urls.begin() + index);
        if (urls.empty()) return;  // deck emptied -> back home
        if (index >= static_cast<int>(urls.size())) index = urls.size() - 1;
        // Rebuild the browser for the new count, re-advanced to `index`.
        browser = pokedex::CardBrowser(static_cast<int>(urls.size()));
        for (int i = 0; i < index; ++i) browser.apply(pokedex::Swipe::Left);
        showCardImage(urls[index], index, static_cast<int>(urls.size()));
        drawRemoveButton();
        evictOutsideCache(urls, index);
      } else {
        pokedex::BrowseAction act = browser.apply(swipe);
        if (act == pokedex::BrowseAction::GoHome) return;
        if (act == pokedex::BrowseAction::NextCard ||
            act == pokedex::BrowseAction::PrevCard) {
          index = browser.index();
          showCardImage(urls[index], index, static_cast<int>(urls.size()));
          drawRemoveButton();
          evictOutsideCache(urls, index);
        }
      }
    } else {
      prefetchOne(urls, index);  // buffer ahead while idle
    }
    prev_down = down;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Map a spoken/typed phrase to cards via Claude + pokemontcg.io, then hand off
// to the swipeable results browser. Shared by the mic and keyboard inputs.
void runSearchForPhrase(const std::string& phrase) {
  startBusy("Thinking", 0xFFD000u);
  std::string query;
  bool ok = pokedex::claudeQuery(phrase, query);
  stopBusy();
  if (!ok) {
    // Almost always a transient Claude API issue (e.g. HTTP 529 overloaded).
    // Hold the message long enough for the user to read it.
    showStatus("AI server error", 0xFF3030u);
    vTaskDelay(pdMS_TO_TICKS(15000));
    return;
  }
  ESP_LOGI(TAG, "query: %s", query.c_str());

  startBusy("Finding cards", 0xFFD000u);
  std::vector<std::string> urls;
  ok = pokedex::tcgFetchImageUrls(query, kMaxCards, urls);
  stopBusy();
  if (!ok || urls.empty()) {
    showStatus("No cards found", 0xFF3030u);
    vTaskDelay(pdMS_TO_TICKS(1500));
    return;
  }
  ESP_LOGI(TAG, "found %u cards", (unsigned)urls.size());
  runResultsBrowser(urls);  // returns on swipe-up
}

// ---- On-screen QWERTY keyboard ---------------------------------------------

// The text field along the top: white rounded border + the typed text & cursor.
void drawTextBox(const std::string& text) {
  const int w = M5.Display.width();
  const int fx = 20, fy = 40, fw = w - 40, fh = 120;
  M5.Display.fillRect(0, 0, w, fy + fh + 20, 0x000000u);  // clear top region
  M5.Display.drawRoundRect(fx, fy, fw, fh, 12, 0xFFFFFFu);
  std::string shown = text + "_";  // trailing cursor
  M5.Display.setTextColor(0xFFFFFFu);
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(textdatum_t::middle_left);
  M5.Display.drawString(shown.c_str(), fx + 16, fy + fh / 2);
}

// Draw one key: dark letter keys, an amber Backspace and a green Go button.
void drawKey(const pokedex::Key& k) {
  uint32_t fill = 0x303030u;
  std::string label;
  int text_size = 4;
  if (k.kind == pokedex::KeyKind::Char) {
    label = std::string(1, static_cast<char>(std::toupper(k.ch)));
  } else if (k.kind == pokedex::KeyKind::Space) {
    label = "SPACE";
    text_size = 3;
  } else if (k.kind == pokedex::KeyKind::Backspace) {
    label = "<-";
    fill = 0x804000u;
    text_size = 3;
  } else {  // Go
    label = "GO";
    fill = 0x107C10u;
    text_size = 5;
  }
  const int x = k.origin.x, y = k.origin.y, w = k.size.width, h = k.size.height;
  M5.Display.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 8, fill);
  M5.Display.setTextColor(0xFFFFFFu);
  M5.Display.setTextSize(text_size);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString(label.c_str(), x + w / 2, y + h / 2);
}

// Show the keyboard and collect a typed phrase. Returns the text when Go is
// pressed (empty string = cancel, e.g. Go with no text typed).
std::string runKeyboardScreen() {
  stopBusy();  // the background animator must not draw over the keyboard
  auto kb = pokedex::keyboardLayout(M5.Display.width(), M5.Display.height());
  std::string text;

  M5.Display.fillScreen(0x000000u);
  for (const auto& k : kb.keys) drawKey(k);
  drawTextBox(text);

  bool prev_down = false;
  pokedex::Point last{0, 0};
  while (true) {
    M5.update();
    bool down = M5.Touch.getCount() > 0;
    if (down) {
      auto t = M5.Touch.getDetail();
      last = {t.x, t.y};
    } else if (prev_down) {  // release edge -> treat as a tap at `last`
      const pokedex::Key* k = pokedex::keyAt(kb, last);
      if (k) {
        if (k->kind == pokedex::KeyKind::Go) return text;
        text = pokedex::applyKey(text, *k);
        drawTextBox(text);
      }
    }
    prev_down = down;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// The battery indicator in the top-right of the home screen: a 4-bar icon whose
// fill reflects charge level, plus a yellow bolt to its left while charging.
// Bars are green when charging, red when low, amber mid, white when healthy.
void drawBattery() {
  constexpr int kBars = 4;
  const int level = M5.Power.getBatteryLevel();
  // Judge charging from current direction (into battery = +), not the CHG_STAT
  // pin, which can stay asserted after the cable is pulled.
  const int current_ma = M5.Power.getBatteryCurrent();
  const bool charging = pokedex::batteryCharging(current_ma);
  ESP_LOGI(TAG, "battery %d%% cur=%dmA charging=%d", level, current_ma,
           (int)charging);
  const auto o = pokedex::batteryIconOrigin(M5.Display.width());
  const auto s = pokedex::kBatteryIconSize;
  const int filled = pokedex::batteryBars(level, kBars);

  // Clear the whole indicator region (bolt area on the left + body + nub).
  M5.Display.fillRect(o.x - 28, o.y - 2, s.width + 28 + 12, s.height + 4,
                      0x000000u);

  if (charging) {  // a simple lightning bolt from two triangles
    const int bx = o.x - 24, by = o.y;
    M5.Display.fillTriangle(bx + 11, by, bx, by + 18, bx + 10, by + 18,
                            0xFFD000u);
    M5.Display.fillTriangle(bx + 10, by + 14, bx + 20, by + 14, bx + 9,
                            by + s.height, 0xFFD000u);
  }

  // Body outline + the little terminal nub on the right.
  M5.Display.drawRoundRect(o.x, o.y, s.width, s.height, 4, 0xFFFFFFu);
  M5.Display.fillRect(o.x + s.width, o.y + s.height / 2 - 6, 6, 12, 0xFFFFFFu);

  // Filled bars. Yellow is reserved for the charging bolt, so a discharging
  // battery is never yellow: green while charging, red when low, else white.
  const uint32_t fill = charging      ? 0x30FF30u
                        : level <= 20 ? 0xFF3030u
                                      : 0xFFFFFFu;
  const int pad = 4;
  const int bar_w = (s.width - pad * (kBars + 1)) / kBars;
  const int bar_h = s.height - pad * 2;
  for (int i = 0; i < filled; ++i) {
    M5.Display.fillRect(o.x + pad + i * (bar_w + pad), o.y + pad, bar_w, bar_h,
                        fill);
  }
}

// The blue "View deck" button on the home screen, above Jolteon. renderHome
// leaves the gap for it; we draw the text button there ourselves.
void drawViewDeckButton(const pokedex::HomeLayout& layout) {
  const auto o = layout.deck;
  const auto s = pokedex::kViewDeckButtonSize;
  M5.Display.fillRoundRect(o.x, o.y, s.width, s.height, 14, 0x1060C0u);
  M5.Display.setTextColor(0xFFFFFFu);
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.drawString("View deck", o.x + s.width / 2, o.y + s.height / 2);
}
}  // namespace

extern "C" void app_main(void) {
  auto cfg = M5.config();
  M5.begin(cfg);  // powers + initialises display, mic, speaker, touch

  M5.Speaker.setVolume(255);  // max playback volume
  {
    auto mic_cfg = M5.Mic.config();
    mic_cfg.magnification = 8;  // modest gain; speaker volume gives loudness
    M5.Mic.config(mic_cfg);
  }

  // Make sure the battery actually charges while plugged in: enable charging and
  // request the Tab5's max rate (1000mA, QuickCharge on). M5.begin() enables
  // charging by default, but this is cheap insurance and helps the charge keep
  // up with the screen + WiFi + audio load.
  M5.Power.setBatteryCharge(true);
  M5.Power.setChargeCurrent(1000);
  ESP_LOGI(TAG, "battery %d%%, cur=%dmA, charging=%d",
           (int)M5.Power.getBatteryLevel(), (int)M5.Power.getBatteryCurrent(),
           (int)pokedex::batteryCharging(M5.Power.getBatteryCurrent()));

  pokedex::M5GfxDisplay display;
  if (!display.begin()) {
    ESP_LOGE(TAG, "display init failed");
    return;
  }
  ESP_LOGI(TAG, "display %dx%d", display.width(), display.height());

  // Status-line mutex + background "busy dots" animator. Created before any
  // showStatus()/startBusy() call so both tasks share the display safely.
  s_disp_mtx = xSemaphoreCreateMutex();
  xTaskCreate(busyTask, "busy", 4096, nullptr, 4, nullptr);

  // Mic + keyboard button hit rectangles + the deck button position (same
  // layout maths the renderer used).
  auto mic_size = pokedex::pngSize(mic_start, len(mic_start, mic_end));
  auto kbd_size = pokedex::pngSize(kbd_start, len(kbd_start, kbd_end));
  pokedex::HomeLayout layout = pokedex::homeLayout(
      display.width(), display.height(),
      *pokedex::pngSize(title_start, len(title_start, title_end)),
      *pokedex::pngSize(hero_start, len(hero_start, hero_end)), *mic_size,
      *kbd_size);

  auto render = [&] {
    bool ok =
        pokedex::renderHome(display, title_start, len(title_start, title_end),
                            hero_start, len(hero_start, hero_end), mic_start,
                            len(mic_start, mic_end), kbd_start,
                            len(kbd_start, kbd_end));
    drawViewDeckButton(layout);  // overlay the text button renderHome left room for
    drawBattery();               // top-right charge indicator
    return ok;
  };
  render();

  // Bring up WiFi (via the ESP32-C6) so we can reach the STT + APIs.
  esp_err_t nvs = nvs_flash_init();
  if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }
  initDeckStorage();  // bring up the dedicated deck partition
  loadDeck();          // restore the saved deck
  showStatus("WiFi: connecting...", 0xFFD000u);
  std::string ip;
  if (pokedex::wifiConnect(WIFI_SSID, WIFI_PASSWORD, ip)) {
    ESP_LOGI(TAG, "online, STT_URL=%s", STT_URL);
    showStatus(("WiFi " + ip).c_str(), 0x30FF30u);
    vTaskDelay(pdMS_TO_TICKS(2000));  // let the user see the IP briefly
    showStatus("Ready!", 0x30FF30u);
  } else {
    showStatus("WiFi failed", 0xFF3030u);
  }

  // Recording buffer in PSRAM.
  auto* buf = static_cast<int16_t*>(
      heap_caps_malloc(kMaxSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM));
  if (buf == nullptr) {
    ESP_LOGE(TAG, "failed to allocate %u-sample buffer", (unsigned)kMaxSamples);
    return;
  }

  bool recording = false;
  bool last_down = false;
  bool press_on_kbd = false;                  // press began on the keyboard icon
  bool press_on_deck = false;                 // press began on the View deck button
  size_t rec_index = 0;                       // samples captured so far
  const size_t kChunk = kSampleRate / 10;     // 100ms per record() call
  int battery_ticks = 0;  // refresh the indicator every ~2s (100 * 20ms)

  while (true) {
    // Periodically refresh the battery indicator so plugging/unplugging and
    // level changes show up without leaving the home screen. Skip while
    // recording so it can't fight the recording UI for the display.
    if (!recording && ++battery_ticks >= 100) {
      battery_ticks = 0;
      drawBattery();
    }
    M5.update();
    // Current finger count gives true press-and-hold semantics.
    bool finger_down = M5.Touch.getCount() > 0;
    auto t = M5.Touch.getDetail();
    pokedex::Point tp{t.x, t.y};
    bool on_mic = finger_down && pokedex::contains(layout.mic, *mic_size, tp);
    bool press_edge = finger_down && !last_down;
    bool release_edge = !finger_down && last_down;
    if (press_edge) {
      press_on_kbd = pokedex::contains(layout.kbd, *kbd_size, tp);
      press_on_deck =
          pokedex::contains(layout.deck, pokedex::kViewDeckButtonSize, tp);
    }

    if (finger_down != last_down) {
      ESP_LOGI(TAG, "touch %s count=%d x=%d y=%d on_mic=%d",
               finger_down ? "DOWN" : "UP", M5.Touch.getCount(), t.x, t.y,
               on_mic);
      last_down = finger_down;
    }

    // Tap on the "View deck" button -> browse the saved deck (minus to remove).
    if (release_edge && press_on_deck && !recording) {
      press_on_deck = false;
      runDeckBrowser();  // returns on swipe-up or when the deck empties
      render();          // back to the home screen
      continue;
    }

    // Tap on the keyboard icon -> open the on-screen keyboard. On GO, plumb the
    // typed phrase through the same search path the mic uses.
    if (release_edge && press_on_kbd && !recording) {
      press_on_kbd = false;
      std::string phrase = runKeyboardScreen();
      if (!phrase.empty()) {
        ESP_LOGI(TAG, "typed: \"%s\"", phrase.c_str());
        runSearchForPhrase(phrase);
      }
      render();  // back to the home screen
      continue;
    }

    // Start recording when a finger lands on the mic button.
    if (!recording && on_mic) {
      M5.Speaker.end();  // mic and speaker share the audio bus
      if (M5.Mic.begin()) {
        recording = true;
        rec_index = 0;
        showStatus("\xE2\x97\x8f Recording...", 0xFF3030u);
        ESP_LOGI(TAG, "recording started");
      } else {
        ESP_LOGE(TAG, "mic begin failed");
      }
    }

    // While held, capture one small chunk at a time. Small chunks keep the
    // release responsive (M5.Mic.end() only waits for the current chunk).
    if (recording && finger_down && rec_index + kChunk <= kMaxSamples) {
      if (M5.Mic.record(buf + rec_index, kChunk, kSampleRate)) {
        int guard = 0;
        while (M5.Mic.isRecording() && guard++ < 100) {
          vTaskDelay(pdMS_TO_TICKS(2));
        }
        rec_index += kChunk;
      }
      continue;  // re-poll touch immediately — no tail delay, no audio gap
    }

    // Stop + play back on release (or when the buffer is full).
    if (recording && (!finger_down || rec_index + kChunk > kMaxSamples)) {
      M5.Mic.end();  // current chunk is short, so this returns quickly
      recording = false;
      size_t recorded = rec_index;

      int32_t peak = 0;
      for (size_t i = 0; i < recorded; ++i) {
        int32_t a = buf[i] < 0 ? -buf[i] : buf[i];
        if (a > peak) peak = a;
      }
      ESP_LOGI(TAG, "recorded %u samples (%.1fs), peak=%d/32767, playing back",
               (unsigned)recorded, recorded / (float)kSampleRate, (int)peak);

      showStatus("\xE2\x96\xB6 Playing...", 0x30FF30u);
      M5.Speaker.begin();
      M5.Speaker.setVolume(255);
      M5.Speaker.playRaw(buf, recorded, kSampleRate);
      while (M5.Speaker.isPlaying()) {
        M5.update();
        vTaskDelay(pdMS_TO_TICKS(10));
      }

      // Voice -> STT -> Claude -> pokemontcg.io -> swipeable cards.
      startBusy("Transcribing", 0xFFD000u);
      std::string transcript;
      bool heard =
          pokedex::sttTranscribe(buf, recorded, kSampleRate, transcript) &&
          !transcript.empty();
      stopBusy();
      if (!heard) {
        showStatus("Didn't catch that", 0xFF3030u);
        vTaskDelay(pdMS_TO_TICKS(1500));
      } else {
        ESP_LOGI(TAG, "heard: \"%s\"", transcript.c_str());
        showStatus(transcript.c_str(), 0xFFFFFFu);
        vTaskDelay(pdMS_TO_TICKS(800));  // let the user read it
        runSearchForPhrase(transcript);
      }
      render();  // back to the home screen
    }

    // >= 1 tick (default 10ms) so the idle task runs and the watchdog is fed.
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

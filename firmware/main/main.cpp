#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
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
#include "pokedex/deck_sort.h"
#include "pokedex/deck_view.h"
#include "pokedex/gesture.h"
#include "pokedex/hit_test.h"
#include "pokedex/home_screen.h"
#include "pokedex/keyboard.h"
#include "pokedex/png_info.h"
#include "pokedex/render.h"
#include "pokedex/tcg.h"
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
// Schema tag for the saved card metadata. Bump it whenever the set of fetched
// fields changes so the deck triggers a one-time backfill of the new field for
// cards saved under the old schema (e.g. evolvesFrom, added after supertype).
constexpr char kDeckSchemaKey[] = "schema";
constexpr char kDeckSchema[] = "v2-evo";
// Set at load time when the saved schema is older than kDeckSchema: the next
// time the deck opens, backfill refetches metadata for *every* card, not just
// those missing a supertype.
bool s_deckNeedsRefetch = false;

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
  // Decide whether the saved metadata predates the current schema; if so the
  // next deck open refetches every card to fill in newly-added fields.
  char schema[16] = {};
  std::size_t slen = sizeof(schema);
  bool current = nvs_get_str(h, kDeckSchemaKey, schema, &slen) == ESP_OK &&
                 std::string(schema) == kDeckSchema;
  s_deckNeedsRefetch = s_deck.size() > 0 && !current;
  nvs_close(h);
  ESP_LOGI(TAG, "loaded deck: %d card(s), schema=%s refetch=%d", s_deck.size(),
           schema[0] ? schema : "(none)", s_deckNeedsRefetch);
}

// Record that the saved metadata now matches the current schema, so the deck
// won't refetch again on the next open.
void markDeckSchemaCurrent() {
  nvs_handle_t h;
  if (nvs_open_from_partition(kDeckNvsPartition, kDeckNvsNamespace,
                              NVS_READWRITE, &h) != ESP_OK) {
    return;
  }
  nvs_set_str(h, kDeckSchemaKey, kDeckSchema);
  nvs_commit(h);
  nvs_close(h);
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

// Decoded card frames, keyed by image URL: each is a full-screen sprite (the
// card scaled-to-fit, centred, on black) held in PSRAM. We decode the PNG once
// here so that showing a card is a fast pushSprite (DMA blit) instead of
// re-inflating + re-scaling the PNG on every swipe. std::unique_ptr frees the
// sprite's PSRAM buffer on eviction. Bounded to a small window around the
// current card by evictOutsideCache().
std::map<std::string, std::unique_ptr<M5Canvas>> s_card_cache;

// What a card-load miss narrates on the status strip. "Loading next card" while
// browsing one at a time; "Loading 4 cards" in the deck's 2x2 grid view.
std::string s_cardLoadLabel = "Loading next card";

// Decode a card PNG into a fresh full-screen sprite, scaled-to-fit and centred
// on black. Returns nullptr if the sprite can't be allocated or the PNG is bad.
std::unique_ptr<M5Canvas> decodeCard(const std::string& png) {
  const int disp_w = M5.Display.width();
  const int disp_h = M5.Display.height();
  const auto* bytes = reinterpret_cast<const uint8_t*>(png.data());

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

  auto canvas = std::make_unique<M5Canvas>(&M5.Display);  // PSRAM, parent depth
  canvas->setColorDepth(16);
  if (!canvas->createSprite(disp_w, disp_h)) {
    ESP_LOGE(TAG, "card sprite alloc failed (%dx%d)", disp_w, disp_h);
    return nullptr;
  }
  canvas->fillScreen(0x000000u);
  canvas->drawPng(bytes, png.size(), x, y, 0, 0, 0, 0, scale, scale);
  return canvas;
}

// Return the decoded sprite for `url`, downloading + decoding and caching it on
// a miss. Returns nullptr on a download/decode failure (not cached, so retries).
// A miss is narrated on the bottom strip with the animated "Loading next
// card..." dots (covering both the download and the decode); a hit is silent.
M5Canvas* loadCard(const std::string& url) {
  auto it = s_card_cache.find(url);
  if (it != s_card_cache.end()) return it->second.get();  // hit: no work

  startBusy(s_cardLoadLabel.c_str(), 0xFFD000u);
  std::string png;
  int status = pokedex::httpGet(url, png);
  if (status != 200 || png.empty()) {
    stopBusy();
    ESP_LOGE(TAG, "card GET %d (%u bytes)", status, (unsigned)png.size());
    return nullptr;
  }
  int64_t t0 = esp_timer_get_time();
  auto canvas = decodeCard(png);
  stopBusy();
  if (!canvas) return nullptr;
  ESP_LOGI(TAG, "decoded card %u bytes in %lldms (%u resident, free_heap=%u)",
           (unsigned)png.size(), (esp_timer_get_time() - t0) / 1000,
           (unsigned)s_card_cache.size() + 1,
           (unsigned)esp_get_free_heap_size());
  return s_card_cache.emplace(url, std::move(canvas)).first->second.get();
}

// Drop every cached card whose URL isn't in `keep`, bounding memory.
void keepOnlyCached(const std::set<std::string>& keep) {
  for (auto it = s_card_cache.begin(); it != s_card_cache.end();) {
    it = keep.count(it->first) ? std::next(it) : s_card_cache.erase(it);
  }
}

// Drop cached cards outside the prefetch window around `index`, bounding memory.
void evictOutsideCache(const std::vector<std::string>& urls, int index) {
  std::set<std::string> keep;
  for (int j : pokedex::prefetchWindow(index, static_cast<int>(urls.size()),
                                       pokedex::kPrefetchBack,
                                       pokedex::kPrefetchAhead)) {
    keep.insert(urls[j]);
  }
  keepOnlyCached(keep);
}

// Keep the current grid page and the next one resident (eight cards), so the
// idle prefetch of the next page survives until the user swipes to it.
void evictOutsideGrid(const std::vector<std::string>& urls, int page) {
  std::set<std::string> keep;
  int base = pokedex::cardIndexAt(page, 0);
  for (int i = base; i < base + 2 * pokedex::kGridCells &&
                     i < static_cast<int>(urls.size());
       ++i) {
    keep.insert(urls[i]);
  }
  keepOnlyCached(keep);
}

// Download + decode the single highest-priority not-yet-cached card in the
// window around `index`. Returns true if it fetched one (i.e. there is more
// buffering to do). Called when idle so the next swipe lands on an already
// resident card. loadCard() narrates the work as "Loading next card...".
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

// One idle buffering step, narrated on the bottom status strip: animate
// "Loading next card..." while a card downloads, then show "Next card loaded!"
// once the whole window around `index` is resident. `buffering` tracks whether a
// download happened so the "loaded" message shows exactly once per fill.
void prefetchTick(const std::vector<std::string>& urls, int index,
                  bool& buffering) {
  if (prefetchOne(urls, index)) {
    buffering = true;
  } else if (buffering) {
    showStatus("Next card loaded!", 0x30FF30u);
    buffering = false;
  }
}

// Show a card: blit its pre-decoded sprite (the fast path) or, on a cache miss,
// download + decode it first. The full-screen sprite includes the black
// background, so a cache hit is a single pushSprite with no clear/flash. On a
// miss we clear + show the counter so the user sees which card is loading.
bool showCardImage(const std::string& url, int index, int total) {
  if (s_card_cache.find(url) == s_card_cache.end()) {
    M5.Display.fillScreen(0x000000u);
    drawCounter(index, total);
  }
  M5Canvas* card = loadCard(url);
  if (!card) {
    M5.Display.fillScreen(0x000000u);
    drawCounter(index, total);
    return false;
  }
  int64_t t0 = esp_timer_get_time();
  card->pushSprite(&M5.Display, 0, 0);
  ESP_LOGI(TAG, "blit card in %lldms", (esp_timer_get_time() - t0) / 1000);
  drawCounter(index, total);  // dynamic, drawn live over the blitted card
  return true;
}

// Draw one of the rounded-rect deck buttons that overlay a card image, centring
// `label` in it. Matches the keyboard keys' look.
void drawDeckButton(pokedex::Point origin, pokedex::ImageSize s,
                    const char* label, uint32_t fill, int text_size) {
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
                 pokedex::kDeckButtonSize, pokedex::addButtonLabel(s_deck, url),
                 added ? 0x404040u : 0x107C10u, added ? 4 : 7);
}

// The "-" remove button, top-left, shown only while viewing the deck.
void drawRemoveButton() {
  drawDeckButton(pokedex::removeButtonOrigin(), pokedex::kDeckButtonSize, "-",
                 0xA01010u, 7);
}

// True if a release at `last` (with no swipe) was a tap inside the button
// rectangle at `origin` with size `size`.
bool tappedButton(pokedex::Point origin, pokedex::ImageSize size,
                  pokedex::Swipe swipe, pokedex::Point last) {
  return swipe == pokedex::Swipe::None &&
         pokedex::contains(origin, size, last);
}

// The "4 cards" / "1 card" view-toggle button, top-right, shown only while
// viewing the deck. It sits where the results browser's "+" button would be,
// which is free in the deck screen (that screen uses the top-left "-" instead).
void drawViewToggleButton(pokedex::DeckViewMode mode) {
  drawDeckButton(pokedex::viewToggleButtonOrigin(M5.Display.width()),
                 pokedex::kViewToggleButtonSize, pokedex::viewToggleLabel(mode),
                 0x303030u, 4);
}

// Draw one deck card scaled-to-fit, centred in a grid cell. Reuses the decoded
// full-screen sprite cache (so the prefetch/eviction machinery applies) and
// zoom-blits it down into the cell.
void drawCardInCell(const std::string& url, pokedex::Rect cell) {
  M5Canvas* card = loadCard(url);
  if (!card) return;
  float zx = static_cast<float>(cell.size.width) / card->width();
  float zy = static_cast<float>(cell.size.height) / card->height();
  float z = zx < zy ? zx : zy;
  card->pushRotateZoom(&M5.Display, cell.origin.x + cell.size.width / 2,
                       cell.origin.y + cell.size.height / 2, 0.0f, z, z);
}

// Render the 2x2 grid page for the deck: up to four cards from `page`, the
// "i / N" counter (i = the page's first card), then the "1 card" toggle on top.
// Keeps this page and the next resident so the prefetched page survives.
void showGridPage(const std::vector<std::string>& urls, int page) {
  M5.Display.fillScreen(0x000000u);
  auto cells = pokedex::gridCells(M5.Display.width(), M5.Display.height());
  int base = pokedex::cardIndexAt(page, 0);
  for (int c = 0; c < pokedex::kGridCells; ++c) {
    int idx = base + c;
    if (idx < static_cast<int>(urls.size())) drawCardInCell(urls[idx], cells[c]);
  }
  evictOutsideGrid(urls, page);
  drawCounter(base, static_cast<int>(urls.size()));  // "1 / N", "5 / N", ...
  drawViewToggleButton(pokedex::DeckViewMode::Grid);
}

// Download + decode the first not-yet-cached card of the page *after* `page`, so
// swiping forward in the grid lands on an already-resident page. Returns true if
// it fetched one (more buffering to do).
bool gridPrefetchOne(const std::vector<std::string>& urls, int page) {
  int next = pokedex::cardIndexAt(page + 1, 0);
  for (int i = next;
       i < next + pokedex::kGridCells && i < static_cast<int>(urls.size());
       ++i) {
    if (s_card_cache.find(urls[i]) == s_card_cache.end()) {
      loadCard(urls[i]);
      return true;
    }
  }
  return false;
}

// One idle buffering step for the grid, narrated as "Loading 4 cards..." then
// "Next 4 loaded!" once the following page is fully resident.
void gridPrefetchTick(const std::vector<std::string>& urls, int page,
                      bool& buffering) {
  if (gridPrefetchOne(urls, page)) {
    buffering = true;
  } else if (buffering) {
    showStatus("Next 4 loaded!", 0x30FF30u);
    buffering = false;
  }
}

// Show the result cards and let the user swipe: left/right between cards, up to
// return to the home screen. Tapping the top-right "+" adds the current card to
// the deck (button then reads "Added").
void runResultsBrowser(const std::vector<pokedex::Card>& cards) {
  s_cardLoadLabel = "Loading next card";
  const int total = static_cast<int>(cards.size());
  std::vector<std::string> urls;
  urls.reserve(total);
  for (const auto& c : cards) urls.push_back(c.url);
  const pokedex::Point add_origin =
      pokedex::addButtonOrigin(M5.Display.width());
  pokedex::CardBrowser browser(total);
  showCardImage(urls[0], 0, total);
  drawAddButton(urls[0]);
  evictOutsideCache(urls, 0);

  bool prev_down = false;
  bool buffering = false;
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
      if (tappedButton(add_origin, pokedex::kDeckButtonSize, swipe, last)) {
        s_deck.add(cards[browser.index()]);  // full metadata for type-sorting
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
          buffering = false;  // narrate buffering afresh for this card
        }
      }
    } else {
      // Idle (finger up): buffer the next card so the coming swipe is instant.
      prefetchTick(urls, browser.index(), buffering);
    }
    prev_down = down;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// One-time metadata backfill for legacy deck cards (saved before we captured
// type info). For each card missing a supertype we recover its pokemontcg.io id
// from the image URL, batch-query the API (in chunks to keep the URL short),
// and fill in name/supertype/subtype matched by image URL. Persisted so it runs
// at most once per card; silently leaves a card unsorted if the fetch fails
// (e.g. offline) so the deck still opens.
void backfillDeckMetadata() {
  // Map each card id -> its stored deck URL, for cards still missing metadata.
  // Matching fetched results back by id (not raw URL) is robust to any URL
  // formatting differences between what we saved and what the API returns.
  std::map<std::string, std::string> idToUrl;
  for (const auto& c : s_deck.records()) {
    // Refetch everything on a schema bump (to fill newly-added fields like
    // evolvesFrom); otherwise only cards with no metadata at all.
    if (!s_deckNeedsRefetch && !c.supertype.empty()) continue;
    std::string id = pokedex::cardIdFromImageUrl(c.url);
    if (!id.empty()) idToUrl[id] = c.url;
  }
  if (idToUrl.empty()) {
    if (s_deckNeedsRefetch) {  // nothing to fetch but schema still stale
      markDeckSchemaCurrent();
      s_deckNeedsRefetch = false;
    }
    return;
  }

  std::vector<std::string> ids;
  ids.reserve(idToUrl.size());
  for (const auto& kv : idToUrl) ids.push_back(kv.first);
  ESP_LOGI(TAG, "backfill: %u cards need metadata", (unsigned)ids.size());

  startBusy("Sorting deck", 0xFFD000u);
  int matched = 0;
  bool fetchOk = true;
  constexpr int kChunk = 15;  // keep the `id:a OR id:b ...` query URL short
  for (std::size_t i = 0; i < ids.size(); i += kChunk) {
    std::size_t end = i + kChunk < ids.size() ? i + kChunk : ids.size();
    std::vector<std::string> chunk(ids.begin() + i, ids.begin() + end);
    std::vector<pokedex::Card> cards;
    if (!pokedex::tcgFetchCards(pokedex::tcgIdQuery(chunk),
                                static_cast<int>(chunk.size()), cards)) {
      ESP_LOGW(TAG, "backfill: fetch failed for chunk at %u", (unsigned)i);
      fetchOk = false;
      continue;
    }
    for (const auto& fetched : cards) {
      std::string id = pokedex::cardIdFromImageUrl(fetched.url);
      auto it = idToUrl.find(id);
      if (it == idToUrl.end()) continue;
      pokedex::Card c = fetched;
      c.url = it->second;  // keep the deck's stored URL as the identity
      if (s_deck.update(c)) ++matched;
    }
  }
  stopBusy();
  ESP_LOGI(TAG, "backfill: filled %d/%u cards (fetchOk=%d)", matched,
           (unsigned)ids.size(), fetchOk);
  if (matched > 0) saveDeck();
  // Only advance the schema once the whole deck was fetched without error, so a
  // transient failure (e.g. offline) retries on the next open instead of
  // leaving cards permanently unsorted.
  if (fetchOk) {
    markDeckSchemaCurrent();
    s_deckNeedsRefetch = false;
  }
}

// Browse the deck. The cards are grouped by type (Pokémon first, alphabetical,
// then Supporter / Item / Other, Energy last). Two views, switched by the
// top-right "4 cards" / "1 card" button:
//  - Single: one card at a time. Swipe left/right between cards, up to go home.
//    Tapping the top-left "-" removes the current card (deck shrinks live; we
//    drop home once it empties).
//  - Grid: a 2x2 page of four cards. Swipe left/right to page, up to go home.
//    Tapping a card opens it full-screen (back to Single on that card).
void runDeckBrowser() {
  // Fill in type metadata for any legacy cards, then snapshot the deck sorted
  // by type; we browse the sorted URLs.
  backfillDeckMetadata();
  s_cardLoadLabel = "Loading 4 cards";  // the deck opens in the 2x2 grid view
  std::vector<pokedex::Card> sorted = pokedex::sortDeckByType(s_deck.records());
  std::vector<std::string> urls;
  urls.reserve(sorted.size());
  for (const auto& c : sorted) urls.push_back(c.url);
  if (urls.empty()) {
    M5.Display.fillScreen(0x000000u);
    showStatus("Deck is empty", 0xFFD000u);
    vTaskDelay(pdMS_TO_TICKS(1500));
    return;
  }
  const pokedex::Point remove_origin = pokedex::removeButtonOrigin();
  const pokedex::Point toggle_origin =
      pokedex::viewToggleButtonOrigin(M5.Display.width());
  pokedex::DeckViewMode mode = pokedex::DeckViewMode::Grid;  // open in grid view
  int index = 0;
  int page = 0;
  pokedex::CardBrowser browser(static_cast<int>(urls.size()));

  // Redraw the single-card view (image + remove + view-toggle buttons).
  auto drawSingle = [&]() {
    showCardImage(urls[index], index, static_cast<int>(urls.size()));
    drawRemoveButton();
    drawViewToggleButton(pokedex::DeckViewMode::Single);
    evictOutsideCache(urls, index);
  };
  // Rebuild the swipe browser so its index matches `index`.
  auto syncBrowser = [&]() {
    browser = pokedex::CardBrowser(static_cast<int>(urls.size()));
    for (int i = 0; i < index; ++i) browser.apply(pokedex::Swipe::Left);
  };

  showGridPage(urls, page);  // open in the 2x2 grid view

  bool prev_down = false;
  bool buffering = false;
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
      if (tappedButton(toggle_origin, pokedex::kViewToggleButtonSize, swipe,
                       last)) {
        mode = pokedex::toggle(mode);
        if (mode == pokedex::DeckViewMode::Grid) {
          s_cardLoadLabel = "Loading 4 cards";
          page = index / pokedex::kGridCells;
          showGridPage(urls, page);
        } else {
          s_cardLoadLabel = "Loading next card";
          // Land on the first card of the page we were viewing.
          index = pokedex::cardIndexAt(page, 0);
          if (index >= static_cast<int>(urls.size())) index = urls.size() - 1;
          syncBrowser();
          drawSingle();
        }
        buffering = false;
      } else if (mode == pokedex::DeckViewMode::Single) {
        if (tappedButton(remove_origin, pokedex::kDeckButtonSize, swipe,
                         last)) {
          s_deck.remove(urls[index]);
          saveDeck();
          urls.erase(urls.begin() + index);
          if (urls.empty()) return;  // deck emptied -> back home
          if (index >= static_cast<int>(urls.size())) index = urls.size() - 1;
          syncBrowser();
          drawSingle();
          buffering = false;
        } else {
          pokedex::BrowseAction act = browser.apply(swipe);
          if (act == pokedex::BrowseAction::GoHome) return;
          if (act == pokedex::BrowseAction::NextCard ||
              act == pokedex::BrowseAction::PrevCard) {
            index = browser.index();
            drawSingle();
            buffering = false;
          }
        }
      } else {  // Grid mode
        if (swipe == pokedex::Swipe::None) {
          int cell = pokedex::gridCellAt(M5.Display.width(),
                                         M5.Display.height(), last);
          int idx = cell >= 0 ? pokedex::cardIndexAt(page, cell) : -1;
          if (idx >= 0 && idx < static_cast<int>(urls.size())) {
            mode = pokedex::DeckViewMode::Single;
            s_cardLoadLabel = "Loading next card";
            index = idx;
            syncBrowser();
            drawSingle();
          }
        } else if (swipe == pokedex::Swipe::Up) {
          return;  // go home
        } else if (swipe == pokedex::Swipe::Left) {
          if (page + 1 < pokedex::gridPageCount(urls.size())) {
            ++page;
            showGridPage(urls, page);
          }
        } else if (swipe == pokedex::Swipe::Right) {
          if (page > 0) {
            --page;
            showGridPage(urls, page);
          }
        }
      }
    } else if (mode == pokedex::DeckViewMode::Single) {
      prefetchTick(urls, index, buffering);  // buffer the next card while idle
    } else {
      gridPrefetchTick(urls, page, buffering);  // buffer the next 4 while idle
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
  std::vector<pokedex::Card> cards;
  ok = pokedex::tcgFetchCards(query, kMaxCards, cards);
  stopBusy();
  if (!ok || cards.empty()) {
    showStatus("No cards found", 0xFF3030u);
    vTaskDelay(pdMS_TO_TICKS(1500));
    return;
  }
  ESP_LOGI(TAG, "found %u cards", (unsigned)cards.size());
  runResultsBrowser(cards);  // returns on swipe-up
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

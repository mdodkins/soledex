#include "net.h"

#include <cctype>
#include <cstring>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "pokedex/base64.h"
#include "pokedex/tcg.h"
#include "pokedex/wav.h"
#include "secrets.h"

namespace pokedex {
namespace {

const char* TAG = "net";

// Maps a spoken phrase to a pokemontcg.io Lucene query. Mirrors the host
// pipeline's prompt, including the "<type> energy" -> name:"..." fix.
const char* kSystem =
    "You convert a spoken Pokemon card search into a query for the "
    "pokemontcg.io v2 API `q` parameter (Lucene). Rules: a bare type -> "
    "types: (fairy -> types:Fairy). A supertype word -> supertype: (trainer "
    "-> supertype:Trainer; energy -> supertype:Energy). \"<type> energy\" -> "
    "name:\"<type> energy\" (fairy energy -> name:\"fairy energy\"). An effect "
    "keyword -> search card text (healing -> (attacks.text:heal* OR "
    "abilities.text:heal* OR rules:heal*)). A Pokemon name -> name:. Respond "
    "only via the structured output.";

// Accumulate response bodies (HTTP_EVENT_ON_DATA) into a std::string*.
esp_err_t collectEvt(esp_http_client_event_t* e) {
  if (e->event_id == HTTP_EVENT_ON_DATA && e->user_data) {
    static_cast<std::string*>(e->user_data)
        ->append(static_cast<const char*>(e->data), e->data_len);
  }
  return ESP_OK;
}

// One HTTP request. method "GET"/"POST"; headers as name/value pairs; optional
// body. Returns HTTP status (or -1), body in `resp`.
int httpDo(esp_http_client_method_t method, const std::string& url,
           const std::vector<std::pair<std::string, std::string>>& headers,
           const char* body, std::size_t body_len, std::string& resp) {
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.event_handler = collectEvt;
  cfg.user_data = &resp;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;  // HTTPS via cert bundle
  cfg.timeout_ms = 30000;
  cfg.buffer_size = 4096;
  cfg.buffer_size_tx = 2048;

  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (!c) return -1;
  esp_http_client_set_method(c, method);
  for (const auto& h : headers) {
    esp_http_client_set_header(c, h.first.c_str(), h.second.c_str());
  }
  if (body) esp_http_client_set_post_field(c, body, static_cast<int>(body_len));

  int status = -1;
  esp_err_t err = esp_http_client_perform(c);
  if (err == ESP_OK) {
    status = esp_http_client_get_status_code(c);
  } else {
    ESP_LOGE(TAG, "%s %s failed: %s", method == HTTP_METHOD_POST ? "POST" : "GET",
             url.c_str(), esp_err_to_name(err));
  }
  esp_http_client_cleanup(c);
  return status;
}

}  // namespace

int httpGet(const std::string& url, std::string& out) {
  out.clear();
  return httpDo(HTTP_METHOD_GET, url, {}, nullptr, 0, out);
}

bool sttTranscribe(const int16_t* pcm, std::size_t samples, uint32_t sampleRate,
                   std::string& text_out) {
  const std::size_t data_bytes = samples * sizeof(int16_t);
  auto header = wavHeader(static_cast<uint32_t>(data_bytes), sampleRate);

  const std::string boundary = "----pokedexBoundary";
  std::string body;
  body.reserve(data_bytes + 512);
  body += "--" + boundary +
          "\r\nContent-Disposition: form-data; name=\"file\"; "
          "filename=\"a.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  body.append(reinterpret_cast<const char*>(header.data()), header.size());
  body.append(reinterpret_cast<const char*>(pcm), data_bytes);
  body += "\r\n--" + boundary +
          "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\n"
          "json\r\n--" + boundary + "--\r\n";

  // The STT endpoint is fronted by HTTPS basic auth when served over the
  // internet (STT_AUTH = "user:pass"). On a no-auth LAN box STT_AUTH is empty
  // and basicAuthHeader() returns "", so we send no Authorization header.
  std::vector<std::pair<std::string, std::string>> headers = {
      {"Content-Type", "multipart/form-data; boundary=" + boundary}};
  if (const std::string auth = basicAuthHeader(STT_AUTH); !auth.empty()) {
    headers.push_back({"Authorization", auth});
  }

  std::string resp;
  int status = httpDo(HTTP_METHOD_POST, STT_URL, headers, body.data(),
                      body.size(), resp);
  if (status != 200) {
    ESP_LOGE(TAG, "STT HTTP %d", status);
    return false;
  }
  cJSON* root = cJSON_Parse(resp.c_str());
  if (!root) return false;
  cJSON* text = cJSON_GetObjectItem(root, "text");
  bool ok = cJSON_IsString(text);
  if (ok) {
    text_out = text->valuestring;
    // Trim leading/trailing whitespace whisper sometimes adds.
    while (!text_out.empty() && std::isspace((unsigned char)text_out.front()))
      text_out.erase(text_out.begin());
    while (!text_out.empty() && std::isspace((unsigned char)text_out.back()))
      text_out.pop_back();
  }
  cJSON_Delete(root);
  return ok;
}

bool claudeQuery(const std::string& transcript, std::string& query_out) {
  // Build the request with cJSON so the transcript is escaped correctly.
  cJSON* req = cJSON_CreateObject();
  cJSON_AddStringToObject(req, "model", "claude-opus-4-8");
  cJSON_AddNumberToObject(req, "max_tokens", 1024);
  cJSON_AddStringToObject(req, "system", kSystem);

  cJSON* schema = cJSON_CreateObject();
  cJSON_AddStringToObject(schema, "type", "object");
  cJSON* props = cJSON_AddObjectToObject(schema, "properties");
  cJSON* q = cJSON_AddObjectToObject(props, "q");
  cJSON_AddStringToObject(q, "type", "string");
  cJSON* expl = cJSON_AddObjectToObject(props, "explanation");
  cJSON_AddStringToObject(expl, "type", "string");
  cJSON* required = cJSON_AddArrayToObject(schema, "required");
  cJSON_AddItemToArray(required, cJSON_CreateString("q"));
  cJSON_AddItemToArray(required, cJSON_CreateString("explanation"));
  cJSON_AddBoolToObject(schema, "additionalProperties", false);
  cJSON* fmt = cJSON_CreateObject();
  cJSON_AddStringToObject(fmt, "type", "json_schema");
  cJSON_AddItemToObject(fmt, "schema", schema);
  cJSON* output_config = cJSON_AddObjectToObject(req, "output_config");
  cJSON_AddItemToObject(output_config, "format", fmt);

  cJSON* messages = cJSON_AddArrayToObject(req, "messages");
  cJSON* msg = cJSON_CreateObject();
  cJSON_AddStringToObject(msg, "role", "user");
  cJSON_AddStringToObject(msg, "content", transcript.c_str());
  cJSON_AddItemToArray(messages, msg);

  char* body = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);
  if (!body) return false;

  std::string resp;
  int status = httpDo(HTTP_METHOD_POST, "https://api.anthropic.com/v1/messages",
                      {{"x-api-key", ANTHROPIC_API_KEY},
                       {"anthropic-version", "2023-06-01"},
                       {"content-type", "application/json"}},
                      body, std::strlen(body), resp);
  cJSON_free(body);
  if (status != 200) {
    ESP_LOGE(TAG, "Claude HTTP %d: %.200s", status, resp.c_str());
    return false;
  }

  // response.content[] -> first text block -> its text is the JSON {"q":...}
  bool ok = false;
  cJSON* root = cJSON_Parse(resp.c_str());
  if (root) {
    cJSON* content = cJSON_GetObjectItem(root, "content");
    cJSON* block = nullptr;
    cJSON_ArrayForEach(block, content) {
      cJSON* type = cJSON_GetObjectItem(block, "type");
      if (cJSON_IsString(type) && std::strcmp(type->valuestring, "text") == 0) {
        cJSON* text = cJSON_GetObjectItem(block, "text");
        if (cJSON_IsString(text)) {
          cJSON* inner = cJSON_Parse(text->valuestring);
          cJSON* qv = inner ? cJSON_GetObjectItem(inner, "q") : nullptr;
          if (cJSON_IsString(qv)) {
            query_out = qv->valuestring;
            ok = true;
          }
          if (inner) cJSON_Delete(inner);
        }
        break;
      }
    }
    cJSON_Delete(root);
  }
  if (!ok) {
    ESP_LOGE(TAG, "Claude 200 but no q extracted: %.300s", resp.c_str());
  }
  return ok;
}

bool tcgFetchImageUrls(const std::string& query, int limit,
                       std::vector<std::string>& urls_out) {
  std::string url = tcgQueryUrl(query, limit);
  ESP_LOGI(TAG, "TCG GET %s", url.c_str());
  std::string resp;
  int status = httpGet(url, resp);
  ESP_LOGI(TAG, "TCG status=%d bytes=%u free_heap=%u", status,
           (unsigned)resp.size(), (unsigned)esp_get_free_heap_size());
  if (status != 200) {
    ESP_LOGE(TAG, "TCG HTTP %d body=%.200s", status, resp.c_str());
    return false;
  }
  cJSON* root = cJSON_Parse(resp.c_str());
  if (!root) {
    ESP_LOGE(TAG, "TCG JSON parse failed (%u bytes): %.200s",
             (unsigned)resp.size(), resp.c_str());
    return false;
  }
  cJSON* data = cJSON_GetObjectItem(root, "data");
  int n = 0;
  cJSON* card = nullptr;
  cJSON_ArrayForEach(card, data) {
    cJSON* images = cJSON_GetObjectItem(card, "images");
    cJSON* large = images ? cJSON_GetObjectItem(images, "large") : nullptr;
    if (cJSON_IsString(large)) {
      urls_out.emplace_back(large->valuestring);
      ++n;
    }
  }
  ESP_LOGI(TAG, "TCG parsed %d image urls", n);
  cJSON_Delete(root);
  return true;
}

}  // namespace pokedex

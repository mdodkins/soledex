# Generates firmware/main/secrets.h from the git-ignored secret files at the
# project root, at CMake configure time. The generated header is itself
# git-ignored. Re-runs on every configure, so editing a secret file then
# rebuilding picks up the change.

set(_root "${CMAKE_CURRENT_LIST_DIR}/../..")        # firmware/cmake -> repo root
set(_out  "${CMAKE_CURRENT_LIST_DIR}/../main/secrets.h")

# --- Anthropic API key ---
if(NOT EXISTS "${_root}/claude-api-key.txt")
  message(FATAL_ERROR "Missing claude-api-key.txt at project root")
endif()
file(READ "${_root}/claude-api-key.txt" _api)
string(STRIP "${_api}" _api)

# --- WiFi SSID,password (first line of wifi-passwords.txt) ---
if(NOT EXISTS "${_root}/wifi-passwords.txt")
  message(FATAL_ERROR "Missing wifi-passwords.txt at project root")
endif()
file(STRINGS "${_root}/wifi-passwords.txt" _wifi_lines)
list(GET _wifi_lines 0 _line)
string(FIND "${_line}" "," _comma)
string(SUBSTRING "${_line}" 0 ${_comma} _ssid)
math(EXPR _pw_start "${_comma}+1")
string(SUBSTRING "${_line}" ${_pw_start} -1 _pw)

# --- STT endpoint (optional stt-url.txt, else the hosted internet endpoint) ---
if(EXISTS "${_root}/stt-url.txt")
  file(READ "${_root}/stt-url.txt" _stt)
  string(STRIP "${_stt}" _stt)
else()
  set(_stt "https://dev.uoawen.com/stt/inference")
endif()

# --- STT basic-auth (optional stt-auth.txt = "user:password") ---
# The hosted endpoint sits behind nginx basic auth; a no-auth LAN box leaves
# this empty and the firmware sends no Authorization header.
if(EXISTS "${_root}/stt-auth.txt")
  file(READ "${_root}/stt-auth.txt" _stt_auth)
  string(STRIP "${_stt_auth}" _stt_auth)
else()
  set(_stt_auth "")
endif()

file(WRITE "${_out}"
"// AUTO-GENERATED from git-ignored secret files by cmake/gen_secrets.cmake.
// Do NOT commit — this file is in .gitignore.
#pragma once
#define WIFI_SSID \"${_ssid}\"
#define WIFI_PASSWORD \"${_pw}\"
#define ANTHROPIC_API_KEY \"${_api}\"
#define STT_URL \"${_stt}\"
#define STT_AUTH \"${_stt_auth}\"
")
message(STATUS "Generated secrets.h (SSID='${_ssid}', STT_URL='${_stt}')")

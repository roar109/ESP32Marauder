#include "ResetDiag.h"
#include "configs.h"
#include "WiFiScan.h"

#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#ifdef HAS_SD
  #include <SD.h>
  #include "SDInterface.h"
  extern SDInterface sd_obj;
#endif

namespace {

struct RtcSnapshot {
  uint32_t magic;
  uint32_t uptime_s;
  uint32_t free_heap;
  uint32_t largest_block;
  uint32_t min_free_heap;
  uint32_t scan_mode;
  uint32_t check;
};

constexpr uint32_t kMagic = 0x4D52444Du;
constexpr uint32_t kTickMs = 5000;

RTC_NOINIT_ATTR RtcSnapshot g_snapshot;

String g_reason = "UNKNOWN";
String g_log_line = "";
bool g_abnormal = false;
uint32_t g_last_tick_ms = 0;
bool g_ticked = false;

uint32_t snapshotCheck(const RtcSnapshot& s) {
  return s.magic ^ (s.uptime_s * 31u) ^ (s.free_heap * 17u) ^ (s.largest_block * 13u) ^
         (s.min_free_heap * 7u) ^ (s.scan_mode * 3u);
}

const char* reasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:   return "UNKNOWN";
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXTERNAL";
    case ESP_RST_SW:        return "SOFTWARE";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "OTHER";
  }
}

}  // namespace

void resetDiagBegin() {
  const esp_reset_reason_t reason = esp_reset_reason();
  g_reason = reasonName(reason);
  g_abnormal = (reason == ESP_RST_PANIC) || (reason == ESP_RST_INT_WDT) ||
               (reason == ESP_RST_TASK_WDT) || (reason == ESP_RST_WDT) ||
               (reason == ESP_RST_BROWNOUT);

  g_log_line = "reset=" + g_reason + " (" + String((int)reason) + ")";

  const bool valid = (g_snapshot.magic == kMagic) && (g_snapshot.check == snapshotCheck(g_snapshot));
  if (valid && (reason != ESP_RST_POWERON)) {
    g_log_line += " prev_uptime_s=" + String(g_snapshot.uptime_s);
    g_log_line += " prev_scan_mode=" + String(g_snapshot.scan_mode) +
                  ((g_snapshot.scan_mode == WIFI_SCAN_WAR_DRIVE) ? "(WARDRIVE)" : "");
    g_log_line += " prev_free_heap=" + String(g_snapshot.free_heap);
    g_log_line += " prev_largest_block=" + String(g_snapshot.largest_block);
    g_log_line += " prev_min_free_heap=" + String(g_snapshot.min_free_heap);
  }

  // Invalidate until the first tick of this run, so a reset during boot does
  // not report stale numbers from two runs ago.
  g_snapshot.magic = 0;

  Serial.println();
  Serial.println("[RESET] " + g_log_line + (g_abnormal ? "  <-- abnormal reset" : ""));
}

void resetDiagTick(uint32_t now_ms, uint8_t scan_mode) {
  if (g_ticked && (now_ms - g_last_tick_ms < kTickMs))
    return;
  g_ticked = true;
  g_last_tick_ms = now_ms;

  g_snapshot.uptime_s = now_ms / 1000;
  g_snapshot.free_heap = (uint32_t)ESP.getFreeHeap();
  g_snapshot.largest_block = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  g_snapshot.min_free_heap = (uint32_t)ESP.getMinFreeHeap();
  g_snapshot.scan_mode = scan_mode;
  g_snapshot.magic = kMagic;
  g_snapshot.check = snapshotCheck(g_snapshot);
}

void resetDiagLogToSd() {
  #ifdef HAS_SD
    if (!sd_obj.supported)
      return;
    File log_file = SD.open("/reset_log.txt", FILE_APPEND);
    if (!log_file)
      return;
    log_file.println(g_log_line);
    log_file.close();
  #endif
}

const char* resetDiagReason() {
  return g_reason.c_str();
}

bool resetDiagAbnormal() {
  return g_abnormal;
}

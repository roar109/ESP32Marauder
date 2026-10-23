#pragma once

#ifndef ResetDiag_h
#define ResetDiag_h

#include <Arduino.h>

// Reboot diagnostics. The reset reason of the previous run is printed on
// serial at boot and appended to /reset_log.txt on the SD card, together with
// the last uptime / heap snapshot the previous run stored in RTC memory (which
// survives panic and watchdog resets, but not a power cycle).

// Call once, early in setup() after Serial.begin().
void resetDiagBegin();

// Call from loop(). Cheap; refreshes the RTC snapshot every few seconds.
void resetDiagTick(uint32_t now_ms, uint8_t scan_mode);

// Call once the SD card is initialised.
void resetDiagLogToSd();

const char* resetDiagReason();
bool resetDiagAbnormal();

#endif

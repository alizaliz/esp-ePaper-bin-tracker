#pragma once

// Answers the config page (docs/config/index.html) over the USB serial port.
//
// The page sends one JSON command per line, e.g. {"cmd":"get"}. Replies are
// single lines starting with "@@CFG " so the page can tell them apart from
// the log output on the same port (readers should look for the marker
// anywhere in a line, in case log output runs into it). Commands:
//   hello            -> {"ok":true,"device":"esp-ePaper-bin-tracker"}
//   get              -> {"ok":true,"settings":{...}}  (no Wi-Fi password)
//   set + settings   -> validates and saves; {"ok":true} or {"ok":false,"error":"..."}
//   reset            -> forgets saved settings, back to the config.h defaults
//   restart          -> replies, then restarts so new settings take effect
namespace config_service {

// Starts listening if a computer is connected over USB. Does nothing on
// battery or a USB charger.
void start();

// Stops routing console output through the USB driver, so log output can't
// block once the computer is gone. Call before deep sleep.
void stop();

}  // namespace config_service

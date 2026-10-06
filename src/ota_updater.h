#pragma once

#include "esp_err.h"

// Over-the-air firmware updates from the project's GitHub releases.
//
// The board runs from one of two app slots and downloads an update into the
// other. New firmware starts "pending": it must call markHealthy() (after a
// successful refresh) before the next restart, or the bootloader goes back to
// the previous version.
namespace ota {

// The running firmware's version, e.g. "v1.1.0", or a commit hash for a
// development build.
const char* currentVersion();

// True if this is the first run of newly installed firmware that hasn't been
// confirmed yet.
bool isPendingVerify();

// Confirms newly installed firmware, cancelling the rollback.
void markHealthy();

// Checks the latest release and installs it if it's newer than the running
// firmware. Needs a network connection. Returns ESP_OK if an update was
// installed (the caller should restart), ESP_ERR_NOT_FOUND if there's nothing
// to install, or another error if the check or download failed.
//
// Development builds (not a vX.Y.Z version) are only updated when `force` is
// true, so a build flashed from a computer isn't replaced overnight.
esp_err_t checkAndInstall(bool force);

}  // namespace ota

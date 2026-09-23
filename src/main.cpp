#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {
  printf("ESP32-C6 ePaper tracker booting...\n");

  while (1) {
    printf("Heartbeat\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

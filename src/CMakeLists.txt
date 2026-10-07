FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c ${CMAKE_SOURCE_DIR}/src/*.cpp)

idf_component_register(
  SRCS ${app_sources}
  INCLUDE_DIRS "."
  REQUIRES esp_adc esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_usb_serial_jtag esp_lcd esp_timer
           esp_wifi esp_netif esp_event esp_http_client esp_https_ota app_update mbedtls nvs_flash lvgl fonts espressif__cjson)

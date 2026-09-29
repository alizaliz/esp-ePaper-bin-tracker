FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c ${CMAKE_SOURCE_DIR}/src/*.cpp)

# PNGs in src/assets/ are linked into the firmware; see src/assets.h.
FILE(GLOB asset_files ${CMAKE_SOURCE_DIR}/src/assets/*.png)

idf_component_register(
  SRCS ${app_sources}
  INCLUDE_DIRS "."
  EMBED_FILES ${asset_files}
  REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi esp_lcd esp_timer lvgl)

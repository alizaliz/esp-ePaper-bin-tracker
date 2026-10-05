#include "battery.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"

namespace battery {
namespace {

constexpr const char* TAG = "battery";
constexpr adc_channel_t CHANNEL = ADC_CHANNEL_0;  // GPIO0 (BAT_ADC)
constexpr float DIVIDER = 2.0f;                    // 200k / 200k
constexpr int SAMPLES = 16;

// Resting voltage to charge for a typical lithium polymer cell.
struct Point {
  float volts;
  int percent;
};
constexpr Point CURVE[] = {
    {4.20f, 100}, {4.10f, 90}, {4.00f, 79}, {3.90f, 66}, {3.80f, 52}, {3.75f, 41},
    {3.70f, 31},  {3.65f, 20}, {3.60f, 12}, {3.50f, 5},  {3.30f, 0},
};

}  // namespace

esp_err_t readVoltage(float& volts) {
  adc_oneshot_unit_handle_t adc = nullptr;
  adc_oneshot_unit_init_cfg_t unit_config = {};
  unit_config.unit_id = ADC_UNIT_1;
  ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &adc), TAG, "ADC init failed");

  adc_oneshot_chan_cfg_t channel_config = {};
  channel_config.atten = ADC_ATTEN_DB_12;  // up to ~3.1V at the pin
  channel_config.bitwidth = ADC_BITWIDTH_DEFAULT;
  esp_err_t err = adc_oneshot_config_channel(adc, CHANNEL, &channel_config);

  adc_cali_handle_t cali = nullptr;
  adc_cali_curve_fitting_config_t cali_config = {};
  cali_config.unit_id = ADC_UNIT_1;
  cali_config.chan = CHANNEL;
  cali_config.atten = ADC_ATTEN_DB_12;
  cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;
  if (err == ESP_OK) {
    err = adc_cali_create_scheme_curve_fitting(&cali_config, &cali);
  }

  int total_mv = 0;
  for (int i = 0; i < SAMPLES && err == ESP_OK; ++i) {
    int raw = 0;
    int mv = 0;
    err = adc_oneshot_read(adc, CHANNEL, &raw);
    if (err == ESP_OK) err = adc_cali_raw_to_voltage(cali, raw, &mv);
    total_mv += mv;
  }

  if (cali != nullptr) adc_cali_delete_scheme_curve_fitting(cali);
  adc_oneshot_del_unit(adc);
  ESP_RETURN_ON_ERROR(err, TAG, "ADC read failed");

  volts = DIVIDER * total_mv / SAMPLES / 1000.0f;
  return ESP_OK;
}

int percentFromVoltage(float volts) {
  constexpr int last = sizeof(CURVE) / sizeof(CURVE[0]) - 1;
  if (volts >= CURVE[0].volts) return 100;
  if (volts <= CURVE[last].volts) return 0;
  for (int i = 1; i <= last; ++i) {
    if (volts >= CURVE[i].volts) {
      const Point& hi = CURVE[i - 1];
      const Point& lo = CURVE[i];
      const float t = (volts - lo.volts) / (hi.volts - lo.volts);
      return lo.percent + static_cast<int>(t * (hi.percent - lo.percent) + 0.5f);
    }
  }
  return 0;
}

}  // namespace battery

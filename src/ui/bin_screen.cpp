#include "bin_screen.h"

#include <algorithm>
#include <cstdio>

#include "fonts.h"

namespace {

constexpr int COLUMN_SPACING = 66;  // centre-to-centre distance of the bin columns

lv_obj_t* addLabel(lv_obj_t* parent, const char* text, const lv_font_t* font) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  return label;
}

// Icon followed by a reading, e.g. [thermometer] 21°C
lv_obj_t* addReading(lv_obj_t* parent, const char* icon, const char* value) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 3, 0);
  addLabel(row, icon, &icons_22);
  addLabel(row, value, &font_bold_18);
  return row;
}

// Wi-Fi icon, struck through with a diagonal slash when offline. Font
// Awesome Free has no "wifi off" icon, so the slash is drawn with lines: a
// thick white line to cut a gap through the icon, then a thin black one.
void addWifiStatus(lv_obj_t* parent, bool connected) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, 28, 24);
  lv_obj_t* icon = addLabel(box, ICON_WIFI, &icons_22);
  lv_obj_center(icon);
  if (connected) return;

  static lv_point_precise_t slash[] = {{3, 2}, {25, 22}};
  const struct {
    lv_color_t color;
    int width;
  } strokes[] = {{lv_color_white(), 7}, {lv_color_black(), 3}};
  for (const auto& stroke : strokes) {
    lv_obj_t* line = lv_line_create(box);
    lv_line_set_points(line, slash, 2);
    lv_obj_set_style_line_color(line, stroke.color, 0);
    lv_obj_set_style_line_width(line, stroke.width, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
  }
}

// Battery outline whose fill is proportional to the charge. Drawn rather than
// taken from Font Awesome, which only has five fixed levels.
void addBattery(lv_obj_t* parent, bool known, int percent) {
  constexpr int BODY_W = 22;
  constexpr int BODY_H = 12;
  constexpr int BORDER = 2;
  constexpr int GAP = 1;  // white gap between the outline and the fill

  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, BODY_W + 3, BODY_H);

  lv_obj_t* body = lv_obj_create(box);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, BODY_W, BODY_H);
  lv_obj_set_style_border_color(body, lv_color_black(), 0);
  lv_obj_set_style_border_width(body, BORDER, 0);
  lv_obj_set_style_border_opa(body, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(body, 2, 0);

  lv_obj_t* nub = lv_obj_create(box);
  lv_obj_remove_style_all(nub);
  lv_obj_set_size(nub, 3, 6);
  lv_obj_set_pos(nub, BODY_W, (BODY_H - 6) / 2);
  lv_obj_set_style_bg_color(nub, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);

  if (!known) return;
  constexpr int INNER_W = BODY_W - 2 * (BORDER + GAP);
  const int fill_w = (std::clamp(percent, 0, 100) * INNER_W + 50) / 100;
  if (fill_w == 0) return;
  lv_obj_t* fill = lv_obj_create(box);
  lv_obj_remove_style_all(fill);
  lv_obj_set_size(fill, fill_w, BODY_H - 2 * (BORDER + GAP));
  lv_obj_set_pos(fill, BORDER + GAP, BORDER + GAP);
  lv_obj_set_style_bg_color(fill, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
}

// Top row, centred: Wi-Fi status, temperature, humidity, battery.
void addStatusBar(lv_obj_t* screen, const BinScreenData& data) {
  char temperature[12];
  char humidity[8];
  if (data.hasClimate) {
    // Clamp so both readings stay two digits wide.
    snprintf(temperature, sizeof(temperature), "%d\xC2\xB0" "C",
             std::clamp(data.temperatureC, -9, 99));
    snprintf(humidity, sizeof(humidity), "%d%%", std::clamp(data.humidityPct, 0, 99));
  } else {
    snprintf(temperature, sizeof(temperature), "--\xC2\xB0" "C");
    snprintf(humidity, sizeof(humidity), "--%%");
  }

  lv_obj_t* bar = lv_obj_create(screen);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bar, 8, 0);
  addWifiStatus(bar, data.wifiConnected);
  addReading(bar, ICON_THERMOMETER, temperature);
  addReading(bar, ICON_DROPLET, humidity);
  addBattery(bar, data.hasBattery, data.batteryPct);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 5);
}

void addPickupDay(lv_obj_t* screen, const BinScreenData& data) {
  static const char* const DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

  lv_obj_t* banner = lv_obj_create(screen);
  lv_obj_remove_style_all(banner);
  lv_obj_set_size(banner, LV_PCT(100), 54);
  lv_obj_align(banner, LV_ALIGN_TOP_MID, 0, 38);

  lv_obj_t* label;
  if (data.isToday || data.isTonight) {
    // Evening before (put the bins out) or the pickup day itself: white text
    // on a solid black strip
    lv_obj_set_style_bg_color(banner, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
    label = addLabel(banner, data.isToday ? "TODAY" : "TONIGHT", &font_bold_32);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
  } else {
    char text[12];
    snprintf(text, sizeof(text), "%s %d", DAYS[dayOfWeek(data.pickup)], data.pickup.day);
    label = addLabel(banner, text, &font_bold_40);
  }
  lv_obj_center(label);
}

void addBinColumn(lv_obj_t* screen, int column, const char* icon, bool collected) {
  const int x_offset = (column - 1) * COLUMN_SPACING;

  lv_obj_t* bin = addLabel(screen, icon, &icons_52);
  lv_obj_align(bin, LV_ALIGN_TOP_MID, x_offset, 104);

  lv_obj_t* mark = addLabel(screen, collected ? ICON_CHECK : ICON_XMARK, &icons_30);
  lv_obj_align(mark, LV_ALIGN_TOP_MID, x_offset, 164);
}

}  // namespace

lv_obj_t* createBinScreen(const BinScreenData& data) {
  lv_obj_t* screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_black(), 0);
  lv_obj_set_scrollable(screen, false);

  addStatusBar(screen, data);
  addPickupDay(screen, data);
  addBinColumn(screen, 0, ICON_TRASH_CAN, data.rubbish);
  addBinColumn(screen, 1, ICON_RECYCLE, data.recycling);
  addBinColumn(screen, 2, ICON_APPLE, data.foodScraps);
  return screen;
}

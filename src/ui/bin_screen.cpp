#include "bin_screen.h"

#include <algorithm>
#include <cstdio>

#include "doodles.h"
#include "fonts.h"

namespace {

// Layout, top to bottom: a hand-drawn speech bubble with the pickup day and
// the bins going out, then Binny "saying" it, with the midday forecast beside
// it. Everything is a pen doodle or handwriting.
constexpr int BUBBLE_X = 8, BUBBLE_Y = 4;     // 184x108 image, tail included
constexpr int CENTRE_X = BUBBLE_X + 92;       // centre of the bubble's body
constexpr int BIN_STEP = 42;                  // between bin icons (34px)
constexpr int MASCOT_X = 8, MASCOT_Y = 112;   // 84px image
constexpr int WEATHER_X = 112, WEATHER_Y = 116;  // 84x70 image
constexpr int STATUS_X = 90, STATUS_Y = 116, STATUS_STEP = 25;  // 22px icons, between Binny and the weather

const char* const DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

lv_obj_t* addLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                   lv_color_t color = lv_color_black()) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  return label;
}

lv_obj_t* addImage(lv_obj_t* parent, const lv_image_dsc_t* src, int x, int y) {
  lv_obj_t* image = lv_image_create(parent);
  lv_image_set_src(image, src);
  lv_obj_set_pos(image, x, y);
  return image;
}

int widthOf(lv_obj_t* obj) {
  lv_obj_update_layout(obj);
  return lv_obj_get_width(obj);
}

// Height of capital letters above the baseline. Handwriting fonts leave a lot
// of room above the capitals, so rows are stacked by their ink rather than
// their line boxes. Roughly right for Gochi Hand.
int capHeight(const lv_font_t* font) {
  return (font->line_height - font->base_line) * 72 / 100;
}

// Places a line of text centred on centre_x with the top of its capitals at
// cap_top. Returns the y just below the baseline.
int placeText(lv_obj_t* label, int centre_x, int cap_top) {
  const lv_font_t* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
  const int ascent = font->line_height - font->base_line;
  lv_obj_set_pos(label, centre_x - widthOf(label) / 2, cap_top - (ascent - capHeight(font)));
  return cap_top + capHeight(font);
}

const lv_image_dsc_t* mascotFace(const BinScreenData& data) {
  if (!data.wifiConnected || data.isStale) return &doodle_face_worried;
  if (data.batteryLow) return &doodle_face_sleepy;
  if (data.isTonight) return &doodle_face_excited;
  if (data.isToday) return &doodle_face_proud;
  // Ordinary days alternate, so the face changes daily.
  return data.dayNumber % 2 ? &doodle_face_wink : &doodle_face_happy;
}

const lv_image_dsc_t* weatherIcon(Sky sky) {
  switch (sky) {
    case Sky::Clear: return &doodle_weather_clear;
    case Sky::PartlyCloudy: return &doodle_weather_partly_cloudy;
    case Sky::Cloudy: return &doodle_weather_cloudy;
    case Sky::Fog: return &doodle_weather_fog;
    case Sky::Drizzle: return &doodle_weather_drizzle;
    case Sky::Rain: return &doodle_weather_rain;
    case Sky::Snow: return &doodle_weather_snow;
    case Sky::Thunder: return &doodle_weather_thunder;
    default: return nullptr;
  }
}

void addBubble(lv_obj_t* screen, const BinScreenData& data) {
  const bool loud = data.isTonight || data.isToday;
  const lv_color_t ink = loud ? lv_color_white() : lv_color_black();
  addImage(screen, loud ? &doodle_bubble_loud : &doodle_bubble, BUBBLE_X, BUBBLE_Y);

  int bins_y;
  if (loud) {
    lv_obj_t* word = addLabel(screen, data.isTonight ? "TONIGHT" : "TODAY", &font_hand_38, ink);
    bins_y = placeText(word, CENTRE_X, BUBBLE_Y + 14) + 12;
  } else {
    char day[12];
    snprintf(day, sizeof(day), "%s %d", DAYS[dayOfWeek(data.pickup)], data.pickup.day);
    lv_obj_t* intro = addLabel(screen, data.isStale ? "Last known" : "Bins out", &font_hand_20, ink);
    const int below = placeText(intro, CENTRE_X, BUBBLE_Y + 9);
    bins_y = placeText(addLabel(screen, day, &font_hand_42, ink), CENTRE_X, below + 7) + 8;
  }

  // The bins going out, in a centred row.
  const lv_image_dsc_t* bins[3];
  int count = 0;
  if (data.rubbish) bins[count++] = &doodle_bin_rubbish;
  if (data.recycling) bins[count++] = &doodle_bin_recycling;
  if (data.foodScraps) bins[count++] = &doodle_bin_food;
  for (int i = 0; i < count; ++i) {
    const int centre = CENTRE_X + (2 * i - (count - 1)) * BIN_STEP / 2;
    lv_obj_t* bin = addImage(screen, bins[i], centre - bins[i]->header.w / 2, bins_y);
    lv_obj_set_style_image_recolor(bin, ink, 0);
    lv_obj_set_style_image_recolor_opa(bin, LV_OPA_COVER, 0);
  }
}

// Status icons, stacked in the gap between Binny and the weather, only when
// something needs attention. Returns how many were drawn.
int addStatus(lv_obj_t* screen, const BinScreenData& data) {
  int count = 0;
  auto add = [&](const lv_image_dsc_t* icon) {
    addImage(screen, icon, STATUS_X, STATUS_Y + count++ * STATUS_STEP);
  };
  if (!data.wifiConnected) add(&doodle_status_offline);
  if (data.batteryLow) add(&doodle_status_battery);
  if (data.isStale) add(&doodle_status_stale);
  return count;
}

// The midday forecast: an icon with the temperature written small over its
// lower left. Without a forecast, the indoor temperature, large.
void addWeather(lv_obj_t* screen, const BinScreenData& data, int status_icons) {
  char temperature[12];
  if (data.hasTemperature) {
    snprintf(temperature, sizeof(temperature), "%d\xC2\xB0", std::clamp(data.temperatureC, -9, 99));
  } else {
    snprintf(temperature, sizeof(temperature), "--\xC2\xB0");
  }

  const lv_image_dsc_t* icon = weatherIcon(data.sky);
  if (icon == nullptr) {
    constexpr int centre = WEATHER_X + 42;
    const int below = placeText(addLabel(screen, temperature, &font_hand_42), centre, WEATHER_Y + 12);
    if (data.hasTemperature) placeText(addLabel(screen, "inside", &font_hand_20), centre, below + 10);
    return;
  }

  addImage(screen, icon, WEATHER_X, WEATHER_Y);
  // A white patch behind the text keeps it readable over the drawing.
  lv_obj_t* label = addLabel(screen, temperature, &font_hand_24);
  lv_obj_set_style_bg_color(label, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(label, 3, 0);
  lv_obj_set_style_radius(label, 6, 0);
  // Offset left of the icon so most of it stays visible, unless a third
  // status icon sits there.
  lv_obj_set_pos(label, status_icons >= 3 ? WEATHER_X + 3 : WEATHER_X - 8, WEATHER_Y + 46);
}

}  // namespace

lv_obj_t* createBinScreen(const BinScreenData& data) {
  lv_obj_t* screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_black(), 0);
  lv_obj_set_scrollable(screen, false);

  addBubble(screen, data);
  addImage(screen, mascotFace(data), MASCOT_X, MASCOT_Y);
  addWeather(screen, data, addStatus(screen, data));
  return screen;
}

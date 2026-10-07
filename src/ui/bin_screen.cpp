#include "bin_screen.h"

#include <algorithm>
#include <cstdio>

#include "fonts.h"
#include "mascot.h"

namespace {

// Layout, top to bottom: a speech bubble with the pickup day and the bins
// going out, then the mascot "saying" it, with the weather beside it.
constexpr int BUBBLE_X = 8, BUBBLE_Y = 10, BUBBLE_W = 184, BUBBLE_H = 92;
constexpr int TAIL_X = 50;                  // where the tail meets the bubble
constexpr int TAIL_HALF_W = 10;
constexpr int TAIL_TIP_Y = BUBBLE_Y + BUBBLE_H + 16;
constexpr int MASCOT_X = 10, MASCOT_Y = 110;  // 84px image
constexpr int SIDE_RIGHT = 190;               // right edge of the weather column

const char* const DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

lv_obj_t* addLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                   lv_color_t color = lv_color_black()) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  return label;
}

int widthOf(lv_obj_t* obj) {
  lv_obj_update_layout(obj);
  return lv_obj_get_width(obj);
}

void placeCentred(lv_obj_t* obj, int centre_x, int y) {
  lv_obj_set_pos(obj, centre_x - widthOf(obj) / 2, y);
}

void addLine(lv_obj_t* parent, lv_point_precise_t* points, int width, lv_color_t color) {
  lv_obj_t* line = lv_line_create(parent);
  lv_line_set_points(line, points, 2);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_color(line, color, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
}

// Diagonal slash across a box: a thick white line to cut a gap through what's
// underneath, then a thin black one. Used for the offline Wi-Fi icon.
void addSlash(lv_obj_t* parent, int x, int y, int w, int h) {
  auto* points = new lv_point_precise_t[2]{{(lv_value_precise_t)x, (lv_value_precise_t)y},
                                           {(lv_value_precise_t)(x + w), (lv_value_precise_t)(y + h)}};
  addLine(parent, points, 6, lv_color_white());
  addLine(parent, points, 2, lv_color_black());
}

const lv_image_dsc_t* mascotFace(const BinScreenData& data) {
  if (!data.wifiConnected || data.isStale) return &mascot_emoji_worried;
  if (data.batteryLow) return &mascot_emoji_sleepy;
  if (data.isTonight) return &mascot_emoji_excited;
  if (data.isToday) return &mascot_emoji_proud;
  // Ordinary days alternate, so the face changes daily.
  return data.dayNumber % 2 ? &mascot_emoji_wink : &mascot_emoji_happy;
}

void addBubble(lv_obj_t* screen, const BinScreenData& data) {
  const bool loud = data.isTonight || data.isToday;
  const lv_color_t ink = loud ? lv_color_white() : lv_color_black();

  lv_obj_t* bubble = lv_obj_create(screen);
  lv_obj_remove_style_all(bubble);
  lv_obj_set_pos(bubble, BUBBLE_X, BUBBLE_Y);
  lv_obj_set_size(bubble, BUBBLE_W, BUBBLE_H);
  lv_obj_set_style_radius(bubble, 14, 0);
  lv_obj_set_style_border_width(bubble, 3, 0);
  lv_obj_set_style_border_color(bubble, lv_color_black(), 0);
  lv_obj_set_style_border_opa(bubble, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bubble, loud ? lv_color_black() : lv_color_white(), 0);
  lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
  lv_obj_set_scrollable(bubble, false);

  // Tail pointing down at the mascot.
  static lv_point_precise_t left[] = {{TAIL_X - TAIL_HALF_W, BUBBLE_Y + BUBBLE_H - 2}, {TAIL_X, TAIL_TIP_Y}};
  static lv_point_precise_t right[] = {{TAIL_X + TAIL_HALF_W, BUBBLE_Y + BUBBLE_H - 2}, {TAIL_X, TAIL_TIP_Y}};
  static lv_point_precise_t gap[] = {{TAIL_X - TAIL_HALF_W + 3, BUBBLE_Y + BUBBLE_H - 2},
                                     {TAIL_X + TAIL_HALF_W - 3, BUBBLE_Y + BUBBLE_H - 2}};
  if (loud) {
    // Fill the tail so it matches the black bubble.
    for (int k = -TAIL_HALF_W + 2; k <= TAIL_HALF_W - 2; k += 2) {
      auto* fill = new lv_point_precise_t[2]{
          {(lv_value_precise_t)(TAIL_X + k), (lv_value_precise_t)(BUBBLE_Y + BUBBLE_H - 2)},
          {TAIL_X, (lv_value_precise_t)TAIL_TIP_Y}};
      addLine(screen, fill, 3, lv_color_black());
    }
  } else {
    addLine(screen, gap, 4, lv_color_white());  // open the bubble's border where the tail joins
  }
  addLine(screen, left, 3, lv_color_black());
  addLine(screen, right, 3, lv_color_black());

  const int centre = BUBBLE_W / 2;
  if (loud) {
    placeCentred(addLabel(bubble, data.isTonight ? "TONIGHT" : "TODAY", &font_bold_32, ink), centre, 8);
  } else {
    char day[12];
    snprintf(day, sizeof(day), "%s %d", DAYS[dayOfWeek(data.pickup)], data.pickup.day);
    placeCentred(addLabel(bubble, data.isStale ? "Last known" : "Bins out", &font_bold_14, ink), centre, 4);
    placeCentred(addLabel(bubble, day, &font_bold_32, ink), centre, 20);
  }

  // The bins going out, in a centred row.
  const char* icons[3];
  int count = 0;
  if (data.rubbish) icons[count++] = ICON_TRASH_CAN;
  if (data.recycling) icons[count++] = ICON_RECYCLE;
  if (data.foodScraps) icons[count++] = ICON_APPLE;
  constexpr int ICON_STEP = 42;
  for (int i = 0; i < count; ++i) {
    lv_obj_t* icon = addLabel(bubble, icons[i], &icons_30, ink);
    placeCentred(icon, centre + (2 * i - (count - 1)) * ICON_STEP / 2, 49);
  }
}

// Temperature over humidity, right-aligned beside the mascot, with status
// icons below only when something needs attention.
void addSide(lv_obj_t* screen, const BinScreenData& data) {
  char temperature[12];
  char humidity[8];
  if (data.hasClimate) {
    snprintf(temperature, sizeof(temperature), "%d\xC2\xB0" "C", std::clamp(data.temperatureC, -9, 99));
    snprintf(humidity, sizeof(humidity), "%d%%", std::clamp(data.humidityPct, 0, 99));
  } else {
    snprintf(temperature, sizeof(temperature), "--\xC2\xB0" "C");
    snprintf(humidity, sizeof(humidity), "--%%");
  }

  const bool issues = !data.wifiConnected || data.batteryLow || data.isStale;
  const int top = issues ? 124 : 134;  // centred beside the mascot, or raised to fit the icons

  lv_obj_t* t = addLabel(screen, temperature, &font_bold_18);
  lv_obj_set_pos(t, SIDE_RIGHT - widthOf(t), top);
  lv_obj_t* h = addLabel(screen, humidity, &font_bold_18);
  const int hx = SIDE_RIGHT - widthOf(h);
  lv_obj_set_pos(h, hx, top + 26);
  lv_obj_t* drop = addLabel(screen, ICON_DROPLET, &icons_18);
  lv_obj_set_pos(drop, hx - widthOf(drop) - 3, top + 26);

  if (!issues) return;
  int x = SIDE_RIGHT;
  const int y = top + 52;
  // Right to left. Returns the icon's width; x moves to its left edge.
  auto addIcon = [&](const char* glyph) {
    lv_obj_t* icon = addLabel(screen, glyph, &icons_18);
    const int w = widthOf(icon);
    x -= w;
    lv_obj_set_pos(icon, x, y);
    return w;
  };
  if (data.isStale) x -= addIcon(ICON_STALE) ? 8 : 0;
  if (data.batteryLow) x -= addIcon(ICON_BATTERY_LOW) ? 8 : 0;
  if (!data.wifiConnected) {
    const int w = addIcon(ICON_WIFI);
    addSlash(screen, x, y, w, 18);
  }
}

}  // namespace

lv_obj_t* createBinScreen(const BinScreenData& data) {
  lv_obj_t* screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_black(), 0);
  lv_obj_set_scrollable(screen, false);

  addBubble(screen, data);

  lv_obj_t* mascot = lv_image_create(screen);
  lv_image_set_src(mascot, mascotFace(data));
  lv_obj_set_pos(mascot, MASCOT_X, MASCOT_Y);

  addSide(screen, data);
  return screen;
}

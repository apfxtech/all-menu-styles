#include "../menu_style_helpers.h"
#include <furi_hal_rtc.h>
#include <power/power_service/power.h>

#define MENU_STYLE_HOME_JUMP 5

static const char* const menu_style_home_weekdays[] =
    {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
static const char* const menu_style_home_months[] =
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static PowerInfo menu_style_home_power(void) {
    static uint32_t next_refresh;
    static PowerInfo info;
    static bool known;
    if(!known || (int32_t)(furi_get_tick() - next_refresh) >= 0) {
        Power* power = furi_record_open(RECORD_POWER);
        power_get_info(power, &info);
        furi_record_close(RECORD_POWER);
        next_refresh = furi_get_tick() + furi_ms_to_ticks(5000);
        known = true;
    }
    return info;
}

static void menu_style_home_draw(Canvas* canvas, MenuModel* model) {
    size_t position = model->position;
    size_t count = model->count;
    char line[24];

    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(line, sizeof(line), "%02u:%02u", now.hour, now.minute);
    canvas_draw_str_aligned(canvas, 2, 1, AlignLeft, AlignTop, line);

    canvas_set_font(canvas, FontSecondary);
    snprintf(
        line,
        sizeof(line),
        "%s %u %s",
        menu_style_home_weekdays[(now.weekday - 1) % 7],
        now.day,
        menu_style_home_months[(now.month - 1) % 12]);
    canvas_draw_str_aligned(canvas, 126, 1, AlignRight, AlignTop, line);
    PowerInfo power = menu_style_home_power();
    snprintf(
        line, sizeof(line), "%s %u%%", power.is_charging ? "Charging" : "Battery", power.charge);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignTop, line);
    canvas_draw_line(canvas, 0, 20, 127, 20);

    for(int32_t i = -2; i <= 2; i++) {
        if((size_t)ABS(i) >= count) continue;
        const MenuItem* item = &model->items[(position + count + i) % count];
        int32_t center_x = 64 + 28 * i;
        if(i == 0) {
            canvas_draw_rframe(canvas, center_x - 11, 25, 22, 22, 3);
            menu_style_icon_centered(canvas, item->icon, center_x - 11, 25, 22, 22);
        } else {
            menu_style_icon_centered(canvas, item->icon, center_x - 7, 29, 14, 14);
        }
    }

    canvas_set_font(canvas, FontPrimary);
    elements_scrollable_text_line_str(
        canvas,
        64,
        60,
        124,
        menu_style_label(&model->items[position], false),
        menu_style_scroll(model, true),
        false,
        true);
}

static size_t menu_style_home_navigate(MenuModel* model, InputKey key) {
    size_t position = model->position;
    switch(key) {
    case InputKeyUp:
        return position >= MENU_STYLE_HOME_JUMP ? position - MENU_STYLE_HOME_JUMP : 0;
    case InputKeyDown:
        return MIN(position + MENU_STYLE_HOME_JUMP, model->count - 1);
    default:
        return menu_style_navigate_wrap(model, key);
    }
}

static const MenuStyle menu_style_home = {
    .draw = menu_style_home_draw,
    .navigate = menu_style_home_navigate,
};

MENU_STYLE_PLUGIN(menu_style_home, menu_style_home_ep)

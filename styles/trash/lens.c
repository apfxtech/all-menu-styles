#include "menu_style_helpers.h"

#define MENU_STYLE_LENS_CX     64
#define MENU_STYLE_LENS_CY     26
#define MENU_STYLE_LENS_RADIUS 20
#define MENU_STYLE_LENS_PITCH  30
#define MENU_STYLE_LENS_JUMP   5

static bool menu_style_lens_pixel(const uint8_t* buffer, bool flipped, int32_t x, int32_t y) {
    if(flipped) {
        x = 127 - x;
        y = 63 - y;
    }
    return buffer[(y >> 3) * 128 + x] & (1 << (y & 7));
}

static void menu_style_lens_draw_magnified(Canvas* canvas, IconAnimation* icon) {
    int32_t width = icon_animation_get_width(icon);
    int32_t height = icon_animation_get_height(icon);
    int32_t x = MENU_STYLE_LENS_CX - width;
    int32_t y = MENU_STYLE_LENS_CY - height;
    bool flipped = canvas_get_orientation(canvas) == CanvasOrientationHorizontalFlip;
    const uint8_t* buffer = canvas_get_buffer(canvas);

    canvas_draw_icon_animation(canvas, 0, 0, icon);
    for(int32_t py = 0; py < height; py++) {
        for(int32_t px = 0; px < width; px++) {
            if(menu_style_lens_pixel(buffer, flipped, px, py)) {
                canvas_draw_box(canvas, x + px * 2, y + py * 2, 2, 2);
            }
        }
    }
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, width, height);
    canvas_set_color(canvas, ColorBlack);
}

static void menu_style_lens_draw(Canvas* canvas, MenuModel* model) {
    size_t position = model->position;
    size_t count = model->count;

    menu_style_lens_draw_magnified(canvas, model->items[position].icon);
    canvas_draw_circle(canvas, MENU_STYLE_LENS_CX, MENU_STYLE_LENS_CY, MENU_STYLE_LENS_RADIUS);

    for(int32_t i = -2; i <= 2; i++) {
        if(i == 0 || (size_t)ABS(i) >= count) continue;
        const MenuItem* item = &model->items[(position + count + i) % count];
        int32_t center_x = MENU_STYLE_LENS_CX + MENU_STYLE_LENS_PITCH * i;
        menu_style_icon_centered(canvas, item->icon, center_x - 7, MENU_STYLE_LENS_CY - 7, 14, 14);
    }

    canvas_set_font(canvas, FontPrimary);
    elements_scrollable_text_line_str(
        canvas,
        64,
        58,
        124,
        menu_style_label(&model->items[position], false),
        menu_style_scroll(model, true),
        false,
        true);
    menu_style_scrollbar_horizontal(canvas, 0, 64, 128, position, count);
}

static size_t menu_style_lens_navigate(MenuModel* model, InputKey key) {
    size_t position = model->position;
    switch(key) {
    case InputKeyUp:
        return position >= MENU_STYLE_LENS_JUMP ? position - MENU_STYLE_LENS_JUMP : 0;
    case InputKeyDown:
        return MIN(position + MENU_STYLE_LENS_JUMP, model->count - 1);
    default:
        return menu_style_navigate_wrap(model, key);
    }
}

static const MenuStyle menu_style_lens = {
    .draw = menu_style_lens_draw,
    .navigate = menu_style_lens_navigate,
};

MENU_STYLE_PLUGIN(menu_style_lens, menu_style_lens_ep)

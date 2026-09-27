#include "menu_style_helpers.h"

#include <furi_hal_version.h>

/*
 * The main menu drawn as the ExpressLRS configurator draws its device page. The app
 * (applications_user/ExpressLRS) does not use the canvas: it renders its own 128x64 frame with
 * its own 5x7 font, so the font, the lcd primitives and the page layout are ported here from
 * elrs_font.cpp, elrs_lcd.cpp and elrs_script.cpp rather than approximated with canvas calls, and
 * the finished frame is blitted in one go.
 *
 * Nothing is drawn that the app does not draw. The entries of the menu stand in for the
 * parameters the app reads out of a transmitter, and the Flipper's own name for the name of the
 * device that answers.
 */

/* Text flags and draw attributes of the app's lcd, under the app's own names */
#define INVERS       0x02u
#define RIGHT        0x04u
#define FORCE        0x02u
#define ERASE        0x04u
#define SOLID        0xffu
#define GREY_DEFAULT 0x000B0000u

/* Layout of the device page, all of it from elrs_script.cpp */
#define LCD_W          128
#define LCD_H          64
#define COL1           0
#define COL2           70
#define BAR_H          9
#define TEXT_SIZE      8
#define TEXT_Y_OFFSET  3
#define MAX_LINE_INDEX 6 /**< The app keeps the selection off the seventh row, which looks ahead */
#define PAGE_ROWS      (MAX_LINE_INDEX + 1)
#define EXITVER        "-- RELOAD --"

/** The parameter load the page opens with, in ms since the menu was opened */
#define ELRS_SEARCH_MS 500 /**< Pinging, nothing has answered yet */
#define ELRS_NAME_MS   500 /**< A device answered with its name, its field count is still unknown */
#define ELRS_FIELD_MS  85 /**< Per entry, once the fields are being read */
#define ELRS_SETTLE_MS 200 /**< Gauge full, before the title comes back */

/** A gap this long means the menu was somewhere else - see menu_style_elrs_elapsed() */
#define ELRS_RESTART_MS 1000

typedef enum {
    ElrsPhaseSearch,
    ElrsPhaseName,
    ElrsPhaseFields,
    ElrsPhaseReady,
} ElrsPhase;

/* The 5x7 font of the app, from elrs_font.cpp. A column of 0xff is not a column of pixels but a
 * column that is not there at all, which is what makes the font proportional. */
static const uint8_t elrs_font_5x7[96][5] = {
    {0x00, 0x00, 0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0x6F, 0xFF, 0xFF}, {0xFF, 0x07, 0x00, 0x07, 0xFF},
    {0x18, 0x30, 0x18, 0x0C, 0x06}, {0x08, 0x7F, 0x7F, 0x7F, 0x08}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50}, {0xFF, 0x05, 0x03, 0xFF, 0xFF}, {0xFF, 0x1C, 0x22, 0x41, 0xFF},
    {0xFF, 0x41, 0x22, 0x1C, 0xFF}, {0x14, 0x08, 0x3E, 0x08, 0x14}, {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0xFF, 0x50, 0x30, 0xFF, 0xFF}, {0x00, 0x08, 0x08, 0x08, 0xFF}, {0xFF, 0xFF, 0x40, 0xFF, 0xFF},
    {0x20, 0x10, 0x08, 0x04, 0x02}, {0x3E, 0x41, 0x41, 0x3E, 0xFF}, {0x00, 0x42, 0x7F, 0x40, 0xFF},
    {0x62, 0x51, 0x49, 0x46, 0xFF}, {0x41, 0x49, 0x49, 0x36, 0xFF}, {0x18, 0x14, 0x12, 0x7F, 0xFF},
    {0x27, 0x45, 0x45, 0x39, 0xFF}, {0x3E, 0x49, 0x49, 0x32, 0xFF}, {0x01, 0x79, 0x05, 0x03, 0xFF},
    {0x36, 0x49, 0x49, 0x36, 0xFF}, {0x06, 0x49, 0x29, 0x1E, 0xFF}, {0xFF, 0x36, 0x36, 0xFF, 0xFF},
    {0xFF, 0x56, 0x36, 0xFF, 0xFF}, {0xFF, 0x08, 0x14, 0x22, 0x41}, {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x41, 0x22, 0x14, 0x08, 0xFF}, {0x02, 0x01, 0x51, 0x09, 0x06}, {0x06, 0x09, 0x09, 0x06, 0xFF},
    {0x7E, 0x09, 0x09, 0x09, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0xFF, 0x41, 0x7F, 0x41, 0xFF},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0xFF},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x26, 0x49, 0x49, 0x49, 0x32}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x30, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0xFF, 0x7F, 0x41, 0x41, 0xFF},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0xFF, 0x41, 0x41, 0x7F, 0xFF}, {0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0xFF}, {0xFF, 0x01, 0x02, 0x04, 0xFF}, {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7F},
    {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x0C, 0x52, 0x52, 0x52, 0x3E},
    {0x7F, 0x08, 0x04, 0x04, 0x78}, {0xFF, 0x44, 0x7D, 0x40, 0xFF}, {0x20, 0x40, 0x44, 0x3D, 0xFF},
    {0x7F, 0x10, 0x28, 0x44, 0xFF}, {0xFF, 0x41, 0x7F, 0x40, 0xFF}, {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38}, {0x7C, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7C}, {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C}, {0x1C, 0x20, 0x40, 0x20, 0x1C},
    {0x3C, 0x40, 0x20, 0x40, 0x3C}, {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44}, {0x10, 0x08, 0x10, 0x08, 0xFF}, {0xFF, 0xFF, 0x7F, 0xFF, 0xFF},
    {0x41, 0x22, 0x54, 0x28, 0xFF}, {0x08, 0x08, 0x22, 0x1C, 0x08}, {0x08, 0x1C, 0x22, 0x08, 0x08},
};

static const uint8_t elrs_font_5x7_extra[19][5] = {
    {0x04, 0x02, 0x7B, 0x02, 0x04}, {0x10, 0x20, 0x6F, 0x20, 0x10}, {0x20, 0x12, 0x0A, 0x02, 0x1E},
    {0x02, 0x24, 0x28, 0x20, 0x3C}, {0x3C, 0x20, 0x28, 0x24, 0x02}, {0x1E, 0x02, 0x0A, 0x12, 0x20},
    {0x60, 0x58, 0x46, 0x58, 0x60}, {0x30, 0x47, 0x5F, 0x47, 0x30}, {0x38, 0x7C, 0x74, 0x7C, 0x38},
    {0x30, 0x78, 0x78, 0x34, 0x0C}, {0x40, 0x6F, 0x71, 0x6F, 0x40}, {0x3E, 0x36, 0x77, 0x36, 0x3E},
    {0x6A, 0x1A, 0x3A, 0x02, 0x1E}, {0x55, 0x55, 0x04, 0x0E, 0x1F}, {0x5E, 0x6E, 0x72, 0x6E, 0x5E},
    {0x70, 0x7A, 0x7F, 0x7F, 0x72}, {0x14, 0x34, 0x55, 0x16, 0x14}, {0x20, 0x75, 0x25, 0x09, 0x02},
    {0x7F, 0x41, 0x41, 0x42, 0x7C},
};

/* The app's display buffer, in the bit order canvas_draw_xbm() takes: one row after another, the
 * leftmost pixel in the lowest bit. The app packs its own buffer by column instead, which nothing
 * outside its lcd layer can see. */
static uint8_t elrs_display[LCD_W * LCD_H / 8];

// Both style callbacks run under the menu's view model mutex, so this needs no lock of its own.
static uint32_t menu_style_elrs_start;
static uint32_t menu_style_elrs_last;
static bool menu_style_elrs_started;
static bool menu_style_elrs_skipped;

static void lcd_clear(void) {
    memset(elrs_display, 0, sizeof(elrs_display));
}

static void lcd_mask_point(int x, int y, uint32_t att) {
    if((unsigned)x >= LCD_W || (unsigned)y >= LCD_H) return;
    uint8_t* p = &elrs_display[y * (LCD_W / 8) + (x >> 3)];
    const uint8_t mask = (uint8_t)(1 << (x & 7));
    if(att & FORCE) {
        *p |= mask;
    } else if(att & ERASE) {
        *p &= (uint8_t)~mask;
    } else {
        *p ^= mask;
    }
}

static void lcd_horizontal_line(int x, int y, int w, uint8_t pat, uint32_t att) {
    if(y >= LCD_H) return;
    if(x + w > LCD_W) w = LCD_W - x;

    while(w-- > 0) {
        if(pat & 1) {
            lcd_mask_point(x, y, att);
            pat = (uint8_t)((pat >> 1) | 0x80);
        } else {
            pat = (uint8_t)(pat >> 1);
        }
        x++;
    }
}

static void lcd_vertical_line(int x, int y, int h, uint8_t pat, uint32_t att) {
    if(x >= LCD_W || y >= LCD_H) return;
    if(y + h > LCD_H) h = LCD_H - y;

    for(int i = 0; i < h; i++) {
        if(pat & (1 << ((y + i) & 7))) lcd_mask_point(x, y + i, att);
    }
}

static void lcd_rect(int x, int y, int w, int h, uint8_t pat, uint32_t att) {
    lcd_vertical_line(x, y, h, pat, att);
    lcd_vertical_line(x + w - 1, y, h, pat, att);
    lcd_horizontal_line(x + 1, y + h - 1, w - 2, pat, att);
    lcd_horizontal_line(x + 1, y, w - 2, pat, att);
}

static void lcd_filled_rect(int x, int y, int w, int h, uint8_t pat, uint32_t att) {
    for(int i = y; i < y + h; i++) {
        lcd_horizontal_line(x, i, w, pat, att);
        pat = (uint8_t)((pat >> 1) + ((pat & 1) << 7));
    }
}

static void lcd_gauge(int x, int y, int w, int h, int num, int den) {
    lcd_rect(x, y, w, h, SOLID, 0);
    if(den == 0) return;

    int len = (uint8_t)(w * num / den);
    if(len < 1) len = 1;
    if(len > w) len = w;
    lcd_filled_rect(x + 1, y + 1, len, h - 2, SOLID, 0);
}

static const uint8_t* char_pattern(uint8_t c) {
    if(c < 0xC0) {
        const int index = (int)c - 0x20;
        return elrs_font_5x7[(index >= 0 && index < 96) ? index : 0];
    }
    const int index = (int)c - 0xC0;
    return elrs_font_5x7_extra[(index < 19) ? index : 0];
}

static int char_width(uint8_t c) {
    const uint8_t* data = char_pattern(c);
    int width = 0;
    for(int i = 0; i < 5; i++) {
        if(data[i] != 0xff) width++;
    }
    return width;
}

static int text_width(const char* s) {
    int width = 0;
    for(const uint8_t* p = (const uint8_t*)s; *p; p++) {
        width += char_width(*p) + 1;
    }
    return width;
}

/** lcdPutPattern() of the app, narrowed to the one font this style draws in.
 *
 * Both the set and the clear pixels of a glyph are written, and an inverted one carries a border
 * of one pixel on every side - which is why a selected row of the app runs one pixel into the
 * rows above and below it. A column of 0xff advances nothing, not even the cursor.
 */
static void lcd_put_char(int* x, int y, uint8_t c, bool inv) {
    const uint8_t* data = char_pattern(c);

    for(int i = 0; i < 7; i++) {
        if(*x >= 0 && *x < LCD_W) {
            uint8_t column = 0;
            if(i == 0) {
                if(*x == 0 || !inv) continue;
                (*x)--;
            } else if(i <= 5) {
                column = data[i - 1];
                if(column == 0xff) continue;
            }

            for(int j = -1; j <= 7; j++) {
                bool plot;
                if(j < 0 || j == 7) {
                    plot = false;
                    if(j < 0 && !inv) continue;
                    if(y + j < 0) continue;
                } else {
                    plot = (column & (1 << j)) != 0;
                }
                if(inv) plot = !plot;
                lcd_mask_point(*x, y + j, plot ? FORCE : ERASE);
            }
        }
        (*x)++;
    }
}

static void lcd_put_text(int* x, int y, const char* s, bool inv) {
    for(const uint8_t* p = (const uint8_t*)s; *p; p++) {
        if(*p < 0x20) continue;
        lcd_put_char(x, y, *p, inv);
    }
}

static void lcd_draw_text(int x, int y, const char* s, uint32_t flags) {
    if(!s) return;
    if(flags & RIGHT) x -= text_width(s);
    lcd_put_text(&x, y, s, (flags & INVERS) != 0);
}

/** Ticks since the menu was opened.
 *
 * A style has no lifecycle hook, so the open is inferred from the gap since the previous frame:
 * menu_enter() starts the 333 ms scroll timer and menu_exit() stops it, so while the menu is on
 * screen another frame always follows within that, and a longer gap means it was away. Which also
 * makes the load replay after every app, as the app itself re-reads its parameters.
 */
static uint32_t menu_style_elrs_elapsed(void) {
    const uint32_t now = furi_get_tick();
    if(!menu_style_elrs_started ||
       now - menu_style_elrs_last > furi_ms_to_ticks(ELRS_RESTART_MS)) {
        menu_style_elrs_start = now;
        menu_style_elrs_started = true;
        menu_style_elrs_skipped = false;
    }
    menu_style_elrs_last = now;
    return now - menu_style_elrs_start;
}

/** Where the load has got to, and how many entries have been read so far */
static ElrsPhase menu_style_elrs_phase(uint32_t elapsed, size_t count, size_t* loaded) {
    *loaded = count;
    if(menu_style_elrs_skipped) return ElrsPhaseReady;

    if(elapsed < furi_ms_to_ticks(ELRS_SEARCH_MS)) {
        *loaded = 0;
        return ElrsPhaseSearch;
    }
    elapsed -= furi_ms_to_ticks(ELRS_SEARCH_MS);

    if(elapsed < furi_ms_to_ticks(ELRS_NAME_MS)) {
        *loaded = 0;
        return ElrsPhaseName;
    }
    elapsed -= furi_ms_to_ticks(ELRS_NAME_MS);

    const uint32_t field = furi_ms_to_ticks(ELRS_FIELD_MS);
    const uint32_t read = elapsed / field;
    if(read < count) {
        *loaded = read;
        return ElrsPhaseFields;
    }
    return (elapsed < count * field + furi_ms_to_ticks(ELRS_SETTLE_MS)) ? ElrsPhaseFields :
                                                                          ElrsPhaseReady;
}

/** lcd_title_bw() of the app: the separator goes down first and the bar is XORed over it, which
 * is what turns it white; while the fields are read the bar is a gauge over the name column. */
static void menu_style_elrs_title(const MenuModel* model, ElrsPhase phase, size_t loaded) {
    lcd_vertical_line(LCD_W - 10, 0, BAR_H, SOLID, INVERS);

    if(phase == ElrsPhaseFields) {
        lcd_filled_rect(COL2, 0, LCD_W, BAR_H, SOLID, GREY_DEFAULT);
        lcd_gauge(0, 0, COL2, BAR_H, (int)loaded, (int)model->count);
    } else {
        lcd_filled_rect(0, 0, LCD_W, BAR_H, SOLID, GREY_DEFAULT);
        const char* title = (phase == ElrsPhaseSearch) ? "Searching for TX..." :
                                                         furi_hal_version_get_device_name_ptr();
        lcd_draw_text(COL1, 1, title, INVERS);
    }
}

/** selectField() of the app, which keeps the selection between the first and the sixth row and
 * leaves the seventh to whatever comes next - the reload entry, once the end is reached. */
static size_t menu_style_elrs_page_offset(MenuModel* model) {
    size_t offset = model->offset;
    if(model->position > MAX_LINE_INDEX - 1 + offset) {
        offset = model->position - (MAX_LINE_INDEX - 1);
    } else if(model->position < offset) {
        offset = model->position;
    }
    model->offset = offset;
    return offset;
}

static void menu_style_elrs_page(MenuModel* model, size_t loaded) {
    const size_t count = model->count;
    const size_t offset = menu_style_elrs_page_offset(model);

    for(size_t row = 1; row <= PAGE_ROWS; row++) {
        const size_t item_i = offset + row - 1;
        const int y = (int)row * TEXT_SIZE + TEXT_Y_OFFSET;

        if(item_i > count) {
            break;
        } else if(item_i == count) {
            // The app closes every list with a back entry; at the top level it reloads the page
            lcd_draw_text(10, y, "[" EXITVER "]", 0);
            break;
        } else if(item_i >= loaded) {
            // A field whose name has not arrived is drawn as nothing at all
            break;
        }

        // Every entry opens something, so each is drawn as the app draws a folder
        const bool selected = item_i == model->position;
        int x = COL1;
        lcd_put_text(&x, y, "> ", selected);
        lcd_put_text(&x, y, model->items[item_i].label, selected);
    }
}

static void menu_style_elrs_draw(Canvas* canvas, MenuModel* model) {
    size_t loaded = 0;
    const ElrsPhase phase =
        menu_style_elrs_phase(menu_style_elrs_elapsed(), model->count, &loaded);

    lcd_clear();
    menu_style_elrs_title(model, phase, loaded);
    menu_style_elrs_page(model, loaded);
    canvas_draw_xbm(canvas, 0, 0, LCD_W, LCD_H, elrs_display);
}

static size_t menu_style_elrs_navigate(MenuModel* model, InputKey key) {
    // Whatever the load is still drawing, the page is usable the moment someone steers it
    menu_style_elrs_skipped = true;

    // The app's menu is a rotary encoder away from the handset: one step at a time, wrapping
    if(key == InputKeyLeft) key = InputKeyUp;
    if(key == InputKeyRight) key = InputKeyDown;
    return menu_style_navigate_list(model, key);
}

static const MenuStyle menu_style_elrs = {
    .draw = menu_style_elrs_draw,
    .navigate = menu_style_elrs_navigate,
};

MENU_STYLE_PLUGIN(menu_style_elrs, menu_style_elrs_ep)

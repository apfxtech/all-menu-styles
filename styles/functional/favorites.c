#include "../menu_style_helpers.h"
#include <loader/loader.h>
#include <storage/storage.h>
#include <flipper_application/flipper_application.h>

#define MENU_STYLE_FAVORITES_PATH  EXT_PATH("favorites.txt")
#define MENU_STYLE_FAVORITES_MAX   6
#define MENU_STYLE_FAVORITES_PITCH 21

typedef struct {
    FuriString* path;
    FuriString* name;
    uint8_t icon[FAP_MANIFEST_MAX_ICON_SIZE];
    bool has_icon;
} MenuStyleFavorite;

static MenuStyleFavorite menu_style_favorites_list[MENU_STYLE_FAVORITES_MAX];
static size_t menu_style_favorites_count;
static bool menu_style_favorites_loaded;
static uint64_t menu_style_favorites_file_size;

static bool menu_style_favorites_add(Storage* storage, FuriString* line) {
    if(!furi_string_end_with_str(line, ".fap")) return false;
    MenuStyleFavorite* favorite = &menu_style_favorites_list[menu_style_favorites_count];
    memset(favorite->icon, 0, sizeof(favorite->icon));
    uint8_t* icon = favorite->icon;
    if(!flipper_application_load_name_and_icon(line, storage, &icon, favorite->name)) {
        return false;
    }
    furi_string_set(favorite->path, line);
    favorite->has_icon = false;
    for(size_t i = 0; i < sizeof(favorite->icon); i++) {
        if(favorite->icon[i]) favorite->has_icon = true;
    }
    menu_style_favorites_count++;
    return menu_style_favorites_count == MENU_STYLE_FAVORITES_MAX;
}

static void menu_style_favorites_load(Storage* storage, uint64_t file_size) {
    for(size_t i = 0; i < MENU_STYLE_FAVORITES_MAX; i++) {
        if(!menu_style_favorites_list[i].path) {
            menu_style_favorites_list[i].path = furi_string_alloc();
            menu_style_favorites_list[i].name = furi_string_alloc();
        }
    }
    menu_style_favorites_count = 0;
    menu_style_favorites_file_size = file_size;
    menu_style_favorites_loaded = true;

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, MENU_STYLE_FAVORITES_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        char chunk[64];
        size_t read;
        bool full = false;
        while(!full && (read = storage_file_read(file, chunk, sizeof(chunk))) > 0) {
            for(size_t i = 0; i < read && !full; i++) {
                if(chunk[i] == '\n' || chunk[i] == '\r') {
                    if(furi_string_size(line)) full = menu_style_favorites_add(storage, line);
                    furi_string_reset(line);
                } else {
                    furi_string_push_back(line, chunk[i]);
                }
            }
        }
        if(!full && furi_string_size(line)) menu_style_favorites_add(storage, line);
        furi_string_free(line);
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void menu_style_favorites_refresh(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo info;
    uint64_t file_size = 0;
    if(storage_common_stat(storage, MENU_STYLE_FAVORITES_PATH, &info) == FSE_OK) {
        file_size = info.size;
    }
    if(!menu_style_favorites_loaded || file_size != menu_style_favorites_file_size) {
        menu_style_favorites_load(storage, file_size);
    }
    furi_record_close(RECORD_STORAGE);
}

static void menu_style_favorites_launch(const MenuStyleFavorite* favorite) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_start_detached_with_gui_error(loader, furi_string_get_cstr(favorite->path), NULL);
    furi_record_close(RECORD_LOADER);
}

static size_t menu_style_favorites_cursor(MenuModel* model) {
    if(model->offset > menu_style_favorites_count) model->offset = 0;
    return model->offset;
}

static void menu_style_favorites_draw(Canvas* canvas, MenuModel* model) {
    if(!menu_style_favorites_loaded) menu_style_favorites_refresh();

    size_t position = model->position;
    size_t count = model->count;
    size_t cursor = menu_style_favorites_cursor(model);

    canvas_set_font(canvas, FontSecondary);
    if(menu_style_favorites_count == 0) {
        canvas_draw_str_aligned(canvas, 2, 4, AlignLeft, AlignTop, "Add favorites in Archive");
    }
    for(size_t i = 0; i < menu_style_favorites_count; i++) {
        const MenuStyleFavorite* favorite = &menu_style_favorites_list[i];
        int32_t x = 2 + MENU_STYLE_FAVORITES_PITCH * i;
        if(cursor == i + 1) {
            canvas_draw_rbox(canvas, x, 0, 18, 16, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, x, 0, 18, 16, 2);
        }
        if(favorite->has_icon) {
            canvas_draw_bitmap(canvas, x + 4, 3, 10, 10, favorite->icon);
        } else {
            canvas_draw_rframe(canvas, x + 4, 3, 10, 10, 2);
        }
        canvas_set_color(canvas, ColorBlack);
    }

    for(int32_t i = -2; i <= 2; i++) {
        if((size_t)ABS(i) >= count) continue;
        const MenuItem* item = &model->items[(position + count + i) % count];
        int32_t center_x = 64 + 28 * i;
        if(i == 0) {
            canvas_draw_rframe(canvas, center_x - 11, 22, 22, 22, 3);
            menu_style_icon_centered(canvas, item->icon, center_x - 11, 22, 22, 22);
        } else {
            menu_style_icon_centered(canvas, item->icon, center_x - 7, 26, 14, 14);
        }
    }

    const char* label = cursor ? furi_string_get_cstr(menu_style_favorites_list[cursor - 1].name) :
                                 menu_style_label(&model->items[position], false);
    canvas_set_font(canvas, FontPrimary);
    elements_scrollable_text_line_str(
        canvas, 64, 57, 120, label, menu_style_scroll(model, true), false, true);
    menu_style_scrollbar_horizontal(canvas, 0, 64, 128, position, count);
}

static size_t menu_style_favorites_navigate(MenuModel* model, InputKey key) {
    size_t position = model->position;
    size_t cursor = menu_style_favorites_cursor(model);

    if(cursor == 0) {
        switch(key) {
        case InputKeyUp:
            menu_style_favorites_refresh();
            if(menu_style_favorites_count) model->offset = 1;
            return position;
        case InputKeyDown:
            return position;
        default:
            return menu_style_navigate_wrap(model, key);
        }
    }

    switch(key) {
    case InputKeyLeft:
        model->offset = cursor > 1 ? cursor - 1 : menu_style_favorites_count;
        break;
    case InputKeyRight:
        model->offset = cursor < menu_style_favorites_count ? cursor + 1 : 1;
        break;
    case InputKeyUp:
        menu_style_favorites_launch(&menu_style_favorites_list[cursor - 1]);
        model->offset = 0;
        break;
    default:
        model->offset = 0;
        break;
    }
    return position;
}

static const MenuStyle menu_style_favorites = {
    .draw = menu_style_favorites_draw,
    .navigate = menu_style_favorites_navigate,
};

MENU_STYLE_PLUGIN(menu_style_favorites, menu_style_favorites_ep)

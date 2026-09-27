#include <applications.h>
#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/loading.h>
#include <gui/modules/menu.h>
#include <gui/modules/submenu.h>
#include <gui/view_dispatcher.h>
#include <loader/loader.h>
#include <storage/storage.h>

#include <string.h>

#define TAG "MenuViewer"

typedef enum {
    MenuViewerViewStyles,
    MenuViewerViewPreview,
    MenuViewerViewLoading,
} MenuViewerView;

typedef enum {
    MenuViewerEventRefresh = 1,
    MenuViewerEventReturn,
    MenuViewerEventOpenBase = 100,
} MenuViewerEvent;

typedef struct {
    char* file;
    char* name;
} MenuViewerStyle;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    Submenu* styles_menu;
    Menu* preview_menu;
    Loading* loading;
    PluginManager* plugin;
    MenuViewerStyle* styles;
    size_t style_count;
    bool preview_active;
} MenuViewer;

static void menu_viewer_dummy_callback(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

static void menu_viewer_build_preview(MenuViewer* app) {
    size_t index = 0;
    menu_add_item(
        app->preview_menu,
        LOADER_APPLICATIONS_NAME,
        NULL,
        index++,
        menu_viewer_dummy_callback,
        app);

    for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
        menu_add_item(
            app->preview_menu,
            FLIPPER_APPS[i].name,
            FLIPPER_APPS[i].icon,
            index++,
            menu_viewer_dummy_callback,
            app);
    }
    for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT; i++) {
        menu_add_item(
            app->preview_menu,
            FLIPPER_EXTERNAL_APPS[i].name,
            FLIPPER_EXTERNAL_APPS[i].icon,
            index++,
            menu_viewer_dummy_callback,
            app);
    }
    menu_add_item(app->preview_menu, "Settings", NULL, index, menu_viewer_dummy_callback, app);
}

static void menu_viewer_free_styles(MenuViewer* app) {
    for(size_t i = 0; i < app->style_count; i++) {
        free(app->styles[i].file);
        free(app->styles[i].name);
    }
    free(app->styles);
    app->styles = NULL;
    app->style_count = 0;
}

static void menu_viewer_select_callback(void* context, uint32_t index) {
    MenuViewer* app = context;
    view_dispatcher_send_custom_event(
        app->dispatcher, index ? MenuViewerEventOpenBase + index - 1 : MenuViewerEventRefresh);
}

static void menu_viewer_scan_styles(MenuViewer* app) {
    File* directory = storage_file_alloc(app->storage);
    FuriString* path = furi_string_alloc();
    FuriString* name = furi_string_alloc();
    FuriString* file_name = furi_string_alloc();
    uint8_t icon[FAP_MANIFEST_MAX_ICON_SIZE];
    uint8_t* icon_ptr = icon;
    char file[64];

    submenu_reset(app->styles_menu);
    submenu_set_header(app->styles_menu, "MenuView");
    menu_viewer_free_styles(app);
    submenu_add_item(app->styles_menu, "Refresh", 0, menu_viewer_select_callback, app);

    if(storage_dir_open(directory, LOADER_MENU_STYLES_PATH)) {
        while(storage_dir_read(directory, NULL, file, sizeof(file))) {
            furi_string_set_str(file_name, file);
            if(!furi_string_start_with_str(file_name, LOADER_MENU_STYLE_PREFIX) ||
               !furi_string_end_with_str(file_name, ".fal")) {
                continue;
            }

            furi_string_printf(path, "%s/%s", LOADER_MENU_STYLES_PATH, file);
            if(!flipper_application_load_name_and_icon(path, app->storage, &icon_ptr, name)) {
                FURI_LOG_W(TAG, "Skipping unreadable style %s", file);
                continue;
            }

            const size_t old_count = app->style_count;
            app->styles = realloc(app->styles, (old_count + 1) * sizeof(MenuViewerStyle));
            size_t position = old_count;
            while(position &&
                  strcmp(app->styles[position - 1].name, furi_string_get_cstr(name)) > 0) {
                app->styles[position] = app->styles[position - 1];
                position--;
            }
            app->styles[position].file = strdup(file);
            app->styles[position].name = strdup(furi_string_get_cstr(name));
            app->style_count++;
        }
        storage_dir_close(directory);
    } else {
        FURI_LOG_W(TAG, "Cannot open %s", LOADER_MENU_STYLES_PATH);
    }

    for(size_t i = 0; i < app->style_count; i++) {
        submenu_add_item(
            app->styles_menu, app->styles[i].name, i + 1, menu_viewer_select_callback, app);
    }
    submenu_set_selected_item(app->styles_menu, 0);

    furi_string_free(file_name);
    furi_string_free(name);
    furi_string_free(path);
    storage_file_free(directory);
}

static void menu_viewer_unload_style(MenuViewer* app) {
    menu_set_style(app->preview_menu, NULL);
    if(app->plugin) {
        plugin_manager_free(app->plugin);
        app->plugin = NULL;
    }
}

static bool menu_viewer_custom_event(void* context, uint32_t event) {
    MenuViewer* app = context;

    if(event == MenuViewerEventReturn && app->preview_active) {
        app->preview_active = false;
        view_dispatcher_switch_to_view(app->dispatcher, MenuViewerViewStyles);
        menu_viewer_unload_style(app);
        return true;
    }

    if(event == MenuViewerEventRefresh) {
        view_dispatcher_switch_to_view(app->dispatcher, MenuViewerViewLoading);
        furi_delay_ms(150);
        menu_viewer_scan_styles(app);
        view_dispatcher_switch_to_view(app->dispatcher, MenuViewerViewStyles);
        return true;
    }

    if(event >= MenuViewerEventOpenBase) {
        const size_t index = event - MenuViewerEventOpenBase;
        if(index >= app->style_count || app->preview_active) return true;

        view_dispatcher_switch_to_view(app->dispatcher, MenuViewerViewLoading);
        furi_delay_ms(150);
        FuriString* path =
            furi_string_alloc_printf("%s/%s", LOADER_MENU_STYLES_PATH, app->styles[index].file);
        PluginManager* plugin =
            plugin_manager_alloc(MENU_STYLE_PLUGIN_APP_ID, MENU_STYLE_PLUGIN_API_VERSION, NULL);
        PluginManagerError error = plugin_manager_load_single(plugin, furi_string_get_cstr(path));
        furi_string_free(path);

        const MenuStyle* style = NULL;
        if(error == PluginManagerErrorNone) {
            const MenuStyle* candidate = plugin_manager_get_ep(plugin, 0);
            if(candidate && candidate->draw && candidate->navigate) style = candidate;
        }

        if(style) {
            app->plugin = plugin;
            menu_set_style(app->preview_menu, style);
            app->preview_active = true;
            view_dispatcher_switch_to_view(app->dispatcher, MenuViewerViewPreview);
        } else {
            FURI_LOG_E(TAG, "Cannot load style (%u)", error);
            plugin_manager_free(plugin);
            view_dispatcher_switch_to_view(app->dispatcher, MenuViewerViewStyles);
        }
        return true;
    }

    return false;
}

static bool menu_viewer_navigation_event(void* context) {
    MenuViewer* app = context;
    if(app->preview_active) {
        view_dispatcher_send_custom_event(app->dispatcher, MenuViewerEventReturn);
        return true;
    }
    return false;
}

int32_t menu_viewer_app(void* argument) {
    UNUSED(argument);
    MenuViewer* app = malloc(sizeof(MenuViewer));
    memset(app, 0, sizeof(MenuViewer));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dispatcher = view_dispatcher_alloc();
    app->styles_menu = submenu_alloc();
    app->preview_menu = menu_alloc();
    app->loading = loading_alloc();

    menu_viewer_build_preview(app);
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, menu_viewer_custom_event);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, menu_viewer_navigation_event);
    view_dispatcher_add_view(
        app->dispatcher, MenuViewerViewStyles, submenu_get_view(app->styles_menu));
    view_dispatcher_add_view(
        app->dispatcher, MenuViewerViewPreview, menu_get_view(app->preview_menu));
    view_dispatcher_add_view(
        app->dispatcher, MenuViewerViewLoading, loading_get_view(app->loading));
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_switch_to_view(app->dispatcher, MenuViewerViewLoading);
    furi_delay_ms(150);
    menu_viewer_scan_styles(app);
    view_dispatcher_switch_to_view(app->dispatcher, MenuViewerViewStyles);
    view_dispatcher_run(app->dispatcher);

    view_dispatcher_switch_to_view(app->dispatcher, MenuViewerViewLoading);
    menu_viewer_unload_style(app);
    view_dispatcher_remove_view(app->dispatcher, MenuViewerViewStyles);
    view_dispatcher_remove_view(app->dispatcher, MenuViewerViewPreview);
    view_dispatcher_remove_view(app->dispatcher, MenuViewerViewLoading);
    view_dispatcher_free(app->dispatcher);
    loading_free(app->loading);
    menu_free(app->preview_menu);
    submenu_free(app->styles_menu);
    menu_viewer_free_styles(app);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}

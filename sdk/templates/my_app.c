/**
 * @file my_app.c
 * @brief Starter template for a Stratus Watch App
 * 
 * Drop this file into the `user_projects/` folder and it will be 
 * automatically compiled and registered in the watch's App Drawer.
 */

#include "idreeswatch_sdk.h"

// Optional: Define a logging tag for this app
static const char *TAG = "my_app";

// Declare UI elements globally for this app's lifecycle
static struct {
    lv_obj_t *screen;
    lv_obj_t *label;
    lv_obj_t *btn;
} app_state_t;

/**
 * @brief Button click event handler
 */
static void btn_click_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Button was clicked!");
    lv_label_set_text(app_state_t.label, "Hello from the SDK!");
    ui_manager_show_notification("Button Clicked", 3000);
}

/**
 * @brief Initialize the app UI
 * This is called when the user opens the app from the launcher.
 * 
 * @param parent The screen object to draw on
 */
static void my_app_create(lv_obj_t *parent) {
    app_state_t.screen = parent;
    
    // Enable swipe right to exit the app
    ui_add_swipe_to_exit(parent);
    
    // Optional: Make the background match the current theme
    lv_obj_set_style_bg_color(parent, ui_theme_background(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    // Create a title using the standard UI builder
    lv_obj_t *title = ui_create_title(parent, "My First App");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    // Create a text label
    app_state_t.label = ui_create_label(parent, "Press the button below", ui_theme_text());
    lv_obj_align(app_state_t.label, LV_ALIGN_CENTER, 0, -20);

    // Create a styled button
    app_state_t.btn = ui_create_button(parent, "Click Me");
    lv_obj_align(app_state_t.btn, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_event_cb(app_state_t.btn, btn_click_cb, LV_EVENT_CLICKED, NULL);
    
    ESP_LOGI(TAG, "App started");
}

/**
 * @brief Clean up the app UI
 * This is called when the user exits the app.
 * All LVGL children of `parent` are deleted automatically,
 * but you should free any generic memory here and clean up state.
 */
static void my_app_destroy(void) {
    // Reset state struct
    memset(&app_state_t, 0, sizeof(app_state_t));
    ESP_LOGI(TAG, "App destroyed");
}

// ============================================================================
// Automatic Registration
// ============================================================================
// The IDREESWATCH_REGISTER_APP macro registers your app at boot time.
// Parameters:
// 1. App ID (unique valid C identifier name, no quotes)
// 2. Display Name (in quotes)
// 3. LVGL Icon Symbol (e.g., LV_SYMBOL_SETTINGS, LV_SYMBOL_STAR, etc.)
// 4. Create function pointer
// 5. Destroy function pointer

IDREESWATCH_REGISTER_APP(
    my_first_app,           // ID
    "My App",               // Name
    LV_SYMBOL_SETTINGS,     // Icon
    my_app_create,          // Init Callback
    my_app_destroy          // Deinit Callback
);

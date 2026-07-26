/**
 * @file my_watchface.c
 * @brief Starter template for an IdreesWatch SDK Watchface
 * 
 * Drop this file into the `user_projects/` folder and it will be 
 * automatically compiled and registered in the watchface picker.
 */

#include "idreeswatch_sdk.h"

// Define a logging tag for this watchface
static const char *TAG = "my_wf";

// Watchface state
static struct {
    lv_obj_t *screen;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_obj_t *battery_icon;
    lv_timer_t *update_timer;
} wf_state_t;

/**
 * @brief Periodically update the time and data on the watchface
 */
static void update_time_data(void) {
    if (!wf_state_t.time_label) return;
    
    // Get formatted time string block (e.g. "10:30")
    char time_str[16];
    time_service_get_time_string(time_str, sizeof(time_str), NULL);
    lv_label_set_text(wf_state_t.time_label, time_str);
    
    // Get formatted date (e.g. "Mon, Oct 5")
    char date_str[32];
    time_service_get_date_string(date_str, sizeof(date_str), "%a, %b %d");
    lv_label_set_text(wf_state_t.date_label, date_str);
    
    // Quick battery status
    uint8_t batt = power_service_get_battery_percent();
    bool charging = power_service_is_charging();
    
    if (charging) {
        lv_label_set_text_fmt(wf_state_t.battery_icon, LV_SYMBOL_CHARGE " %d%%", batt);
        lv_obj_set_style_text_color(wf_state_t.battery_icon, ui_theme_primary(), 0);
    } else {
        lv_label_set_text_fmt(wf_state_t.battery_icon, LV_SYMBOL_BATTERY_FULL " %d%%", batt);
        
        // Change color based on battery level
        if (batt < 20) {
            lv_obj_set_style_text_color(wf_state_t.battery_icon, lv_color_hex(0xFF3B30), 0);
        } else {
            lv_obj_set_style_text_color(wf_state_t.battery_icon, ui_theme_text_secondary(), 0);
        }
    }
}

/**
 * @brief Timer tick callback
 */
static void timer_tick(lv_timer_t *timer) {
    update_time_data();
}

/**
 * @brief Initialize the watchface UI
 * This is called once when the watchface is selected
 * 
 * @param parent The screen object to draw on
 */
static void my_wf_create(lv_obj_t *parent) {
    wf_state_t.screen = parent;
    
    // Standard black background
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    // ==========================================
    // UI Layout
    // ==========================================
    
    // Big central time
    wf_state_t.time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(wf_state_t.time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(wf_state_t.time_label, ui_theme_primary(), 0);
    lv_obj_align(wf_state_t.time_label, LV_ALIGN_CENTER, 0, -20);
    
    // Date label below time
    wf_state_t.date_label = lv_label_create(parent);
    lv_obj_set_style_text_font(wf_state_t.date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(wf_state_t.date_label, ui_theme_text_secondary(), 0);
    lv_obj_align(wf_state_t.date_label, LV_ALIGN_CENTER, 0, 30);
    
    // Battery at top
    wf_state_t.battery_icon = lv_label_create(parent);
    lv_obj_set_style_text_font(wf_state_t.battery_icon, &lv_font_montserrat_16, 0);
    lv_obj_align(wf_state_t.battery_icon, LV_ALIGN_TOP_MID, 0, 40);

    // Set initial values
    update_time_data();
    
    // Create a 1-second update timer
    wf_state_t.update_timer = lv_timer_create(timer_tick, 1000, NULL);
    
    ESP_LOGI(TAG, "Watchface created");
}

/**
 * @brief Update callback
 * Alternatively, the OS natively calls this ~every 1 second.
 * We're using our own lv_timer in this template, so we leave this blank.
 */
static void my_wf_update(void) {
    // If you don't use lv_timer_create, you can update your UI here
    // update_time_data();
}

/**
 * @brief Clean up the watchface UI
 * Called when the user switches to a different watchface
 */
static void my_wf_destroy(void) {
    if (wf_state_t.update_timer) {
        lv_timer_delete(wf_state_t.update_timer);
        wf_state_t.update_timer = NULL;
    }
    memset(&wf_state_t, 0, sizeof(wf_state_t));
    ESP_LOGI(TAG, "Watchface destroyed");
}

// ============================================================================
// Automatic Registration
// ============================================================================
// The IDREESWATCH_REGISTER_WATCHFACE macro registers your face at boot time.
// Parameters:
// 1. ID (unique valid C identifier name, no quotes)
// 2. Display Name (in quotes)
// 3. Author Name (in quotes)
// 4. Create function pointer
// 5. Update function pointer (called approx every 1 second by simple watchfaces)
// 6. Destroy function pointer

IDREESWATCH_REGISTER_WATCHFACE(
    my_minimal_face,        // ID
    "Minimal Custom",       // Display Name
    "SDK Developer",        // Author
    my_wf_create,           // Create callback
    my_wf_update,           // Update callback
    my_wf_destroy           // Destroy callback
);

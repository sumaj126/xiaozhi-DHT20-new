#include "oled_display.h"
#include "assets/lang_config.h"
#include "lvgl_theme.h"
#include "lvgl_font.h"
#include "sensors/sensor_manager.h"

#include <string>
#include <algorithm>
#include <ctime>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <font_awesome.h>

#define TAG "OledDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_1);

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_awesome_30_1);
    
    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("dark", dark_theme);
    current_theme_ = dark_theme;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    // Note: SetupUI() should be called by Application::Initialize(), not in constructor
    // to ensure lvgl objects are created after the display is fully initialized.
}

void OledDisplay::SetupUI() {
    // Prevent duplicate calls - if already called, return early
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }
    
    Display::SetupUI();  // Mark SetupUI as called
    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
}

OledDisplay::~OledDisplay() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }

    bool is_128x64_layout = (top_bar_ != nullptr);
    if (status_bar_ != nullptr && is_128x64_layout) {
        status_label_ = nullptr;
        notification_label_ = nullptr;
        lv_obj_del(status_bar_);
    }
    if (top_bar_ != nullptr) {
        network_label_ = nullptr;
        mute_label_ = nullptr;
        battery_label_ = nullptr;
        lv_obj_del(top_bar_);
    }
    if (side_bar_ != nullptr) {
        if (!is_128x64_layout) {
            status_label_ = nullptr;
            notification_label_ = nullptr;
            network_label_ = nullptr;
            mute_label_ = nullptr;
            battery_label_ = nullptr;
        }
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // Replace all newlines with spaces
    std::string content_str = content;
    std::replace(content_str.begin(), content_str.end(), '\n', ' ');

    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content_str.c_str());
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content_str.c_str());
            lv_obj_remove_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Layer 1: Top bar - for status icons */
    top_bar_ = lv_obj_create(container_);
    lv_obj_set_size(top_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_pad_all(top_bar_, 0, 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);

    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    lv_obj_t* right_icons = lv_obj_create(top_bar_);
    lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_icons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_icons, 0, 0);
    lv_obj_set_style_pad_all(right_icons, 0, 0);
    lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    mute_label_ = lv_label_create(right_icons);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    battery_label_ = lv_label_create(right_icons);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    /* Layer 2: Status bar - for center text labels */
    status_bar_ = lv_obj_create(screen);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);  // Use absolute positioning
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);  // Overlap with top_bar_

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(notification_label_, LV_HOR_RES);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(status_label_, LV_HOR_RES);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);

    content_left_ = lv_obj_create(content_);
    lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_left_, 0, 0);
    lv_obj_set_style_border_width(content_left_, 0, 0);

    emotion_label_ = lv_label_create(content_left_);
    lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_center(emotion_label_);
    lv_obj_set_style_pad_top(emotion_label_, 8, 0);

    content_right_ = lv_obj_create(content_);
    lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_right_, 0, 0);
    lv_obj_set_style_border_width(content_right_, 0, 0);
    lv_obj_set_flex_grow(content_right_, 1);
    lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_right_);
    lv_label_set_text(chat_message_label_, "");
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(chat_message_label_, width_ - 32);
    lv_obj_set_style_pad_top(chat_message_label_, 14, 0);

    // Start scrolling subtitle after a delay
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    // Setup standby screen
    SetupStandbyScreen();
}

void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Emotion label on the left side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_center(emotion_label_);

    /* Right side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_size(side_bar_, width_ - 32, 32);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, width_ - 32, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_style_pad_left(status_label_, 2, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_pad_left(notification_label_, 2, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    // Start scrolling subtitle after a delay
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    // Setup standby screen
    SetupStandbyScreen();
}

void OledDisplay::SetEmotion(const char* emotion) {
    const char* utf8 = font_awesome_get_utf8(emotion);
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    if (utf8 != nullptr) {
        lv_label_set_text(emotion_label_, utf8);
    } else {
        lv_label_set_text(emotion_label_, FONT_AWESOME_NEUTRAL);
    }
}

void OledDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
}

void OledDisplay::SetupStandbyScreen() {
    DisplayLockGuard lock(this);

    ESP_LOGI(TAG, "SetupStandbyScreen() called");

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    ESP_LOGI(TAG, "text_font=%p, large_icon_font=%p", text_font, large_icon_font);

    auto screen = lv_screen_active();

    // Create standby screen container
    standby_screen_ = lv_obj_create(screen);
    lv_obj_set_size(standby_screen_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_radius(standby_screen_, 0, 0);
    lv_obj_set_style_bg_opa(standby_screen_, LV_OPA_COVER, 0);  // Make background opaque
    lv_obj_set_style_bg_color(standby_screen_, lv_color_black(), 0);  // Set background color to black
    lv_obj_set_style_border_width(standby_screen_, 0, 0);
    lv_obj_set_style_pad_all(standby_screen_, 0, 0);
    lv_obj_set_scrollbar_mode(standby_screen_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(standby_screen_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(standby_screen_, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "standby_screen_ container created: %p", standby_screen_);

    // Date label (top)
    date_label_ = lv_label_create(standby_screen_);
    lv_obj_set_width(date_label_, LV_HOR_RES);
    lv_obj_set_style_text_align(date_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(date_label_, text_font, 0);
    lv_obj_set_style_text_color(date_label_, lv_color_white(), 0);  // Set text color to white
    lv_label_set_text(date_label_, "2023-01-01");
    lv_obj_align(date_label_, LV_ALIGN_TOP_MID, 0, 4);
    ESP_LOGI(TAG, "date_label_ created: %p", date_label_);

    // Weekday label
    weekday_label_ = lv_label_create(standby_screen_);
    lv_obj_set_width(weekday_label_, LV_HOR_RES);
    lv_obj_set_style_text_align(weekday_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(weekday_label_, text_font, 0);
    lv_obj_set_style_text_color(weekday_label_, lv_color_white(), 0);  // Set text color to white
    lv_label_set_text(weekday_label_, "Sunday");
    lv_obj_align(weekday_label_, LV_ALIGN_TOP_MID, 0, 18);
    ESP_LOGI(TAG, "weekday_label_ created: %p", weekday_label_);

    // Time label (center, largest font)
    time_label_ = lv_label_create(standby_screen_);
    lv_obj_set_width(time_label_, LV_HOR_RES);  // Same as date and weekday labels
    lv_obj_set_style_text_align(time_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(time_label_, text_font, 0);  // Use text font for time display
    lv_obj_set_style_text_color(time_label_, lv_color_white(), 0);  // Set text color to white
    lv_label_set_text(time_label_, "12:00");
    // Position time label at fixed Y position (between weekday and temp_humidity)
    lv_obj_align(time_label_, LV_ALIGN_TOP_MID, 0, 32);
    ESP_LOGI(TAG, "time_label_ created: %p, text='12:00'", time_label_);

    // Temperature and humidity label (bottom)
    temp_humidity_label_ = lv_label_create(standby_screen_);
    lv_obj_set_width(temp_humidity_label_, LV_HOR_RES);
    lv_obj_set_style_text_align(temp_humidity_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(temp_humidity_label_, text_font, 0);
    lv_obj_set_style_text_color(temp_humidity_label_, lv_color_white(), 0);  // Set text color to white
    lv_label_set_text(temp_humidity_label_, "25.0°C / 50.0%");
    lv_obj_align(temp_humidity_label_, LV_ALIGN_BOTTOM_MID, 0, -4);
    ESP_LOGI(TAG, "temp_humidity_label_ created: %p", temp_humidity_label_);
    
    ESP_LOGI(TAG, "SetupStandbyScreen() completed");
}

void OledDisplay::ShowStandbyScreen() {
    DisplayLockGuard lock(this);

    ESP_LOGI(TAG, "ShowStandbyScreen() called");

    if (standby_screen_ == nullptr) {
        SetupStandbyScreen();
    }

    // Hide other UI elements
    if (container_) {
        lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }
    if (top_bar_) {
        lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (status_bar_) {
        lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (content_) {
        lv_obj_add_flag(content_, LV_OBJ_FLAG_HIDDEN);
    }
    if (content_left_) {
        lv_obj_add_flag(content_left_, LV_OBJ_FLAG_HIDDEN);
    }
    if (content_right_) {
        lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
    }
    if (side_bar_) {
        lv_obj_add_flag(side_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (low_battery_popup_) {
        lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emotion_label_) {
        lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
    }

    // Show standby screen
    lv_obj_remove_flag(standby_screen_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(standby_screen_);  // Move to foreground
    ESP_LOGI(TAG, "Standby screen shown, calling UpdateStandbyScreen()");
    UpdateStandbyScreen();
}

void OledDisplay::HideStandbyScreen() {
    DisplayLockGuard lock(this);

    if (standby_screen_ != nullptr) {
        lv_obj_add_flag(standby_screen_, LV_OBJ_FLAG_HIDDEN);
    }

    // Show other UI elements
    if (container_) {
        lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }
    if (top_bar_) {
        lv_obj_remove_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (status_bar_) {
        lv_obj_remove_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (content_) {
        lv_obj_remove_flag(content_, LV_OBJ_FLAG_HIDDEN);
    }
    if (content_left_) {
        lv_obj_remove_flag(content_left_, LV_OBJ_FLAG_HIDDEN);
    }
    if (content_right_) {
        lv_obj_remove_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
    }
    if (side_bar_) {
        lv_obj_remove_flag(side_bar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emotion_label_) {
        lv_obj_remove_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
    }
}

void OledDisplay::UpdateStandbyScreen() {
    DisplayLockGuard lock(this);

    if (standby_screen_ == nullptr || lv_obj_has_flag(standby_screen_, LV_OBJ_FLAG_HIDDEN)) {
        ESP_LOGW(TAG, "UpdateStandbyScreen skipped - standby_screen_=%p, hidden=%d", 
                 standby_screen_, standby_screen_ ? lv_obj_has_flag(standby_screen_, LV_OBJ_FLAG_HIDDEN) : 0);
        return;
    }

    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Log time values for debugging
    ESP_LOGI(TAG, "UpdateStandbyScreen: year=%d, mon=%d, day=%d, hour=%d, min=%d, sec=%d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Update date
    char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &timeinfo);
    ESP_LOGI(TAG, "Setting date label: %s", date_buf);
    lv_label_set_text(date_label_, date_buf);

    // Update weekday
    char weekday_buf[16];
    strftime(weekday_buf, sizeof(weekday_buf), "%A", &timeinfo);
    ESP_LOGI(TAG, "Setting weekday label: %s", weekday_buf);
    lv_label_set_text(weekday_label_, weekday_buf);

    // Update time
    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
    ESP_LOGI(TAG, "Setting time label: '%s' (time_label_=%p)", time_buf, time_label_);
    if (time_label_ != nullptr) {
        lv_label_set_text(time_label_, time_buf);
        // Force refresh
        lv_obj_invalidate(time_label_);
        ESP_LOGI(TAG, "Time label text set and invalidated");
    } else {
        ESP_LOGE(TAG, "time_label_ is NULL!");
    }

    // Update temperature and humidity
    try {
        // Try to get sensor data
        auto& sensor_manager = SensorManager::GetInstance();
        std::string temp_humidity_str = sensor_manager.GetTemperatureHumidityString();
        ESP_LOGI(TAG, "Setting temp_humidity label: %s", temp_humidity_str.c_str());
        lv_label_set_text(temp_humidity_label_, temp_humidity_str.c_str());
    } catch (...) {
        // If sensor manager is not initialized, show default value
        ESP_LOGW(TAG, "Sensor manager not available, showing default value");
        lv_label_set_text(temp_humidity_label_, "--.-°C / --.-%");
    }
}

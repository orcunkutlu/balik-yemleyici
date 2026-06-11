#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace fishmaster_ui {

class FishMasterUI : public display::DisplayBuffer, public PollingComponent {
 public:
  // Component lifecycle
  void setup() override;
  void update() override;
  void dump_config() override;
  
  // DisplayBuffer implementation
  display::DisplayType get_display_type() override {
    return display::DisplayType::DISPLAY_TYPE_COLOR;
  }
  int get_width_internal() override { return 320; }
  int get_height_internal() override { return 240; }
  
  // Public API
  void set_temperature(float temp);
  void set_wifi_connected(bool connected);
  void handle_touch(int x, int y);

 protected:
  void draw_internal() override;
  
 private:
  // ===== RENDERING =====
  void draw_boot_splash();
  void draw_home_static();
  void draw_home_dynamic();
  void draw_navbar();
  void draw_header(const char* title);
  void draw_alert_strip(bool alert_active);
  void draw_cartoony_fish(int cx, int cy);
  void draw_wifi_status();
  void draw_analytics();
  void draw_history();
  void draw_settings();
  
  // ===== TOUCH =====
  void process_touch(int x, int y);
  bool point_in_rect(int x, int y, int rx, int ry, int rw, int rh);
  
  // ===== STATE =====
  float water_temp_ = NAN;
  bool wifi_connected_ = false;
  
  // Redraw tracking
  float last_temp_ = -999.0f;
  bool last_wifi_ = false;
  bool last_alert_ = false;
  
  // Alert ticker
  int alert_scroll_x_ = 0;
  uint32_t last_scroll_ms_ = 0;
  char alert_msg_[64] = "";
  
  // Page management
  enum class Page { HOME, ANALYTICS, HISTORY, SETTINGS };
  Page current_page_ = Page::HOME;
  bool needs_full_redraw_ = true;
  
  // History buffer
  static constexpr int HISTORY_SIZE = 30;
  float temp_history_[HISTORY_SIZE];
  int history_index_ = 0;
  int history_count_ = 0;
  uint32_t last_data_save_ms_ = 0;
  
  // Touch debounce
  uint32_t last_touch_ms_ = 0;
  
  // ===== COLORS (RGB565) =====
  static constexpr uint16_t COL_BG = 0x0A2A;
  static constexpr uint16_t COL_CARD = 0x1125;
  static constexpr uint16_t COL_TEXT_PRI = 0xFFFF;
  static constexpr uint16_t COL_TEXT_SEC = 0x7BFF;
  static constexpr uint16_t COL_ACC_TEMP = 0xF1A6;
  static constexpr uint16_t COL_ACC_PH = 0x2E9A;
  static constexpr uint16_t COL_ACC_TURB = 0xFE60;
  
  // ===== LAYOUT =====
  static constexpr int SCREEN_W = 320;
  static constexpr int SCREEN_H = 240;
};

}  // namespace fishmaster_ui
}  // namespace esphome
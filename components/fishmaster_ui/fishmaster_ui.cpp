#include "fishmaster_ui.h"

// TFT_eSPI ve XPT2046 - build_flags'te tanımlı
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

namespace esphome {
  namespace fishmaster_ui {

    // ===== STATIC INSTANCES =====
    static TFT_eSPI tft;
    static XPT2046_Touchscreen touch(TOUCH_CS);

    // Touch calibration
    static constexpr int TOUCH_RAW_X_MIN = 340;
    static constexpr int TOUCH_RAW_X_MAX = 3860;
    static constexpr int TOUCH_RAW_Y_MIN = 200;
    static constexpr int TOUCH_RAW_Y_MAX = 3860;

    // ============================================================================
    // COMPONENT LIFECYCLE
    // ============================================================================

    void FishMasterUI::setup() {
      ESP_LOGI("FishMaster", "Initializing TFT...");

      // SPI init
      SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);

      // TFT pins
      pinMode(TFT_CS, OUTPUT);
      digitalWrite(TFT_CS, HIGH);
      pinMode(TFT_RST, OUTPUT);
      digitalWrite(TFT_RST, HIGH);
      delay(20);
      digitalWrite(TFT_RST, LOW);
      delay(20);
      digitalWrite(TFT_RST, HIGH);
      delay(120);

      // Touch pins
      pinMode(TOUCH_CS, OUTPUT);
      digitalWrite(TOUCH_CS, HIGH);

      // Init displays
      tft.init();
      tft.setRotation(3);

      touch.begin();
      touch.setRotation(3);

      // Clear history
      for (int i = 0; i < HISTORY_SIZE; i++) {
        temp_history_[i] = NAN;
      }

      // Boot splash
      draw_boot_splash();
      delay(1500);

      needs_full_redraw_ = true;
      ESP_LOGI("FishMaster", "Dashboard ready");
    }

    void FishMasterUI::update() {
      uint32_t now = millis();
      if (now - last_data_save_ms_ >= 30000) {
        last_data_save_ms_ = now;
        if (!isnan(water_temp_)) {
          temp_history_[history_index_] = water_temp_;
          history_index_ = (history_index_ + 1) % HISTORY_SIZE;
          if (history_count_ < HISTORY_SIZE) history_count_++;
        }
      }
    }

    void FishMasterUI::dump_config() {
      ESP_LOGCONFIG("FishMaster", "FishMaster UI Display");
      ESP_LOGCONFIG("FishMaster", "  Resolution: 320x240");
      ESP_LOGCONFIG("FishMaster", "  Driver: ILI9341");
      ESP_LOGCONFIG("FishMaster", "  Touch: XPT2046");
      ESP_LOGCONFIG("FishMaster", "  History: %d points", HISTORY_SIZE);
    }

    // ============================================================================
    // DisplayBuffer IMPLEMENTATION
    // ============================================================================

    void FishMasterUI::draw_internal() {
      // Process touch
      if (touch.touched()) {
        digitalWrite(TFT_CS, HIGH);
        digitalWrite(TOUCH_CS, LOW);
        TS_Point p = touch.getPoint();
        digitalWrite(TOUCH_CS, HIGH);

        if (!(p.x == 0 && p.y == 0) && !(p.x >= 4095 && p.y >= 4095 && p.z >= 4095)) {
          int x = map(p.x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, SCREEN_W);
          int y = map(p.y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, SCREEN_H);
          x = constrain(x, 0, SCREEN_W - 1);
          y = constrain(y, 0, SCREEN_H - 1);

          if (!(x == 0 && y == 0)) {
            uint32_t now = millis();
            if (now - last_touch_ms_ >= 250) {
              last_touch_ms_ = now;
              process_touch(x, y);
            }
          }
        }
      }

      // Render logic
      if (needs_full_redraw_) {
        switch (current_page_) {
          case Page::HOME:
            draw_home_static();
            break;
          case Page::ANALYTICS:
            draw_analytics();
            break;
          case Page::HISTORY:
            draw_history();
            break;
          case Page::SETTINGS:
            draw_settings();
            break;
        }
        needs_full_redraw_ = false;
      }

      // Dynamic updates (Home page only)
      if (current_page_ == Page::HOME && !needs_full_redraw_) {
        bool alert = !isnan(water_temp_) && (water_temp_ < 22.0f || water_temp_ > 30.0f);
        draw_alert_strip(alert);
        draw_home_dynamic();
      }
    }

    // ============================================================================
    // PUBLIC API
    // ============================================================================

    void FishMasterUI::set_temperature(float temp) {
      water_temp_ = temp;
    }

    void FishMasterUI::set_wifi_connected(bool connected) {
      if (connected != wifi_connected_) {
        wifi_connected_ = connected;
        ESP_LOGI("FishMaster", "WiFi %s", connected ? "CONNECTED" : "DISCONNECTED");
      }
    }

    void FishMasterUI::handle_touch(int x, int y) {
      uint32_t now = millis();
      if (now - last_touch_ms_ < 250) return;
      last_touch_ms_ = now;
      process_touch(x, y);
    }

    // ============================================================================
    // TOUCH HANDLING
    // ============================================================================

    void FishMasterUI::process_touch(int x, int y) {
      ESP_LOGD("FishMaster", "Touch: X=%d Y=%d", x, y);

      if (y >= 212) {
        int tab = x / 80;
        if (tab >= 0 && tab < 4) {
          Page new_page = static_cast<Page>(tab);
          if (new_page != current_page_) {
            current_page_ = new_page;
            needs_full_redraw_ = true;
            ESP_LOGI("FishMaster", "Page: %d", tab);
          }
        }
        return;
      }
    }

    bool FishMasterUI::point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
      return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
    }

    // ============================================================================
    // RENDERING: Boot Splash
    // ============================================================================

    void FishMasterUI::draw_boot_splash() {
      tft.fillScreen(COL_BG);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COL_TEXT_PRI, COL_BG);
      tft.setTextFont(4);
      tft.drawString("FISHMASTER", SCREEN_W / 2, SCREEN_H / 2 - 20);

      tft.setTextColor(COL_TEXT_SEC, COL_BG);
      tft.setTextFont(2);
      tft.drawString("Smart Aquarium System", SCREEN_W / 2, SCREEN_H / 2 + 10);
      tft.fillRect(SCREEN_W / 2 - 80, SCREEN_H / 2, 160, 2, COL_ACC_TEMP);
    }

    // ============================================================================
    // RENDERING: Home Page
    // ============================================================================

    void FishMasterUI::draw_home_static() {
      tft.fillScreen(COL_BG);
      draw_header("FISHMASTER");

      int card_x = 60;
      int card_y = 45;
      int card_w = 200;
      int card_h = 120;

      tft.fillRoundRect(card_x, card_y, card_w, card_h, 16, COL_CARD);
      tft.fillRoundRect(card_x, card_y, card_w, 6, 4, COL_ACC_TEMP);
      tft.fillRect(card_x, card_y + 4, card_w, 2, COL_ACC_TEMP);

      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(COL_TEXT_SEC, COL_CARD);
      tft.setTextFont(2);
      tft.drawString("WATER TEMPERATURE", card_x + card_w / 2, card_y + 25);
      tft.drawString("°C", card_x + card_w / 2, card_y + 95);

      draw_cartoony_fish(270, 180);

      tft.fillRoundRect(20, 175, 280, 45, 12, COL_CARD);
      tft.fillRoundRect(20, 175, 280, 4, 4, COL_ACC_PH);
      tft.setTextDatum(ML_DATUM);
      tft.setTextFont(2);
      tft.setTextColor(COL_TEXT_SEC, COL_CARD);
      tft.drawString("STATUS", 32, 192);
      tft.setTextFont(4);
      tft.setTextColor(COL_ACC_PH, COL_CARD);
      tft.drawString("ALL OK", 90, 200);
      tft.fillCircle(280, 200, 8, COL_ACC_PH);

      draw_wifi_status();
      draw_navbar();

      last_temp_ = -999.0f;
      last_wifi_ = !wifi_connected_;
      last_alert_ = false;
    }

    void FishMasterUI::draw_home_dynamic() {
      char buf[16];
      int card_x = 60;
      int card_y = 45;
      int card_w = 200;

      if (water_temp_ != last_temp_) {
        tft.fillRect(card_x + 10, card_y + 45, card_w - 20, 40, COL_CARD);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_TEXT_PRI, COL_CARD);
        tft.setTextFont(7);
        snprintf(buf, sizeof(buf), isnan(water_temp_) ? "--.-" : "%.1f", water_temp_);
        tft.drawString(buf, card_x + card_w / 2, card_y + 65);
        last_temp_ = water_temp_;
      }

      if (wifi_connected_ != last_wifi_) {
        draw_wifi_status();
        last_wifi_ = wifi_connected_;
      }
    }

    void FishMasterUI::draw_wifi_status() {
      tft.fillRect(220, 4, 96, 28, COL_BG);
      tft.setTextDatum(MR_DATUM);
      tft.setTextFont(2);
      if (wifi_connected_) {
        tft.fillCircle(302, 18, 5, COL_ACC_PH);
        tft.setTextColor(COL_TEXT_PRI, COL_BG);
        tft.drawString("ONLINE", 290, 18);
      } else {
        tft.fillCircle(302, 18, 5, COL_ACC_TEMP);
        tft.setTextColor(COL_TEXT_SEC, COL_BG);
        tft.drawString("LOCAL", 290, 18);
      }
    }

    // ============================================================================
    // RENDERING: Alert Strip
    // ============================================================================

    void FishMasterUI::draw_alert_strip(bool alert) {
      const int strip_x = 20;
      const int strip_y = 175;
      const int strip_w = 280;
      const int strip_h = 45;
      const int mid_y = strip_y + strip_h / 2;
      const int dot_x = strip_x + strip_w - 20;

      if (!alert) {
        if (!last_alert_) return;
        last_alert_ = false;
        alert_scroll_x_ = 0;

        tft.fillRoundRect(strip_x, strip_y, strip_w, 4, 4, COL_ACC_PH);
        tft.fillRoundRect(strip_x, strip_y + 5, strip_w, strip_h - 5, 10, COL_CARD);
        tft.setTextFont(2);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COL_TEXT_SEC, COL_CARD);
        tft.drawString("STATUS", 32, mid_y);
        tft.setTextFont(4);
        tft.setTextColor(COL_ACC_PH, COL_CARD);
        tft.drawString("ALL OK", 90, mid_y);
        tft.fillCircle(dot_x, mid_y, 8, COL_ACC_PH);
        return;
      }

      if (!last_alert_) {
        last_alert_ = true;
        alert_scroll_x_ = strip_w;
        last_scroll_ms_ = millis();

        tft.fillRoundRect(strip_x, strip_y, strip_w, 4, 4, COL_ACC_TEMP);
        tft.fillRoundRect(strip_x, strip_y + 5, strip_w, strip_h - 5, 10, COL_CARD);

        tft.setTextFont(4);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COL_ACC_TEMP, COL_CARD);
        tft.drawString("!", 30, mid_y);
        tft.fillCircle(dot_x, mid_y, 8, COL_ACC_TEMP);

        snprintf(alert_msg_, sizeof(alert_msg_),
                 "Temp %.1fC OUT OF RANGE (22-30C)    ", water_temp_);
      }

      uint32_t now = millis();
      if (now - last_scroll_ms_ < 40) return;
      last_scroll_ms_ = now;
      alert_scroll_x_ -= 2;

      int msg_w = tft.textWidth(alert_msg_);
      if (alert_scroll_x_ < -msg_w) alert_scroll_x_ = strip_w - 50;

      const int vp_x = 50;
      const int vp_y = strip_y + 10;
      const int vp_w = strip_w - 80;
      const int vp_h = strip_h - 20;

      tft.setViewport(vp_x, vp_y, vp_w, vp_h);
      tft.fillRect(0, 0, vp_w, vp_h, COL_CARD);
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(COL_TEXT_PRI, COL_CARD);
      tft.setTextFont(2);
      tft.drawString(alert_msg_, alert_scroll_x_, vp_h / 2);
      tft.resetViewport();
    }

    // ============================================================================
    // RENDERING: Analytics Page
    // ============================================================================

    void FishMasterUI::draw_analytics() {
      tft.fillScreen(COL_BG);
      draw_header("TEMPERATURE HISTORY");

      int g_x = 40;
      int g_y = 50;
      int g_w = 260;
      int g_h = 130;

      tft.drawFastVLine(g_x, g_y, g_h, COL_TEXT_SEC);
      tft.drawFastHLine(g_x, g_y + g_h, g_w, COL_TEXT_SEC);

      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(COL_TEXT_SEC, COL_BG);
      tft.setTextFont(2);
      tft.drawString("30C", g_x - 4, g_y + 10);
      tft.drawString("20C", g_x - 4, g_y + g_h - 10);

      if (history_count_ == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Collecting data...", g_x + g_w / 2, g_y + g_h / 2);
      } else {
        float max_t = 32.0f;
        float min_t = 18.0f;
        int step_x = g_w / HISTORY_SIZE;

        int prev_x = -1, prev_y = -1;
        int idx = (history_index_ - history_count_ + HISTORY_SIZE) % HISTORY_SIZE;

        for (int i = 0; i < history_count_; i++) {
          float t = temp_history_[idx];
          if (!isnan(t)) {
            int px = g_x + i * step_x;
            float norm = constrain((t - min_t) / (max_t - min_t), 0.0f, 1.0f);
            int py = g_y + g_h - (int)(norm * g_h);

            if (prev_x != -1) {
              tft.drawLine(prev_x, prev_y, px, py, COL_ACC_TEMP);
              tft.drawLine(prev_x, prev_y + 1, px, py + 1, COL_ACC_TEMP);
            }
            prev_x = px;
            prev_y = py;
          }
          idx = (idx + 1) % HISTORY_SIZE;
        }
      }

      draw_navbar();
    }

    // ============================================================================
    // RENDERING: History & Settings
    // ============================================================================

    void FishMasterUI::draw_history() {
      tft.fillScreen(COL_BG);
      draw_header("EVENT LOG");

      const char* events[] = {"System booted", "WiFi connected", "Sensor ready"};
      int y = 55;

      tft.setTextDatum(ML_DATUM);
      tft.setTextFont(2);

      for (int i = 0; i < 3; i++) {
        tft.fillRoundRect(20, y, 280, 30, 8, COL_CARD);
        tft.setTextColor(COL_TEXT_PRI, COL_CARD);
        tft.drawString(events[i], 32, y + 15);
        y += 36;
      }

      draw_navbar();
    }

    void FishMasterUI::draw_settings() {
      tft.fillScreen(COL_BG);
      draw_header("SETTINGS");

      const char* settings[] = {
        "Alert Thresholds",
        "Display Brightness",
        "Data Interval",
        "Factory Reset"
      };

      int y = 55;
      tft.setTextDatum(ML_DATUM);
      tft.setTextFont(2);
      tft.setTextColor(COL_TEXT_PRI, COL_BG);

      for (int i = 0; i < 4; i++) {
        tft.fillRoundRect(20, y, 280, 32, 8, COL_CARD);
        tft.drawString(settings[i], 32, y + 16);
        y += 40;
      }

      draw_navbar();
    }

    // ============================================================================
    // RENDERING: Common Elements
    // ============================================================================

    void FishMasterUI::draw_header(const char* title) {
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(COL_TEXT_PRI, COL_BG);
      tft.setTextFont(4);
      tft.drawString(title, 16, 18);
      tft.drawFastHLine(0, 34, SCREEN_W, COL_CARD);
    }

    void FishMasterUI::draw_navbar() {
      tft.fillRect(0, 212, SCREEN_W, 28, COL_CARD);

      const char* labels[] = {"HOME", "CHART", "LOG", "SET"};
      int tab_w = SCREEN_W / 4;

      tft.setTextFont(2);
      for (int i = 0; i < 4; i++) {
        bool active = (static_cast<int>(current_page_) == i);
        uint16_t color = active ? COL_ACC_TEMP : COL_TEXT_SEC;
        int x = i * tab_w;

        tft.setTextColor(color, COL_CARD);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(labels[i], x + tab_w / 2, 226);

        if (active) {
          tft.fillRect(x + 20, 212, tab_w - 40, 2, COL_ACC_TEMP);
        }
      }
    }

    void FishMasterUI::draw_cartoony_fish(int cx, int cy) {
      tft.fillTriangle(cx - 20, cy, cx - 35, cy - 15, cx - 35, cy + 15, COL_ACC_TEMP);
      tft.fillCircle(cx, cy, 22, COL_ACC_TEMP);
      tft.fillCircle(cx + 10, cy - 6, 7, COL_TEXT_PRI);
      tft.fillCircle(cx + 12, cy - 6, 3, COL_CARD);
      tft.fillCircle(cx + 20, cy + 8, 5, COL_ACC_TURB);
      tft.fillCircle(cx + 18, cy + 8, 4, COL_ACC_TEMP);
      tft.fillTriangle(cx, cy - 22, cx - 5, cy - 32, cx + 5, cy - 22, COL_ACC_TEMP);
    }

  }  // namespace fishmaster_ui
}  // namespace esphome

#include "ds3231.h"

namespace esphome {
  namespace ds3231 {

    static const char *const TAG = "ds3231";

    void DS3231::setup() {
      ESP_LOGI(TAG, "DS3231 başlatılıyor...");

      if (!this->read_rtc_()) {
        ESP_LOGE(TAG, "RTC, 0x%02X I2C adresinde bulunamadı!", this->address_);
        this->mark_failed();
        return;
      }

      this->found_ = true;
      ESP_LOGI(TAG, "RTC bulundu (I2C Adresi: 0x%02X)", this->address_);
      this->restore_system_time();

      auto t = this->get_time();

      if (t.year > 2000) {
        ESP_LOGI(TAG, "RTC saati okundu: %04d-%02d-%02d %02d:%02d:%02d",
                 t.year, t.month, t.day, t.hour, t.minute, t.second);
      }

      // Açılışta (Boot) SNTP referansı varsa senkronize et
      if (this->rtc_ != nullptr) {
        auto ref_now = this->rtc_->now();
        if (ref_now.is_valid()) {
          this->sync();
          ESP_LOGI(TAG, "RTC, açılış sırasında SNTP (Internet) saati ile eşitlendi.");
        }
      }
    }

    void DS3231::restore_system_time() {
      auto rtc_now = this->now();

      if (!rtc_now.is_valid()) {
        ESP_LOGW(TAG, "RTC saati geçersiz, sistem saati geri yüklenemiyor!");
        return;
      }

      struct timeval tv;
      tv.tv_sec = rtc_now.timestamp;
      tv.tv_usec = 0;

      settimeofday(&tv, nullptr);

      ESP_LOGI(
        TAG,
        "Sistem saati RTC'den geri yüklendi: %04d-%02d-%02d %02d:%02d:%02d",
        rtc_now.year,
        rtc_now.month,
        rtc_now.day_of_month,
        rtc_now.hour,
        rtc_now.minute,
        rtc_now.second
      );
    }

    void DS3231::loop() {
      // Boş — update() periyodik olarak çalışıyor
    }

    void DS3231::update() {
      // 1. Süre kontrolü: Tanımlanan aralık milisaniye cinsinden kontrol ediliyor.
      uint32_t current_time = millis();
      if (this->last_update_ != 0 && (current_time - this->last_update_ < (this->update_interval_min_ * 60000))) {
        return; // Henüz belirlenen zaman geçmedi, işlemi atla
      }
      this->last_update_ = current_time; // Süre geçtiyse, son güncellemeyi şu an olarak kaydet

      ESP_LOGV(TAG, "update() çağrıldı.");

      if (this->rtc_ == nullptr) {
        ESP_LOGV(TAG, "Referans saat kaynağı tanımlı değil, senkronizasyon atlanıyor.");
        return;
      }

      auto ref_now = this->rtc_->now();
      if (!ref_now.is_valid()) {
        ESP_LOGW(TAG, "Referans saat (SNTP) henüz geçerli değil, RTC senkronizasyonu atlanıyor.");
        return;
      }

      this->sync();
    }

    void DS3231::dump_config() {
      ESP_LOGCONFIG(TAG, "DS3231 RTC Ayarları:");
      ESP_LOGCONFIG(TAG, "  I2C Adresi: 0x%02X", this->address_);
      ESP_LOGCONFIG(TAG, "  Güncelleme Aralığı: %lu dk", this->update_interval_min_);
      ESP_LOGCONFIG(TAG, "  Okuma Aralığı: %lu dk", this->read_interval_min_);
      LOG_I2C_DEVICE(this);
      if (this->rtc_ != nullptr) {
        ESP_LOGCONFIG(TAG, "  Referans Saat Kaynağı: Yapılandırıldı");
      } else {
        ESP_LOGCONFIG(TAG, "  Referans Saat Kaynağı: Yok");
      }
    }

    void DS3231::sync() {
      if (this->rtc_ == nullptr) {
        ESP_LOGW(TAG, "Yapılandırılmış referans saat kaynağı bulunamadı!");
        return;
      }

      auto ref_now = this->rtc_->now();
      if (!ref_now.is_valid()) {
        ESP_LOGW(TAG, "Referans saat geçerli değil, RTC senkronizasyonu atlanıyor!");
        return;
      }

      this->set_time(ref_now.year, ref_now.month, ref_now.day_of_month,
                     ref_now.hour, ref_now.minute, ref_now.second, ref_now.day_of_week);
      ESP_LOGI(TAG, "RTC referans saatten eşitlendi: %04d-%02d-%02d %02d:%02d:%02d",
               ref_now.year, ref_now.month, ref_now.day_of_month,
               ref_now.hour, ref_now.minute, ref_now.second);
    }

    void DS3231::set_time(int year, int month, int day, int hour, int minute, int second, int day_of_week) {
      uint8_t tmpbuffer[7];
      tmpbuffer[0] = this->dec_to_bcd(second);
      tmpbuffer[1] = this->dec_to_bcd(minute);
      tmpbuffer[2] = this->dec_to_bcd(hour);
      tmpbuffer[3] = this->dec_to_bcd(day_of_week);
      tmpbuffer[4] = this->dec_to_bcd(day);
      tmpbuffer[5] = this->dec_to_bcd(month);
      tmpbuffer[6] = this->dec_to_bcd(year - 2000);

      if (!this->write_bytes(0, tmpbuffer, 7)) {
        ESP_LOGE(TAG, "Saat verisi RTC'ye yazılamadı!");
      } else {
        ESP_LOGD(TAG, "Saat verisi RTC'ye başarıyla yazıldı.");
      }
    }

    bool DS3231::read_rtc_() {
      if (!this->read_bytes(0, this->databuffer_, 7)) {
        ESP_LOGE(TAG, "RTC okunurken hata oluştu (I2C hatası)!");
        return false;
      }
      return true;
    }

    Time DS3231::get_time() {
      if (!this->read_rtc_()) {
        ESP_LOGE(TAG, "Saat yazmaçları (registers) okunamadı!");
        return Time{0, 0, 0, 0, 0, 0, 0};
      }

      Time t{
        .second = this->bcd_to_dec(this->databuffer_[0]),
        .minute = this->bcd_to_dec(this->databuffer_[1]),
        .hour = this->bcd_to_dec(this->databuffer_[2]),
        .day_of_week = this->bcd_to_dec(this->databuffer_[3]),
        .day = this->bcd_to_dec(this->databuffer_[4]),
        .month = this->bcd_to_dec(this->databuffer_[5]),
        .year = this->bcd_to_dec(this->databuffer_[6]) + 2000
      };

      return t;
    }

    ESPTime DS3231::now() {
      Time t = this->get_time();

      if (t.year < 2000 || t.month < 1 || t.month > 12 || t.day < 1 || t.day > 31) {
        ESP_LOGW(TAG, "Geçersiz RTC tarih değerleri okundu: %04d-%02d-%02d", t.year, t.month, t.day);
        return ESPTime{};
      }

      ESPTime rtc_time{
        .second = (uint8_t)t.second,
        .minute = (uint8_t)t.minute,
        .hour = (uint8_t)t.hour,
        .day_of_week = (uint8_t)t.day_of_week,
        .day_of_month = (uint8_t)t.day,
        .day_of_year = 1,
        .month = (uint8_t)t.month,
        .year = (uint16_t)t.year,
        .is_dst = false,
        .timestamp = 0
      };

      rtc_time.recalc_timestamp_local();

      if (!rtc_time.is_valid()) {
        ESP_LOGW(TAG, "Hesaplanan zaman damgası (timestamp) geçersiz!");
        return ESPTime{};
      }

      return rtc_time;
    }

    uint8_t DS3231::dec_to_bcd(int val) {
      return ((val / 10 * 16) + (val % 10));
    }

    int DS3231::bcd_to_dec(uint8_t val) {
      return ((val / 16 * 10) + (val % 16));
    }

  }  // namespace ds3231
}  // namespace esphome
#define DEFAULT_DIR      "00000000"
#define DEFAULT_FILE     "00000000.000"
#define DEFAULT_MEDIA_ID "0000000000000000000";

extern "C" {
#include "user_interface.h"
}

static uint32_t fnv_1_hash_32(uint8_t *bytes, size_t length) {
  static const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
  static const uint32_t FNV_PRIME_32 = 16777619U;
  uint32_t hash = FNV_OFFSET_BASIS_32;
  for (size_t i = 0 ; i < length ; ++i) hash = (FNV_PRIME_32 * hash) ^ (bytes[i]);
  return hash;
}

template <class T> uint32_t calc_hash(T& data) {
  return fnv_1_hash_32(((uint8_t*)&data) + sizeof(data.hash), sizeof(T) - sizeof(data.hash));
}

String gopro_dir  = DEFAULT_DIR;
String gopro_file = DEFAULT_FILE;
String media_id   = DEFAULT_MEDIA_ID;

struct {
  uint32_t hash;
  bool gopro_mode;
  bool formatspiffs;
  float Temperature;
  int twitter_phase;
  int gopro_size;
  int chunked_no;
  int attempt_this;
  char gopro_dir[32];
  char gopro_file[32];
  char media_id[32];
} rtc_boot_mode;

bool rtc_config_read() {
  bool ok = system_rtc_mem_read(65, &rtc_boot_mode, sizeof(rtc_boot_mode));
  uint32_t hash = calc_hash(rtc_boot_mode);
  if (!ok || rtc_boot_mode.hash != hash) {
    rtc_boot_mode.gopro_mode    = false;
    rtc_boot_mode.formatspiffs  = false;
    rtc_boot_mode.Temperature   = 0;
    rtc_boot_mode.gopro_size    = 0;
    rtc_boot_mode.twitter_phase = 0;
    rtc_boot_mode.chunked_no    = 0;
    rtc_boot_mode.attempt_this  = 0;
    ok = false;
  } else {
    gopro_dir = rtc_boot_mode.gopro_dir;
    gopro_file = rtc_boot_mode.gopro_file;
    media_id = rtc_boot_mode.media_id;
  }
  return ok;
}

bool rtc_config_save() {
  strncpy(rtc_boot_mode.gopro_dir, gopro_dir.c_str(), sizeof(rtc_boot_mode.gopro_dir));
  strncpy(rtc_boot_mode.gopro_file, gopro_file.c_str(), sizeof(rtc_boot_mode.gopro_file));
  strncpy(rtc_boot_mode.media_id, media_id.c_str(), sizeof(rtc_boot_mode.media_id));
  rtc_boot_mode.hash = calc_hash(rtc_boot_mode);
  bool ok = system_rtc_mem_write(65, &rtc_boot_mode, sizeof(rtc_boot_mode));
  if (!ok) {
    ok = false;
  }
  return ok;
}


void readConfig_helper() {
  if (!rtc_config_read()) {
    Serial.println("[CONFIG] read fail");
  } else {
    Serial.println("[CONFIG] loaded");

    Serial.print("gopro_mode : "); Serial.println(rtc_boot_mode.gopro_mode);
    Serial.print("formatspiffs : "); Serial.println(rtc_boot_mode.formatspiffs);
    Serial.print("Temperature : "); Serial.println(rtc_boot_mode.Temperature);
    Serial.print("gopro_size : "); Serial.println(rtc_boot_mode.gopro_size);
    Serial.print("twitter_phase : "); Serial.println(rtc_boot_mode.twitter_phase);
    Serial.print("chunked_no : "); Serial.println(rtc_boot_mode.chunked_no);
    Serial.print("attempt_this : "); Serial.println(rtc_boot_mode.attempt_this);
    Serial.print("gopro_dir : "); Serial.println(gopro_dir);
    Serial.print("gopro_file : "); Serial.println(gopro_file);
    Serial.print("media_id : "); Serial.println(media_id);
  }
}

void saveConfig_helper() {
  if (!rtc_config_save()) {
    Serial.println("[CONFIG] save fail");
  } else {
    Serial.println("[CONFIG] saved");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.flush();

  readConfig_helper();

  rtc_boot_mode.gopro_mode    = true;
  rtc_boot_mode.formatspiffs  = true;
  rtc_boot_mode.Temperature   = 10;
  rtc_boot_mode.gopro_size    = 3750572;
  rtc_boot_mode.twitter_phase = 1;
  rtc_boot_mode.chunked_no    = 10;
  rtc_boot_mode.attempt_this  = 5;
  
  gopro_dir   = "100GOPRO";
  gopro_file  = "GOPR0003.JPG";
  media_id    = "9223372036854775807";

  saveConfig_helper();

  readConfig_helper();
  Serial.flush();

}

void loop() {

}


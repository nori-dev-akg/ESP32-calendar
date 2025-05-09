#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#define SSID_NAME "xxxxxxxxxxxxxxxxxxxxxx" // WiFi SSID
#define SSID_PASS "xxxxxxxxxxxxxxxxxxxxxx" // WiFi Password

#define FOLDER_ID "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzz" // GoogleDrive folderId


// 外部変数
extern bool stat_wifi;

// 関数プロトタイプ宣言
bool init_wifi();
String url_encode(String str);
bool get_refresh_token(String &code, String &client_id, String &client_secret, String &refresh_token, String &access_token);
bool get_access_token(String &refresh_token, String &client_id, String &client_secret, String &access_token);
void drive_files(String access_token);
int get_pic_drive(String access_token, String image_id, uint8_t *&pic);
bool read_spiffs(String &client_id, String &client_secret, String &refresh_token);
bool write_spiffs(String client_id, String client_secret, String refresh_token);
bool init_gdrive();
int get_pic(const char *fileid, uint8_t *&pic);
bool authenticate(String &client_id, String &client_secret, String &refresh_token, String &access_token);
bool get_random_pic(String &id, String &name, int &size);
String reget_access_token();

#endif
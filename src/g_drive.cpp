#include "g_drive.h"

#define TOKEN_FILE "/access_token.txt"

const char *ssid = SSID_NAME;
const char *pass = SSID_PASS;
#define WIFI_TIMEOUT 30000

String global_access_token = "";

#define GDRIVE_RETRY 3

// ファイル情報
struct FileInfo {
  String id;
  String name;
  long size;
};
#define MAX_FILES 100
FileInfo file_list[MAX_FILES];
int file_count = 0;

bool init_gdrive() {
  randomSeed(millis());

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return false;
  }
  
  global_access_token = reget_access_token();
  if (global_access_token == "") {
    write_spiffs("", "", ""); // Clear the token file if access token is empty
    global_access_token = reget_access_token();
    if (global_access_token == "") {
      return false;
    }
  }

  drive_files(global_access_token);

  return true;
}

String reget_access_token() {

  String client_id = "";
  String client_secret = "";
  String refresh_token = "";
  String access_token = "";

  if (!read_spiffs(client_id, client_secret, refresh_token)) {
    Serial.println("SPIFFS read error");
    return "";
  }

  if (refresh_token.length() == 0) {
    Serial.println("Refresh token is empty. Please authenticate.");
    while (!authenticate(client_id, client_secret, refresh_token, access_token)) {
      Serial.println("Authentication failed.");
      delay(1000);
    }
    write_spiffs(client_id, client_secret, refresh_token);
  }

  if (access_token == "") {
    if (!get_access_token(refresh_token, client_id, client_secret, access_token)) {
      Serial.println("Failed to get access token.");
      return "";
    }
  }
  return access_token;
}

// URLエンコード関数
String url_encode(String str) {
  String encoded_string = "";
  for (char c : str) {
    if (isalnum(c) || (c == '-') || (c == '_') || (c == '.') || (c == '~')) {
      encoded_string += c;
    } else if (c == ' ') {
      encoded_string += '+';
    } else {
      char buf[4];
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded_string += buf;
    }
  }
  return encoded_string;
}

bool get_refresh_token(String &code, String &client_id, String &client_secret, String &refresh_token, String &access_token) {
  bool ret = false;
  HTTPClient http;
  http.begin("https://accounts.google.com/o/oauth2/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String post_data = String("grant_type=authorization_code") +
                    "&code=" + url_encode(code) +
                    "&client_id=" + client_id +
                    "&client_secret=" + url_encode(client_secret) +
                    "&redirect_uri=" + url_encode("urn:ietf:wg:oauth:2.0:oob");

  int http_response_code = http.POST(post_data);

  if (http_response_code > 0) {
    if (http_response_code == HTTP_CODE_OK) {
      String payload = http.getString();

      JsonDocument json_buffer;
      DeserializationError error = deserializeJson(json_buffer, payload);

      if (!error) {
        if (json_buffer["access_token"].is<String>()) {
          access_token = json_buffer["access_token"].as<String>();
          Serial.print("Access Token: ");
          Serial.println(access_token);
        } else {
          Serial.println("Access token not found in the response.");
        }
        if (json_buffer["refresh_token"].is<String>()) {
          refresh_token = json_buffer["refresh_token"].as<String>();
          Serial.print("Refresh Token: ");
          Serial.println(refresh_token);
          ret = true;
        }
      } else {
        Serial.print("JSON Deserialization failed: ");
        Serial.println(error.c_str());
        ret = false;
      }
    }
  } else {
    Serial.printf("HTTP POST request failed, error: %s\n", http.errorToString(http_response_code).c_str());
    ret = false;
  }

  http.end();

  return ret;
}

bool get_access_token(String &refresh_token, String &client_id, String &client_secret, String &access_token) {
  bool ret = false;

  String post_data = String("grant_type=refresh_token") +
                    "&refresh_token=" + url_encode(refresh_token) +
                    "&client_id=" + client_id +
                    "&client_secret=" + url_encode(client_secret);

  for (int i = 0; i < GDRIVE_RETRY && !ret; i++) {
    HTTPClient http;
    if (http.begin("https://accounts.google.com/o/oauth2/token")) {
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      int http_response_code = http.POST(post_data);
      if (http_response_code == HTTP_CODE_OK) {
        String payload = http.getString();

        JsonDocument json_buffer;
        DeserializationError error = deserializeJson(json_buffer, payload);

        if (!error) {
          if (json_buffer["access_token"].is<String>()) {
            access_token = json_buffer["access_token"].as<String>();
            Serial.print("New Access Token: ");
            Serial.println(access_token);
            ret = true;
          } else {
            Serial.println("Access token not found in the refresh token response.");
          }
          if (json_buffer["refresh_token"].is<String>()) {
            String new_refresh_token = json_buffer["refresh_token"].as<String>();
            if (new_refresh_token != refresh_token) {
              Serial.print("New Refresh Token: ");
              Serial.println(new_refresh_token);
              refresh_token = new_refresh_token;
              write_spiffs(client_id, client_secret, refresh_token);
            }
          }
        } else {
          Serial.print("JSON Deserialization failed: ");
          Serial.println(error.c_str());
        }
      } else {
        Serial.printf("POST(): request failed, error: %s\n", http.errorToString(http_response_code).c_str());
      }
    } else {
      Serial.println("begin(): Connection failed");
    }

    http.end();

    if (!ret) {
      delay(1000); // wait before retrying
      Serial.printf("Retrying... (%d/%d)\n", i + 1, GDRIVE_RETRY);
    }
  }

  return ret;
}

void drive_files(String access_token) {
  HTTPClient https;
  String query = "?q=%27" + String(FOLDER_ID) + "%27%20in%20parents%20and%20mimeType%3D%27image%2Fjpeg%27&fields=files(id,name,size)";
  String url = "https://www.googleapis.com/drive/v3/files" + query;

  if (https.begin(url)) {
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", "Bearer " + access_token);

    int http_response_code = https.GET();
    String body = "";
    if (http_response_code == HTTP_CODE_OK) {
      body = https.getString();
      Serial.printf("Received data length: %d\n", body.length());

      JsonDocument filter;
      JsonDocument filter_body;
      filter["files"][0]["id"] = true;
      filter["files"][0]["name"] = true;
      filter["files"][0]["size"] = true;
      deserializeJson(filter_body, body, DeserializationOption::Filter(filter));

      int i = 0;
      while (1) {
        String temp_id = filter_body["files"][i]["id"];
        String temp_name = filter_body["files"][i]["name"];
        long temp_size = filter_body["files"][i]["size"];

        if (temp_id.equals("null")) {
          break;
        }

        file_list[file_count].id = temp_id;
        file_list[file_count].name = temp_name;
        file_list[file_count].size = temp_size;
        file_count++;
        i++;
      }
      Serial.println("Total files: " + String(file_count));

      https.end();
    } else {
      Serial.printf("drive_files:HTTP GET failed, error: %s\n", https.errorToString(http_response_code).c_str());
    }
  } else {
    Serial.println("Connection failed");
  }
}

int get_pic_drive(String access_token, String image_id, uint8_t *&pic) {
  HTTPClient https;

  if (https.begin("https://www.googleapis.com/drive/v3/files/" + image_id + "?alt=media")) {
    https.addHeader("Authorization", "Bearer " + access_token);
    int http_response_code = https.GET();
    size_t size = https.getSize();
    if (http_response_code == HTTP_CODE_OK || http_response_code == HTTP_CODE_MOVED_PERMANENTLY) {
      WiFiClient *stream = https.getStreamPtr();
      pic = (uint8_t *)malloc(size);

      size_t offset = 0;
      while (https.connected()) {
        size_t len = stream->available();
        if (!len) {
          delay(1);
          continue;
        }

        stream->readBytes(pic + offset, len);
        offset += len;
        log_d("%d / %d", offset, size);
        if (offset >= size) {
          break;
        }
      }
    } else {
      Serial.printf("get_pic_drive:HTTP GET failed, error: %s\n", https.errorToString(http_response_code).c_str());
    }

    https.end();
    return size;
  } else {
    Serial.println("Connection failed");
  }
  return 0;
}

bool init_wifi() {
  Serial.print("Connecting WiFi.");
  WiFi.begin(ssid, pass);
  bool stat_wifi = false;

  int n;
  long timeout = millis();
  while ((n = WiFi.status()) != WL_CONNECTED && (millis() - timeout) < WIFI_TIMEOUT) {
    Serial.print(".");
    delay(500);
    if (n == WL_NO_SSID_AVAIL || n == WL_CONNECT_FAILED) {
      delay(1000);
      WiFi.reconnect();
    }
  }
  if (n == WL_CONNECTED) {
    stat_wifi = true;
    Serial.println(" connected.");

    configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
    while (time(nullptr) < 9 * 3600L * 2) {
      delay(1000);
    }
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    Serial.printf("Current time: %04d/%02d/%02d %02d:%02d:%02d\n", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  } else {
    stat_wifi = false;
    Serial.println("WiFi connection failed.");
  }
  return stat_wifi;
}

bool read_spiffs(String &client_id, String &client_secret, String &refresh_token) {
  File fd = SPIFFS.open(TOKEN_FILE, "r");
  if (!fd) {
    Serial.println("Config open error");
    return false;
  }
  client_id = fd.readStringUntil('\n');
  client_id.trim();
  Serial.println("Client ID:" + client_id);
  client_secret = fd.readStringUntil('\n');
  client_secret.trim();
  Serial.println("Client Secret:" + client_secret);
  refresh_token = fd.readStringUntil('\n');
  refresh_token.trim();
  Serial.println("Refresh token:" + refresh_token);
  fd.close();
  return true;
}

bool write_spiffs(String client_id, String client_secret, String refresh_token) {
  File fd = SPIFFS.open(TOKEN_FILE, "w");
  if (!fd) {
    Serial.println("Config open error");
    return false;
  }
  fd.println(client_id);
  fd.println(client_secret);
  fd.println(refresh_token);
  fd.close();
  Serial.println("Config write OK");
  return true;
}

bool authenticate(String &client_id, String &client_secret, String &refresh_token, String &access_token) {
  Serial.println("Client ID?");
  while (!Serial.available()) {
    delay(10);
  }
  client_id = Serial.readStringUntil('\n');
  client_id.trim();

  Serial.println("Please access to");
  Serial.println("https://accounts.google.com/o/oauth2/auth?client_id=" + client_id +
                 "&response_type=code&redirect_uri=urn:ietf:wg:oauth:2.0:oob&scope=https://www.googleapis.com/auth/drive");

  Serial.println("authorization code?");
  while (!Serial.available()) {
    delay(10);
  }
  String code = Serial.readStringUntil('\n');
  code.trim();

  Serial.println("Client secret?");
  while (!Serial.available()) {
    delay(10);
  }
  client_secret = Serial.readStringUntil('\n');
  client_secret.trim();

  Serial.println("OK");
  delay(1000);

  bool ret = get_refresh_token(code, client_id, client_secret, refresh_token, access_token);
  if (ret && refresh_token.length() > 0) {
    Serial.println("OK");
    return true;
  } else {
    Serial.println("NG");
    return false;
  }
}

int get_pic(const char *file_id, uint8_t *&pic) {
  int size = get_pic_drive(global_access_token, String(file_id), pic);
  if (size <= 0) {
    global_access_token = reget_access_token();
    size = get_pic_drive(global_access_token, String(file_id), pic);
  }
  return size;
}

bool get_random_pic(String &id, String &name, int &size) {
  if (file_count == 0) {
    Serial.println("File list is empty.");
    return false;
  }

  int random_index = random(file_count);

  name = file_list[random_index].name;
  id = file_list[random_index].id;
  size = file_list[random_index].size;

  return true;
}

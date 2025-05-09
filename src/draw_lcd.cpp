#include "draw_lcd.h"

LGFX lcd;

#define MAX_HOLIDAYS 31
int holidays[MAX_HOLIDAYS];
int holiday_count;
int holiday_month = 0;

bool init_lcd()
{
  lcd.init();
  lcd.setRotation(LCD_ROTATION);
  lcd.fillScreen(TFT_BLACK);

  return true;
}

void fetch_holidays(int year, int month)
{

  HTTPClient http;
  String month_str = String((month < 10) ? "0" : "") + String(month);  // 月は0埋めする必要がある
  String url = "https://api.national-holidays.jp/" + String(year) + "/" + month_str;
  Serial.println("Fetching holidays from: " + url);
  http.begin(url);

  int http_response_code = http.GET();
  if (http_response_code == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      holiday_count = 0;
      for (JsonObject holiday : doc.as<JsonArray>()) {
        if (holiday_count < MAX_HOLIDAYS) {
          String date = holiday["date"].as<String>();
          int day = date.substring(date.lastIndexOf("-") + 1).toInt();
          holidays[holiday_count++] = day;
        } else {
          Serial.println("Too many holidays to store.");
          break;
        }
      }
    } else {
      Serial.println("Failed to parse holiday JSON.");
    }
  } else {
    Serial.printf("Failed to fetch holidays. HTTP code: %d\n", http_response_code);
  }

  http.end();
}

bool is_holiday(int day)
{
  for (int i = 0; i < holiday_count; i++) {
    if (holidays[i] == day) {
      return true; // 祝日
    }
  }
  return false; // 平日
}

bool get_jpeg_width_height(const uint8_t *data, size_t size, int16_t *width, int16_t *height)
{
  *width = -1;
  *height = -1;

  for (size_t i = 0; i < size - 1; ++i) {
    if (data[i] == 0xFF) {
      uint8_t marker = data[i + 1];

      // SOFマーカーをチェック (0xC0 - 0xC3, 0xC5 - 0xC7, 0xC9 - 0xCB, 0xCD - 0xCF)
      if ((marker >= 0xC0 && marker <= 0xCF) && 
          (marker != 0xC4 && marker != 0xC8 && marker != 0xCC)) {
        // マーカーの後に少なくとも8バイトが存在するか確認
        if (i + 8 < size) {
          *height = (data[i + 5] << 8) | data[i + 6];
          *width = (data[i + 7] << 8) | data[i + 8];
          return (*height > 0 && *width > 0); // 幅と高さが取得できたらtrueを返す
        }
      }

      // スキップ可能なマーカー (0xD0 - 0xD9など)
      if (marker == 0xD0 || marker == 0xD1 || marker == 0xD2 || marker == 0xD3 ||
          marker == 0xD4 || marker == 0xD5 || marker == 0xD6 || marker == 0xD7 ||
          marker == 0xD8 || marker == 0xD9) {
        continue;
      }

      // マーカーの長さを取得してスキップ
      if (i + 3 < size) {
        uint16_t segment_length = (data[i + 2] << 8) | data[i + 3];
        i += segment_length + 1; // マーカー全体をスキップ
      }
    }
  }

  return false; // 幅と高さが取得できなかった場合
}

uint8_t *bg_cal = NULL; size_t bg_cal_size = 0;
uint8_t *bg_img = NULL; size_t bg_img_size = 0;

void set_background(uint8_t *cal, size_t cal_size, uint8_t *img, size_t img_size)
{
  if (bg_cal != NULL) {
    free(bg_cal); // 前の画像を解放
  }
  if (bg_img != NULL) {
    free(bg_img); // 前の画像を解放
  }
  bg_cal = cal;
  bg_cal_size = cal_size;
  
  bg_img = img;
  bg_img_size = img_size;
}

void draw_background(bool vertical)
{
  if (bg_cal == NULL || bg_img == NULL) {
    return; // 背景画像が設定されていない場合は何もしない
  }
  
  int lcd_width = lcd.width();
  int lcd_height = lcd.height();
  int image_width, image_height, image_x, image_y, image_off_x, image_off_y;
  int cal_width, cal_height, cal_x, cal_y, cal_off_x, cal_off_y;
  
  if (vertical) {
    image_width = lcd_width / 3 * 2;
    image_height = lcd_height;
    image_x = lcd_width / 3;
    image_y = 0;
    image_off_x = lcd_width / 3;
    image_off_y = 0;
    cal_width = lcd_width / 3;
    cal_height = lcd_height;
    cal_x = 0;
    cal_y = 0;
    cal_off_x = 0;
    cal_off_y = 0;
  } else {
    image_width = lcd_width;
    image_height = round((float)lcd_height / 3 * 2);
    image_x = 0;
    image_y = 0;
    image_off_x = 0;
    image_off_y = 0;
    cal_width = lcd_width;
    cal_height = round((float)lcd_height / 3);
    cal_x = 0;
    cal_y = round((float)lcd_height / 3 * 2);
    cal_off_x = 0;
    cal_off_y = round((float)lcd_height / 3 * 2);
  }
  lcd.drawJpg(bg_cal, bg_cal_size, cal_x, cal_y, cal_width, cal_height, cal_off_x, cal_off_y);
  lcd.drawJpg(bg_img, bg_img_size, image_x, image_y, image_width, image_height, image_off_x, image_off_y);
}

void draw_picture(const uint8_t *data, size_t size, int16_t width, int16_t height)
{
  int16_t original_width = width;
  int16_t original_height = height;

  int lcd_width = lcd.width();
  int lcd_height = lcd.height();
  int image_width, image_height, target_width, target_height;
  int cal_width, cal_height, image_x, image_y;
  int monthyear_height = 0;
 
  if (original_width < original_height)  { // 縦長 vertical
    image_width = lcd_width / 3 * 2;
    image_height = lcd_height;
    target_width = image_width - 20;
    target_height = image_height - 20;
    cal_width = 0;
    cal_height = 0;
    image_x = lcd_width / 3;
    image_y = 0;
  }
  else { // 横長 horizontal
    image_width = lcd_width;
    image_height = lcd_height / 3 * 2;
    target_width = image_width - 20;
    target_height = image_height - 20;
    cal_width = 0;
    cal_height = 0;
    image_x = 0;
    image_y = 0;
  }

  float scale_width = (float)target_width / original_width;
  float scale_height = (float)target_height / original_height;
  float scale = min(scale_width, scale_height); // 小さい方をscaleに
  int offset_x = (target_width - (int)(original_width * scale)) / 2 + image_x + 10;
  int offset_y = (target_height - (int)(original_height * scale)) / 2 + image_y + 10;
  int shadow_offset_x = offset_x + 5; // 影のXオフセット
  int shadow_offset_y = offset_y + 5; // 影のYオフセット

  // 影
  lcd.fillRect(shadow_offset_x, shadow_offset_y, original_width * scale, original_height * scale, TFT_DARKGREY);

  lcd.drawJpg(data, size, offset_x, offset_y, 0, 0, 0, 0, scale);

}

int get_day_color(int day_of_week, int day)
{
  if (day_of_week == 0 || is_holiday(day)) {
    return COLOR_DAY_OF_SUNDAY;
  } else if (day_of_week == 6) {
    return COLOR_DAY_OF_SATURDAY;
  } else {
    return COLOR_DAY_OF_WEEKDAY;
  }
}

void draw_calendar(bool vertical)
{
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  int current_year = timeinfo.tm_year + 1900;
  int current_month = timeinfo.tm_mon + 1;

  const char *months[] = {"January", "February", "March", "April", "May", "June",
                          "July", "August", "September", "October", "November", "December"};

  const char *days_of_week[] = {"S", "M", "T", "W", "T", "F", "S"};
  int current_day = timeinfo.tm_mday;
  int current_wday = timeinfo.tm_wday; // 0: Sunday, 1: Monday, ..., 6: Saturday
  int first_day_of_week = (current_wday - (current_day % 7) + 7) % 7;
  int days_in_month[] = {31, (current_year % 4 == 0 && (current_year % 100 != 0 || current_year % 400 == 0)) ? 
    29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int days = days_in_month[current_month - 1];
  
  int cal_width, cal_height;
  int cell_width, cell_height;
  int start_x = 0, start_y = 0;
  if (vertical) {
    cal_width = lcd.width() / 3;
    cal_height = lcd.height() / 2 - MONTHYEAR_HEIGHT;
    cell_width = (int)round((double)cal_width / 7);
    cell_height = (int)round((double)cal_height / 7);
    start_x = 0;
    start_y = 0 + MONTHYEAR_HEIGHT;
  } else {
    cal_width = lcd.width() / 2;
    cal_height = lcd.height() / 3;
    cell_width = (int)round((double)cal_width / 7);
    cell_height = (int)round((double)cal_height / 7);
    start_x = 0;
    start_y = lcd.height() / 3 * 2;
  }

  lcd.setFont(FONT_CALENDAR);
  lcd.setTextSize(FONT_CALENDAR_SIZE);

  for (int i = 0; i < 7 + days; i++) {
    if (i < 7) {
      lcd.setTextColor(get_day_color(i, 0));

      int day_text_width = lcd.textWidth(days_of_week[i]);
      int day_centered_x = start_x + i * cell_width + (cell_width - day_text_width) / 2;
      lcd.setCursor(day_centered_x, start_y);
      lcd.print(days_of_week[i]);
    } else {
      int d = i - 7 + 1;
      int column = (first_day_of_week + d ) % 7;
      int row = (first_day_of_week + d ) / 7;

      lcd.setTextColor(get_day_color(column, d));

      String day_string = String(d);
      int day_text_width = lcd.textWidth(day_string);
      int day_centered_x = start_x + column * cell_width + (cell_width - day_text_width) / 2;
      int day_centered_y = start_y + (row + 1) * cell_height;
      lcd.setCursor(day_centered_x, day_centered_y);
      lcd.print(d);

      if (d == current_day) {
        int font_height = lcd.fontHeight();
        int underline_y = day_centered_y + font_height + 1;
        lcd.drawFastHLine(day_centered_x, underline_y, day_text_width, TFT_CYAN);
      }
    }
  }
}

void draw_yearmonth(bool vertical, bool calendar)
{

  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  int current_year = timeinfo.tm_year + 1900;
  int current_month = timeinfo.tm_mon + 1;

  const char *months[] = {"January", "February", "March", "April", "May", "June",
                          "July", "August", "September", "October", "November", "December"};
  
  int month_x, month_y;
  int month_width, month_height;
  if (vertical) {
    month_x = 0;
    month_y = 0;
    month_width = lcd.width() / 3;
    month_height = MONTHYEAR_HEIGHT;
  } else {
    if (calendar) {
      month_x = lcd.width() / 2;
      month_y = lcd.height() / 3 * 2;
      month_width = lcd.width() / 2;
      month_height = MONTHYEAR_HEIGHT;
    } else {
      month_x = 0;
      month_y = lcd.height() / 3 * 2;
      month_width = lcd.width() / 2;
      month_height = MONTHYEAR_HEIGHT;
    }
  }

  lcd.setFont(FONT_MONTHYEAR);
  lcd.setTextSize(FONT_MONTHYEAR_SIZE);
  String month_year = String(months[current_month - 1]) + " " + String(current_year);
  int text_width = lcd.textWidth(month_year);
  int font_height = lcd.fontHeight();
  int centered_x = max(month_x + (month_width - text_width) / 2, 0);
  int centered_y = max(month_y + (month_height - font_height) / 2, 0);

  // 影
  lcd.setTextColor(TFT_DARKGRAY);
  lcd.setCursor(centered_x+5, centered_y+5);
  lcd.print(month_year);

  lcd.setTextColor(TFT_WHITE);
  lcd.setCursor(centered_x, centered_y);
  lcd.print(month_year);
}

void draw_today(bool vertical, bool calendar)
{
  draw_yearmonth(vertical, calendar);

  int weekday_x, weekday_y, day_x, day_y;
  int weekday_width, weekday_height, day_width, day_height;
  if(calendar) {
    if (vertical) {
      weekday_x = 0;
      weekday_y = lcd.height() / 2;
      day_x = 0;
      day_y = lcd.height() / 6 * 4;
      weekday_width = lcd.width() / 3;
      weekday_height = lcd.height() / 6;
      day_width = lcd.width() / 3;
      day_height = lcd.height() / 6 * 2;
    } else {
      weekday_x = lcd.width() / 2;
      weekday_y = lcd.height() / 3 * 2 + MONTHYEAR_HEIGHT;
      day_x = lcd.width() / 6 * 4;
      day_y = lcd.height() / 3 * 2 + MONTHYEAR_HEIGHT;
      weekday_width = lcd.width() / 6;
      weekday_height = lcd.height() / 3 - MONTHYEAR_HEIGHT;
      day_width = lcd.width() / 6 * 2;
      day_height = lcd.height() / 3 - MONTHYEAR_HEIGHT;
    }
  } else {
    if (vertical) {
      weekday_x = 0;
      weekday_y = 0 + MONTHYEAR_HEIGHT;
      day_x = 0;
      day_y = lcd.height()/ 2;
      weekday_width = lcd.width() / 3;
      weekday_height = lcd.height() / 2 - MONTHYEAR_HEIGHT;
      day_width = lcd.width() / 3;
      day_height = lcd.height() / 2;
    } else {
      weekday_x = 0;
      weekday_y = lcd.height() / 3 * 2 + MONTHYEAR_HEIGHT;
      day_x = lcd.width() / 2;
      day_y = lcd.height() / 3 * 2;
      weekday_width = lcd.width() / 2;
      weekday_height = lcd.height() / 3 - MONTHYEAR_HEIGHT;
      day_width = lcd.width() / 2;
      day_height = lcd.height() / 3;
    }
  }


  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  const char *days_of_week_short[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  int current_year = timeinfo.tm_year + 1900;
  int current_month = timeinfo.tm_mon + 1;
  int current_day = timeinfo.tm_mday;
  int current_wday = timeinfo.tm_wday; // 0: Sunday, 1: Monday, ..., 6: Saturday

  lcd.setFont(FONT_TODAY);
  
  // 曜日
  lcd.setTextSize(FONT_WEEKDAY_SIZE);
  int font_height = FONT_TODAY_HEIGHT * FONT_WEEKDAY_SIZE;
  String today_weekday = String(days_of_week_short[current_wday]);
  int text_width = lcd.textWidth(today_weekday);
  int centered_x = max(0, weekday_x + (weekday_width - text_width) / 2);
  int centered_y = max(0, weekday_y + (weekday_height - font_height) / 2);

  //lcd.fillRect(weekday_x, weekday_y, weekday_width, weekday_height, TFT_PINK);

  // 影
  lcd.setTextColor(TFT_DARKGRAY);
  lcd.setCursor(centered_x + 5, centered_y + 5);
  lcd.print(today_weekday);

  lcd.setTextColor(get_day_color(current_wday, current_day));
  lcd.setCursor(centered_x, centered_y);
  lcd.print(today_weekday);

  // 日付
  lcd.setTextSize(FONT_DAY_SIZE);
  font_height = FONT_TODAY_HEIGHT * FONT_DAY_SIZE;
  String today_day = String(current_day);
  text_width = lcd.textWidth(today_day);
  centered_x = max(0, day_x + (day_width - text_width) / 2);
  centered_y = max(0, day_y + (day_height - font_height) / 2);

  // 影
  lcd.setTextColor(TFT_DARKGRAY);
  lcd.setCursor(centered_x + 5, centered_y + 5);
  lcd.print(today_day);

  lcd.setTextColor(get_day_color(current_wday, current_day));
  lcd.setCursor(centered_x, centered_y);
  lcd.print(today_day);
}

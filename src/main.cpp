#include <Arduino.h>

#include "g_drive.h"
#include "draw_lcd.h"

#define DRAW_INTERVAL (10*60*1000) // 10分ごとに描画

bool init_all = false; // 初期化フラグ

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--------------------------------");

  if (!init_wifi())
  {
    return;
  }

  if (!init_gdrive())
  {
    return;
  }

  if (!init_lcd())
  {
    return;
  }
  
  uint8_t *bg_cal = NULL; size_t bg_cal_size = 0;
  uint8_t *bg_img = NULL; size_t bg_img_size = 0;
  if((bg_cal_size = get_pic(COLOR_BACKGROUND_CAL, bg_cal)) > 0 && 
      (bg_img_size = get_pic(COLOR_BACKGROUND_IMG, bg_img)) > 0)
  {
    set_background(bg_cal, bg_cal_size, bg_img, bg_img_size); // 背景画像を設定
  }

  init_all = true;
}

void loop()
{
  if (!init_all)
  {
    delay(1000);
    return;
  }

  int interval = 100;
  static int pre_current_month = 0;
  
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  int current_year = timeinfo.tm_year + 1900;
  int current_month = timeinfo.tm_mon + 1;

  // 月が変わったら祝日を再取得
  if (pre_current_month != current_month) {
    pre_current_month = current_month;
    fetch_holidays(current_year, pre_current_month);
  }

  uint8_t *pic = NULL;
  int size;
  String pic_id, pic_name;
  if (get_random_pic(pic_id, pic_name, size)) // ランダムな画像を取得
  {
    Serial.printf("draw: ID: %s, Name: %s, Size: %d\n", pic_id.c_str(), pic_name.c_str(), size);
    if (size <= 50 * 1024) // ファイルサイズが50KB以下ならば
    {
      int16_t w, h;
      size = get_pic(pic_id.c_str(), pic); // ランダムな画像を取得
      if (size > 0)
      {
        if (get_jpeg_width_height(pic, size, &w, &h)) // 画像の幅と高さを取得
        {
          bool vertical = w < h;

          draw_background(vertical); // 背景を描画

          draw_picture(pic, size, w, h); // 画像を描画
          
          // カレンダーの描画
          if (DRAW_CALENDAR)
          {
            draw_calendar(vertical); // カレンダーを描画
          }

          // 今日、曜日の描画
          draw_today(vertical, DRAW_CALENDAR);

          interval = DRAW_INTERVAL; // 正常に描画できたときの間隔
        }
      }
    }
  }
  if(pic != NULL)
    free(pic);
  delay(interval);
}

#ifndef DRAWLCD_H
#define DRAWLCD_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <HTTPClient.h>

#define ILI9488
//#define ST7789

#if defined(ARDUINO_XIAO_ESP32C6)
  #if defined(ILI9488)
  #include "myILI9488-ESP32C6.hpp"
  #elif defined(ST7789)
  #include "myST7789-ESP32C6.hpp"
  #endif

#elif defined(ARDUINO_XIAO_ESP32S3)
  #if defined(ILI9488)
  #include "myILI9488-ESP32S3.hpp"
  #elif defined(ST7789)
  #include "myST7789-ESP32S3.hpp"
  #endif

#elif defined(ESP32)
  #if defined(ILI9488)
  #include "myILI9488-ESP32.hpp"
  #elif defined(ST7789)
  #include "myST7789-ESP32.hpp"
  #endif

#endif

/*
display: 480x320 vertical
+-------------+----------------------------+---
|  MONTHYEAR  |                            |  MONTHYEAR_HEIGHT
|-------------|                            |
|  CALENDER   |                            | 1/2
|             |                            |
+-------------+           IMAGE            |---
|    WEEK     | 1/3                        |
|-------------|                            | 1/2
|    DAY      | 2/3                        |
|             |                            |
+-------------+----------------------------+---
|    1/3      |             2/3            |

display: 480x320 horizontal
+------------------------------------------+---
|                                          |
|                                          |
|                                          |
|                  IMAGE                   | 2/3
|                                          |
|                    |  1/3  |     2/3     |
+--------------------+---------------------+---
|    CALENDER        |      MONTHYEAR      |  MONTHYEAR_HEIGHT
|                    |WEEKDAY|    DAY      | 1/3
+--------------------+---------------------+---
|         1/2        |         1/2         |

display: 240x240 vertical
+--------+----------------+----
|MONTHYEAR|               |    MONTHYEAR_HEIGHT
|        |                | 1/2
|  WEEK  |                |    
+--------+      IMAGE     |----
|        |                |    
|  DAY   |                | 1/2
|        |                |    
+--------+----------------+----
|   1/3  |       2/3      |


display: 240x240 horizontal
+-------------------------+----
|                         |    
|         IMAGE           |    
|                         | 2/3
|                         |    
+------------+------------+----
| MONTHYEAR  |            |   MONTHYEAR_HEIGHT
|    WEEK    |    DAY     | 1/3
+------------+------------+----
|    1/2     |    1/2     |


*/

#if defined(ILI9488)
#define DRAW_CALENDAR true
#define LCD_ROTATION 1 // 0-3
#elif defined(ST7789)
#define DRAW_CALENDAR false // ST7789は小さいのでカレンダーを描画しない
#define LCD_ROTATION 2 // 0-3
#endif


#define FONT_MONTHYEAR &fonts::Orbitron_Light_24 // 月と年のフォント
#define FONT_MONTHYEAR_SIZE 0.7 // 月と年のフォントサイズ
#define FONT_CALENDAR &fonts::Font2 // カレンダーのフォント
#define FONT_CALENDAR_SIZE 1 // カレンダーのフォントサイズ
#define FONT_TODAY &fonts::Orbitron_Light_24 // 今日のフォント
#define FONT_TODAY_HEIGHT (24.0*1.54) // 今日のフォント高さ
#define FONT_WEEKDAY_SIZE 1 // 曜日のフォントサイズ
#define FONT_DAY_SIZE 2.2 // 日付のフォントサイズ

#define MONTHYEAR_HEIGHT 30 // 月と年の高さ

#define COLOR_DAY_OF_SUNDAY 0xFBA0
#define COLOR_DAY_OF_SATURDAY TFT_GREEN
#define COLOR_DAY_OF_WEEKDAY TFT_WHITE
#define COLOR_BACKGROUND_IMG "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy" // 背景画像のFILE_ID 480x320 img/bg1_480x320.jpg
#define COLOR_BACKGROUND_CAL "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" // 背景画像のFILE_ID 480x320 img/bg2_480x320.jpg

#define TYPE_CALENDAR 0 // カレンダー
#define TYPE_IMAGE 1 // イメージ

bool init_lcd();
bool get_jpeg_width_height(const uint8_t *data, size_t size, int16_t *width, int16_t *height);
void set_background(uint8_t *cal, size_t cal_size, uint8_t *img, size_t img_size);
void draw_background(bool vertical);
void draw_picture(const uint8_t *data, size_t size, int16_t width, int16_t height);
void draw_calendar(bool vertical);
void draw_today(bool vertical, bool calendar);
int get_day_color(int day_of_week, int day);
bool is_holiday(int day);
void fetch_holidays(int year, int month);
#endif
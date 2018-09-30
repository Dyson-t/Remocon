/*-------------------------------------------------------
 *  amazonで売ってるHiLetgo 128x64 LCD I2C版用
 *  ※Adafruit社の互換品なのでドライバがそのまま使える。
 *  スクロール式に文字表示する
 *  2018/8/26 外川
 -------------------------------------------------------*/
#include "LCD.h"

// コンパイル時にヘッダーファイルが適切に編集されていない場合に
// "Height incorrect, please fix Adafruit_SSD1306.h!"
// というエラーを表示するための記述
#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
// RSTピンがない互換品を使用するので-1を指定
Adafruit_SSD1306 display(-1);

// LCD表示領域
static String SSTR[] = {
  "                    "
  ,"                    "
  ,"                    "
  ,"                    "
  ,"                    "
  ,"                    "
  ,"                    "
};

void print_lcd(){
    display.clearDisplay();
    // テキストサイズを設定
    display.setTextSize(1);
    // テキスト色を設定
    display.setTextColor(WHITE);
    // テキストの開始位置を設定
    display.setCursor(0, 0);
    for( int i=0; i<7;i++){
      display.println(SSTR[i]);
    }
    // 描画バッファの内容を画面に表示
    display.display();
}

// 準備
void lcdstart(){
    // I2Cアドレスは使用するディスプレイに合わせて変更する
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.display();
}

// 一行表示する。7行までスクロール表示可能。
void lcdprintln( String str ){
  for( int i=0; i<6; i++ ){
    SSTR[i] = SSTR[i+1];
  }
  SSTR[6] = str;
  print_lcd();
}

// 一行表示する。上3行はスクロールなし。下4行はスクロールする。
void lcdprint_hnh( String str, unsigned int row ){
  if( row >= 3 ){
      for( int i=3; i<6; i++ ){
        SSTR[i] = SSTR[i+1];
      }
      SSTR[6] = str;
  }else{
      SSTR[row] = str;
  }
  print_lcd();
}


// 任意の箇所に文字列を表示する。（縦7 × 横20）
void lcdprint( String str, unsigned int row, unsigned int col){
// まだつくってない

}



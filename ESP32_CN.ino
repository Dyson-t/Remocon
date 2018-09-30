/*-------------------------------------------------------
 *  ESP32コントローラ コネクション版
 *  　■ラズパイからRSSI値をWRITEしてもらい、LCDに表示する
 *  　■コネクトが切れたらディープスリープしてブロードキャストし直す
 *    ■スティックの解像度を上げる。
 *      ■330Ω抵抗を2並列→3並列に変更し抵抗値を下げる。（ハード側）
 *      　これによりスティックの電圧値そのものを0x0～0xfffに解像度UP
 *      ■プログラム内で敢えて3bitシフトして解像度落としていたのをやめる。（ソフト側）
 *      　このため、ノーティファイデータが2倍のサイズとなる。
 *      ■ノーティファイデータが2倍のサイズとなるとキャラクタリスティックの最大サイズをオーバーするようなので、
 *        新たにUUIDを取得し、左右のジョイスティックを別々のキャラクタリスティックとした。
 *    □スキャンされるまでの速度向上（どうやって？）
 *    □コネクトされるまでの速度向上（どうやって？）
 *    □USBからの入力電圧によってスティックの中心電圧が異なる件への対策として半固定抵抗で中心電圧を変えられるように変更
 *  2018/9/16 外川
 -------------------------------------------------------*/
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "esp_sleep.h"
#include "LCD.h"
#include <BLE2902.h>

// ピン番号
#define P_STICK1_LR  39 // 右ジョイスティック
#define P_STICK1_UD  34
#define P_STICK2_LR  32  // 左ジョイスティック
#define P_STICK2_UD  35
#define P_LED  16
#define P_VOLUME  36
#define P_BUTTON1 17
#define P_BUTTON2 4

#define V_STICK_CENTER 0x700 //スティックのセンター値
#define GPIO_DEEP_SLEEP_DURATION     1  // sleep x seconds and then wake up

// デバイスの接続状態を管理
bool deviceConnected = false;
bool oldDeviceConnected = false;

BLEServer* pServer = NULL;
BLECharacteristic* pChrStickR = NULL;
BLECharacteristic* pChrStickL = NULL;
BLECharacteristic* pChrVolume = NULL;
BLECharacteristic* pChrButton1 = NULL;
BLECharacteristic* pChrButton2 = NULL;
BLECharacteristic* pChrRssi = NULL;

// UUID
// https://www.uuidgenerator.net/
#define UUID_S_BSTICK "415460d1-398f-4f35-b5fc-543ce300a0eb"  // コントローラ用サービス
#define UUID_C_STICK_R "dba42ed7-3435-42a6-b42d-488d2e71363d" // NOTIFY用キャラクタリスティック 右ジョイスティック
#define UUID_C_VOLUME "e2d5a4ff-5002-4b03-9a9a-d9d621a2674c"  // NOTIFY用キャラクタリスティック ボリュームつまみ
#define UUID_C_BUTTON1 "66182df1-0823-4d09-a15c-20ba6b3b0ccf" // NOTIFY用キャラクタリスティック ボタン1
#define UUID_C_BUTTON2 "32f79fb6-af3f-4fe9-9740-e3d223436467" // NOTIFY用キャラクタリスティック ボタン2
#define UUID_C_RSSI "714ba78f-eecb-4153-b19f-789c89ebf016"    // WRITE用 RSSI受け取り口
#define UUID_C_STICK_L "e30adb40-c199-41c9-9af5-671bfb310711" // NOTIFY用キャラクタリスティック 左ジョイスティック

void bdaDump(esp_bd_addr_t bd) {
    for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
        Serial.printf("%02x", bd[i]);
        if (i < ESP_BD_ADDR_LEN - 1) {
            Serial.print(":");
        }
    }
};

// 後述の手順②で登録。 接続/切断時のコールバック関数
class MyServerCallbacks: public BLEServerCallbacks {

    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
       deviceConnected = false;
    }
};

// セントラル側からREAD/WRITE要求があった場合のコールバック関数
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        // ここに書けば何かできる。
      }
    }
};

void setup() {
    Serial.begin(115200);
    // コネクションの手順1 BLEDeviceクラスからサーバーオブジェクトを生成する。※ここまではブロードキャストと一緒
    BLEDevice::init("ESP32_CN");
    pServer = BLEDevice::createServer();

    // コネクションの手順2 接続要求あり時または切断要求あり時のコールバック関数を設定しておく
    pServer->setCallbacks(new MyServerCallbacks());  

    // コネクションの手順3 サービスを生成する
    BLEService *pService = pServer->createService( UUID_S_BSTICK );  // サービスを生成

    // コネクションの手順4 キャラクタリスティックを生成する。Read/Writeを許す場合はここでコールバックを登録しておく。
    pChrStickR = pService->createCharacteristic(UUID_C_STICK_R , BLECharacteristic::PROPERTY_NOTIFY );
    pChrStickL = pService->createCharacteristic(UUID_C_STICK_L , BLECharacteristic::PROPERTY_NOTIFY );
    pChrVolume= pService->createCharacteristic(UUID_C_VOLUME,  BLECharacteristic::PROPERTY_NOTIFY );
    pChrButton1= pService->createCharacteristic(UUID_C_BUTTON1,  BLECharacteristic::PROPERTY_NOTIFY );
    pChrButton2= pService->createCharacteristic(UUID_C_BUTTON2,  BLECharacteristic::PROPERTY_NOTIFY );
    pChrRssi = pService->createCharacteristic(UUID_C_RSSI , BLECharacteristic::PROPERTY_WRITE );
    pChrRssi->setCallbacks(new MyCallbacks()); 
   
    pService->start(); 
    pServer->getAdvertising()->start();

    // BLE以外のハードウェアを初期化
    pinMode(P_BUTTON1, INPUT);
    pinMode(P_BUTTON2, INPUT);
    pinMode(P_LED, OUTPUT);
    lcdstart();  // LCD準備
    lcdprint_hnh( "Advertising", 0);
    lcdprint_hnh( "---------------------", 2);
}

void notifyValue( BLECharacteristic* p_stick, uint8_t* v_value ){
  p_stick->setValue(v_value, sizeof(v_value));
  p_stick->notify();
  delay(10);
}

// ジョイスティックに結構誤差があるのでニュートラル時に安定するように補正
// 現状だと各々このような値となる。
// MIN 右LR  右UD  左LR  左UD  | MAX 右LR  右UD  左LR  左UD
//     0x6b7 0x6b3 0x6c5 0x69c |     0x74a 0x737 0x753 0x711
// 中央0x700 0x6F5 0x70C 0x6D6 
//      1792  1781  1804  1750
int stick_hosei( int stick_value ){
    if( 0x69c <= stick_value && stick_value <= 0x753 ){
        return V_STICK_CENTER; 
    }else{
        return stick_value;
    }
}

uint16_t old_stick1_lr = 0;
uint16_t old_stick1_ud = 0;
uint16_t old_stick2_lr = 0;
uint16_t old_stick2_ud = 0;
uint8_t old_button1 = 0;
uint8_t old_button2 = 0;
uint8_t old_volume = 0;

int button_state = 1;

int a = 0;
int b = 0;
int c = 0;
int d = 0;
int amin= 0xfff;
int bmin = 0xfff;
int cmin = 0xfff;
int dmin = 0xfff;
int amax= 0;
int bmax = 0;
int cmax = 0;
int dmax = 0;
uint8_t counter_button2 = 0;

void loop() {
    // notify changed value
    if (deviceConnected) {
        lcdprint_hnh( "Connecting", 0);

        // ジョイスティックの傾きを取得 ※各2Byteで送る。中心に少々遊びを設ける。
        uint16_t v_stick1_lr = stick_hosei( analogRead( P_STICK1_LR )); 
        uint16_t v_stick1_ud =stick_hosei( analogRead( P_STICK1_UD ));
        uint16_t v_stick2_lr = stick_hosei( analogRead( P_STICK2_LR ));
        uint16_t v_stick2_ud =stick_hosei( analogRead( P_STICK2_UD ));

        // ジョイスティックの傾きを纏める
        uint8_t v_sticks_R[8] = {(v_stick1_lr & 0xff00) >> 8, (v_stick1_lr & 0x00ff) 
                               , (v_stick1_ud & 0xff00) >> 8, (v_stick1_ud & 0x00ff)};
        uint8_t v_sticks_L[8] = {(v_stick2_lr & 0xff00) >> 8, (v_stick2_lr & 0x00ff) 
                                ,(v_stick2_ud & 0xff00) >> 8, (v_stick2_ud & 0x00ff)};
                                 
        // ボタンの値
        uint8_t v_button1 = digitalRead( P_BUTTON1 );
        uint8_t v_button2 = digitalRead( P_BUTTON2 );

        // ボリュームの値
        //uint16_t v_volume = analogRead( P_VOLUME );
        uint8_t v_volume = analogRead( P_VOLUME ) >> 4 ;
        //uint8_t v_vol8 = { v_volume & 0x00ff  };
        //Serial.printf("v_volume = %x\n", v_volume);
        
        // 前回の値から変化したらNotify
        if((old_stick1_lr != v_stick1_lr) || (old_stick1_ud != v_stick1_ud)){notifyValue( pChrStickR, v_sticks_R );}
        if((old_stick2_lr != v_stick2_lr) || (old_stick2_ud != v_stick2_ud)){notifyValue( pChrStickL, v_sticks_L );}
        if( old_button1 != v_button1 ){ notifyValue( pChrButton1, &v_button1 ); }
        //if( old_button2 != v_button2 ){ notifyValue( pChrButton2, &v_button2 ); }
        if( old_volume != v_volume ){ notifyValue( pChrVolume, &v_volume ); }

        // ボタン2は押した時間をNotify
        if(v_button2){
          counter_button2++;
          char msg[20];
          sprintf( msg, "BTN2=%d", counter_button2 );
          lcdprint_hnh( msg, 1);
        }else{
          notifyValue( pChrButton2, &counter_button2 );
          counter_button2 = 0;
        }

        if( ( old_stick1_lr != v_stick1_lr )
        || ( old_stick1_ud != v_stick1_ud )
        || ( old_stick2_lr != v_stick2_lr )
        || ( old_stick2_ud != v_stick2_ud ) 
        || ( old_button1 != v_button1 ) 
        //|| ( old_button2 != v_button2 ) 
        || ( old_volume != v_volume ) 
        ){
            char msg[20];
            sprintf( msg, "%03x%03x%03x%03x %x %x %x",v_stick1_lr, v_stick1_ud, v_stick2_lr, v_stick2_ud,  v_button1, v_button2, v_volume);
            lcdprint_hnh( msg, 3);
            old_stick1_lr = v_stick1_lr;
            old_stick1_ud = v_stick1_ud;
            old_stick2_lr = v_stick2_lr;
            old_stick2_ud = v_stick2_ud;
            old_button1 = v_button1;
            //old_button2 = v_button2;
            old_volume = v_volume;
        }
        // RSSI値キャラクタリスティックの値をLCDに表示する。
        std::string rssi = pChrRssi->getValue();
        char msg[20] = "RSSI:";
        for (int i = 0; i < rssi.length(); i++)
          msg[i+5] = rssi[i];
        lcdprint_hnh( msg, 1); 
    }

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        lcdprint_hnh( "Disconnect", 0);
        digitalWrite( P_LED, LOW );
        esp_deep_sleep(1000000LL * GPIO_DEEP_SLEEP_DURATION);
    }

    // connecting  ※初回接続時の処理をここに書ける。
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
        digitalWrite( P_LED, HIGH );
    }
}


#define M5CORE2
//#define M5STICKC

#ifdef M5CORE2
#include <M5Core2.h>
#include <Fonts/EVA_20px.h>
#endif
#ifdef M5STICKC
#include <M5StickC.h>
#endif

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <misakiUTF16.h>

const char* wifi_ssid = "【WiFiアクセスポイントのSID】";
const char* wifi_password = "【WiFiアクセスポイントのパスワード】";
const char* mqtt_server = "【MQTTブローカのホスト名】";
const uint16_t mqtt_port = 【MQTTブローカのポート番号】; // MQTTサーバのポート番号(TCP接続)
const char* topic = "android/notify"; // Notify受信用トピック名
#ifdef M5CORE2
#define MQTT_CLIENT_NAME  "M5Core2" // MQTTサーバ接続時のクライアント名
#endif
#ifdef M5STICKC
#define MQTT_CLIENT_NAME  "M5StickC" // MQTTサーバ接続時のクライアント名
#endif

#define DISP_FORE_COLOR   WHITE
#define DISP_BACK_COLOR   BLACK
#define MQTT_BUFFER_SIZE  1024 // MQTT送受信のバッファサイズ
#define NUM_OF_NOTIFY 10 // Notifyの最大保持数

#ifdef M5CORE2
#define DISP_WIDHT    320 // 画面の幅
#define DISP_HEIGHT   240 // 画面の高さ
#endif
#ifdef M5STICKC
#define DISP_WIDHT    160 // 画面の幅
#define DISP_HEIGHT   80 // 画面の高さ
#endif
#define MAX_TEXT_LEN  64 // 保持可能な最大文字数
#define ICON_WIDTH    32 //アイコンデータの幅

typedef struct{
  char title[MAX_TEXT_LEN];
  char label[MAX_TEXT_LEN];
  char name[MAX_TEXT_LEN];
  unsigned char icon[ICON_WIDTH * (ICON_WIDTH / 8)];
} NOTIFY_MESSAGE;

typedef struct{
  int id;
  int index;
} NOTIFY_INDEX;

NOTIFY_MESSAGE notify_message[NUM_OF_NOTIFY];
NOTIFY_INDEX notify_list[NUM_OF_NOTIFY];
int notify_index = -1; // 現在表示位置

const int capacity = JSON_OBJECT_SIZE(8);
StaticJsonDocument<capacity> json_notify;

WiFiClient espClient;
PubSubClient client(espClient);

void bitdisp(byte x, byte y, uint8_t d ) {
  for (byte i=0; i<8;i++) {
    if (d & (0x80 >> i)) {
      if(x + i < DISP_WIDHT && y < DISP_HEIGHT)
        M5.Lcd.drawPixel(x + i ,y , DISP_FORE_COLOR);
    }      
  }
}

void drawJPChar(byte x, byte y, const char * pUTF8) {
  uint16_t pUTF16[MAX_TEXT_LEN];
  int len = Utf8ToUtf16(pUTF16, (char*)pUTF8);  // UTF8からUTF16に変換する

  // バナー用パターン作成
  byte buf[MAX_TEXT_LEN][8];  //160x8ドットのバナー表示パターン
  for (int i = 0; i < len; i++) {
    getFontDataByUTF16(&buf[i][0], utf16_HantoZen(pUTF16[i]));  // フォントデータの取得    
  }
  
  // ドット表示
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < len; j++){
        bitdisp(x + (j * 8) ,y + i , buf[j][i]);
    }
  }
}

void drawMono(int offset_x, int offset_y, int width, int height, const uint8_t* p_bmp){
  for( int y = 0 ; y < height ; y++ ){
    for( int x = 0 ; x < width ; x++ ){
      if( ( p_bmp[ y * ((width + 7) / 8) + x / 8] & ( 0x0001 << ( 8 - (x % 8) - 1 )) ) != 0x00 )
        M5.Lcd.drawPixel(offset_x + x , offset_x + y , DISP_FORE_COLOR);
    }
  }
}

unsigned char tohex(char c){
  if( c >= '0' && c <= '9')
    return c - '0';
  if( c >= 'a' && c <= 'f' )
    return c - 'a' + 10;
  if( c >= 'F' && c <= 'F' )
    return c - 'A' + 10;

  return 0;
}

long parse_hex(const char* p_hex, unsigned char *p_bin){
  int index = 0;
  while( p_hex[index * 2] != '\0'){
    p_bin[index] = tohex(p_hex[index * 2]) << 4;
    p_bin[index] |= tohex(p_hex[index * 2 + 1]);
    index++;
  }

  return index;
}

void updateNotify(void){
  M5.Lcd.fillScreen(DISP_BACK_COLOR);
  M5.Lcd.setCursor(0, 32);
  if(notify_index < 0){
    M5.Lcd.println("No Notification");
    return;
  }

  int index = notify_list[notify_index].index;
  Serial.println(notify_message[index].name);
  Serial.println(notify_message[index].title);
  Serial.println(notify_message[index].label);

  // アイコンを表示
  drawMono(0, 0, ICON_WIDTH, ICON_WIDTH, notify_message[index].icon);

  // 各種表示
  M5.Lcd.setCursor(40, 0);
  M5.Lcd.print(notify_index);
  M5.Lcd.print(" ");
  M5.Lcd.print(notify_list[notify_index].id);
  drawJPChar(0, 32 + 0, notify_message[index].label);
  drawJPChar(0, 32 + 8, notify_message[index].title);
  drawJPChar(0, 32 + 16, notify_message[index].name);
}

// MQTT Subscribeで受信した際のコールバック関数
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("received");

  // JSONをパース
  DeserializationError err = deserializeJson(json_notify, payload, length);
  if( err ){
    Serial.println("Deserialize error");
    Serial.println(err.c_str());
    return;
  }

  int id = json_notify["id"];
  if( id < 0 ){
    // 再接続発生時には、過去の通知をすべてリセット
    for( int i = 0 ; i < NUM_OF_NOTIFY ; i++ ){
      notify_list[i].index = -1;
    }
    notify_index = -1;
  }else{
    // すでに同じIDのNotifyがあれば一旦削除
    for( int i = 0 ; i < NUM_OF_NOTIFY ; i++ ){
      if( notify_list[i].index < 0 )
        break;
      if( notify_list[i].id == id ){
        for( int j = i ; j < NUM_OF_NOTIFY - 1 ; j++ )
          notify_list[j] = notify_list[j + 1];
        notify_list[NUM_OF_NOTIFY - 1].index = -1;
  
        // 現在表示位置も修正
        if( i == notify_index )
          notify_index = -1;
        else if( notify_index > i )
          notify_index--;
        break;
      }
    }
  
    // posted=trueの場合、リストの先頭に追加
    bool posted = json_notify["posted"];
    if( posted ){
      Serial.println("posted=true");
      // 末尾を削除して、先頭から1つずつ後ろにシフト
      for( int i = NUM_OF_NOTIFY - 1 ; i > 0 ; i-- ){
        notify_list[i] = notify_list[i - 1];
      }
      notify_list[0].index = -1;
  
      // notfy_messageリストから空きを検索
      for( int index = 0 ; index < NUM_OF_NOTIFY ; index++ ){
        int j;
        for( j = 1 ; j < NUM_OF_NOTIFY ; j++ ){
          if( notify_list[j].index == index )
            break;
        }
        if( j >= NUM_OF_NOTIFY ){
          // notify_messageに受信データを格納
          const char *title = json_notify["title"];
          const char *name = json_notify["name"];
          const char *label = json_notify["label"];
          notify_message[index].name[sizeof(notify_message[index].name) - 1] = '\0';
          notify_message[index].title[sizeof(notify_message[index].title) - 1] = '\0';
          notify_message[index].label[sizeof(notify_message[index].label) - 1] = '\0';
          strncpy( notify_message[index].name, name, sizeof(notify_message[index].name) - 1 );
          strncpy( notify_message[index].title, title, sizeof(notify_message[index].title) - 1 );
          strncpy( notify_message[index].label, label, sizeof(notify_message[index].label) - 1 );
  
          // iconがある場合は、バイト配列に変換して格納
          const char *icon = json_notify["icon"];
          if( icon != NULL && strlen(icon) == ICON_WIDTH * ((ICON_WIDTH + 7) / 8) * 2 )
            parse_hex(icon, notify_message[index].icon);
          else
            memset(notify_message[index].icon, 0x00, sizeof(notify_message[index].icon));
  
          // リストの先頭に確定
          notify_list[0].index = index;
          notify_list[0].id = id;
          // 現在表示中も先頭に変更
          notify_index = 0;
          break;
        }
      }
    }
  }

  // 表示を更新
  updateNotify();
}

// WiFiアクセスポイントに接続
void wifi_connect(void){
  Serial.println("");
  Serial.print("WiFi Connenting");

  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  Serial.print("Connected : ");
  Serial.println(WiFi.localIP());
  M5.Lcd.println(WiFi.localIP());
}

void setup() {
#ifdef M5CORE2
  M5.begin(true, false, true, true);
  M5.Lcd.setTextSize(2);
#endif
#ifdef M5STICKC
  M5.begin(true, true, true);
  M5.Lcd.setRotation(3);
  M5.Lcd.setTextSize(1);
#endif
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Now Initializing");

  M5.Lcd.fillScreen(DISP_BACK_COLOR);
  M5.Lcd.setTextColor(DISP_FORE_COLOR);
  
  // リストを初期化
  for( int i = 0 ; i < NUM_OF_NOTIFY ; i++ )
    notify_list[i].index = -1;

  // WiFiアクセスポイントに接続
  M5.Lcd.println("WiFi Connecting");
  wifi_connect();
  M5.Lcd.println("WiFi Connected");

  // バッファサイズの変更
  client.setBufferSize(MQTT_BUFFER_SIZE);
  // MQTTコールバック関数の設定
  client.setCallback(mqtt_callback);
  // MQTTブローカに接続
  client.setServer(mqtt_server, mqtt_port);

  M5.Lcd.println("MQTT Connect");
}

bool isTouched = false;

void loop() {
#ifdef M5STICKC
  M5.update();
#endif
  client.loop();

  // MQTT未接続の場合、再接続
  while(!client.connected() ){
    Serial.println("Mqtt Reconnecting");
    if( client.connect(MQTT_CLIENT_NAME) ){
      // MQTT Subscribe
      client.subscribe(topic);
      Serial.println("Mqtt Connected and Subscribing");
      break;
    }
    delay(1000);
  }

#ifdef M5CORE2
  // タッチの検出
  TouchPoint_t pos = M5.Touch.getPressPoint();
  if( isTouched ){
    if( pos.x < 0 || pos.y < 0 )
      isTouched = false;
  }else{
    if(pos.y > 240){
      if(pos.x < 109){
        // 左ボタン押下
        if( notify_index > 0 )
          notify_index--;
      }else if( pos.x > 218 ){
        // 右ボタン押下
        if( notify_index >= 0 ){
          if( notify_index < (NUM_OF_NOTIFY - 1) && notify_list[notify_index + 1].index >= 0 )
            notify_index++;
        }
      }else{
        // do nothing
      }

      // 表示の更新
      updateNotify();
      isTouched = true;
    }
  }
#endif
#ifdef M5STICKC
  if( M5.BtnA.isPressed()){
    // ボタン押下
    if( !isTouched ){
      if( notify_index >= 0 ){
        if( notify_index < (NUM_OF_NOTIFY - 1) && notify_list[notify_index + 1].index >= 0 )
          notify_index++;
        else
          notify_index = 0;
      }

      // 表示の更新
      updateNotify();
      isTouched = true;
    }
  }
  if( M5.BtnA.isReleased()){
    isTouched = false;
  }
#endif
  
  delay(10);
}

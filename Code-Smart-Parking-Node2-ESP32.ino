#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ================= CẤU HÌNH WIFI & THINGSBOARD =================
const char* ssid = "Xiaomi 15T";       
const char* password = "abcd1234";

const char* mqtt_server = "thingsboard.cloud";
const char* token = "JlrSohUjoJVlygCqp6jv";

WiFiClient espClient;
PubSubClient client(espClient);

static uint8_t peerAddr[] = {0xB0,0xCB,0xD8,0xC9,0x19,0x98};

typedef struct {
  char nodeMac[18];
  bool isEmptyCell_N2;
  bool tempAlarm_N2;
  bool islightCell_N2;
} esp_packet_t;

esp_packet_t espData = {};

bool lastIsEmptyCell = true;
bool lastTempAlarm = false;
bool lastIsLightCell = false;

esp_now_peer_info_t peerInfo = {};

bool espNowReady = false;
uint8_t espNowChannel = 0;

unsigned long lastReconnectAttempt = 0;
unsigned long lastSendTime = 0;
// ===============================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define TRIG_PIN     5
#define ECHO_PIN     18
#define LDR_PIN      32
#define BUTTON_PIN   14  
#define BUZZER_PIN   25  
#define LED_GREEN    26  
#define LED_RED      27  
#define RELAY_PIN    19  
#define FLAME_PIN    33  

#define PIR_PIN      13  
#define LED_VANG_PIN 23  

const int NGUONG_BAT_DEN = 25; 
const int NGUONG_TAT_DEN = 35; 
const int MUC_PHAT_HIEN_LUA = HIGH; 
const float TEMP_ALARM_THRESHOLD = 45.0;

bool isManualEmergency = false; 
bool isFireEmergency = false;   
bool isFireCause = false;
bool wasEmergency = false;      

int lastButtonState = HIGH; 
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; 

bool hasParkedBeeped = false;
bool isSlotFull = false;      
int fillCounter = 0;          
int emptyCounter = 0;         
const int DEBOUNCE_TARGET = 15; 
bool isLightOn = false; 

// [MỚI] Biến theo dõi sự kiện giao mùa (Sáng - Tối) của môi trường
bool isEnvironmentDark = false; 

// Callback ESP-NOW
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  (void)info;
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "ESP-NOW Node 2: Master da nhan goi" : "ESP-NOW Node 2: gui that bai");
}

bool initEspNow() {
  if (WiFi.status() != WL_CONNECTED) return false;
  uint8_t currentChannel = 0; wifi_second_chan_t secondChannel;
  if (esp_wifi_get_channel(&currentChannel, &secondChannel) != ESP_OK) return false;

  if (!espNowReady) {
    if (esp_now_init() != ESP_OK) return false;
    if (esp_now_register_send_cb(OnDataSent) != ESP_OK) return false;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, peerAddr, 6);
    peerInfo.channel = 0;
    peerInfo.ifidx = WIFI_IF_STA;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) return false;
    espNowReady = true;
  }
  if (espNowChannel != currentChannel) espNowChannel = currentChannel;
  return true;
}

void sendNode2ToMaster(float temperatureC) {
  if (!initEspNow()) return;
  snprintf(espData.nodeMac, sizeof(espData.nodeMac), "%s", WiFi.macAddress().c_str());
  espData.isEmptyCell_N2 = isSlotFull;
  espData.tempAlarm_N2 = isFireEmergency || isFireCause || temperatureC >= TEMP_ALARM_THRESHOLD;
  espData.islightCell_N2 = isLightOn;

  esp_now_send(peerAddr, reinterpret_cast<uint8_t *>(&espData), sizeof(espData));
}

void updateNode2State(float temperatureC) {
  bool currentIsEmpty = isSlotFull;
  bool currentTempAlarm = isFireEmergency || isFireCause || (temperatureC >= TEMP_ALARM_THRESHOLD);
  bool currentLight = isLightOn;

  if (currentIsEmpty != lastIsEmptyCell || currentTempAlarm != lastTempAlarm || currentLight != lastIsLightCell) {
    lastIsEmptyCell = currentIsEmpty;
    lastTempAlarm = currentTempAlarm;
    lastIsLightCell = currentLight;
    sendNode2ToMaster(temperatureC);
  }
}

void setBuzzer(bool turnOn) {
  if (turnOn) digitalWrite(BUZZER_PIN, LOW);  
  else        digitalWrite(BUZZER_PIN, HIGH);   
}

void setParkingLight(bool turnOn) {
  if (turnOn) {
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);   
  } else {
    pinMode(RELAY_PIN, INPUT);      
  }
}

// ================= [MỚI] NHẬN LỆNH CÔNG TẮC TỪ THINGSBOARD =================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) { message += (char)payload[i]; }

  if (String(topic).indexOf("v1/devices/me/rpc/request/") != -1) {
    String topicStr = String(topic);
    String requestId = topicStr.substring(topicStr.lastIndexOf("/") + 1);

    // Lắng nghe công tắc bật/tắt đèn có method là "setLight"
    if (message.indexOf("\"method\":\"setLight\"") != -1) {
      if (message.indexOf("\"params\":true") != -1) {
         setParkingLight(true); isLightOn = true;
      } else {
         setParkingLight(false); isLightOn = false;
      }
      client.publish(("v1/devices/me/rpc/response/" + requestId).c_str(), "{\"success\":true}");
    }
  }
}
// ===========================================================================

void setup() {
  Serial.begin(115200);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); 
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LOW);
  pinMode(RELAY_PIN, INPUT);      
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FLAME_PIN, INPUT); 

  pinMode(PIR_PIN, INPUT);
  pinMode(LED_VANG_PIN, OUTPUT);
  digitalWrite(LED_VANG_PIN, LOW);

  dht.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(" CONNECTING WIFI ");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) { delay(500); }

  if (WiFi.status() == WL_CONNECTED) initEspNow();

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback); // Đăng ký hàm nhận lệnh RPC
  client.setBufferSize(512); 
  delay(1500);
  lcd.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // ================= DUY TRÌ KẾT NỐI THINGSBOARD =================
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (currentMillis - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = currentMillis;
        if (client.connect("ESP32_Node2", token, NULL)) {
          lastReconnectAttempt = 0;
          client.subscribe("v1/devices/me/rpc/request/+"); // Đăng ký nghe lệnh RPC
        }
      }
    } else {
      client.loop(); 
    }
  }

  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) lastDebounceTime = currentMillis;
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    static bool buttonPressed = false;
    if (reading == LOW && !buttonPressed) {
      buttonPressed = true;
      if (isManualEmergency || isFireEmergency) {
        isManualEmergency = false; isFireEmergency = false; isFireCause = false;
      } else {
        isManualEmergency = true; isFireCause = false; 
      }
    } else if (reading == HIGH) {
      buttonPressed = false; 
    }
  }
  lastButtonState = reading;

  if (digitalRead(FLAME_PIN) == MUC_PHAT_HIEN_LUA) {
    if (!isFireEmergency && !isManualEmergency) {
      isFireEmergency = true;  isFireCause = true;
    }
  }

  bool isEmergency = isManualEmergency || isFireEmergency;
  bool motionDetected = digitalRead(PIR_PIN);
  static bool lastMotionDetected = false; 

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t)) t = 0.0; 
  if (isnan(h)) h = 0.0;

  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  float distance = (duration * 0.0343) / 2;
  
  int lightVal = map(analogRead(LDR_PIN), 0, 4095, 100, 0);

  if (currentMillis - lastSendTime >= 2000) {
    lastSendTime = currentMillis;
    int dist_int = (distance > 0) ? (int)distance : 0;

    if (client.connected()) {
      String payload = "{";
      payload += "\"is_full\":" + String(isSlotFull ? 1 : 0) + ",";
      payload += "\"emergency\":" + String(isEmergency ? 1 : 0) + ",";
      payload += "\"fire_alert\":" + String(isFireCause ? 1 : 0) + ",";
      payload += "\"distance\":" + String(dist_int) + ",";
      payload += "\"temp\":" + String((int)t) + ",";
      payload += "\"humid\":" + String((int)h) + ",";
      payload += "\"light\":" + String(lightVal) + ",";
      payload += "\"light_status\":" + String(isLightOn ? 1 : 0);
      payload += "}";

      client.publish("v1/devices/me/telemetry", payload.c_str());
    }
  }

  if (isEmergency) {
    if (!wasEmergency) {
      lcd.clear();
      setParkingLight(false);
      isLightOn = false;
      digitalWrite(LED_VANG_PIN, LOW); 
      wasEmergency = true;
    }

    static unsigned long lastBlinkTime = 0;
    static bool blinkState = false;
    if (currentMillis - lastBlinkTime >= 150) { 
      lastBlinkTime = currentMillis;
      blinkState = !blinkState;
      digitalWrite(LED_RED, blinkState ? HIGH : LOW);
      digitalWrite(LED_GREEN, LOW); 
      setBuzzer(blinkState);
    }
    
    lcd.setCursor(0, 0); lcd.print("!!! DANGER !!!  ");
    lcd.setCursor(0, 1); 
    if (isFireCause) lcd.print("TT: PHAT HIEN LUA!"); 
    else             lcd.print("TT: KHAN CAP!     "); 
  } 
  else {
    if (wasEmergency) {
      lcd.clear();
      setBuzzer(false);
      wasEmergency = false;
    }

    // ================= [MỚI] THUẬT TOÁN ĐIỀU KHIỂN ÁNH SÁNG 2 CHIỀU =================
    if (lightVal < NGUONG_BAT_DEN) {
      // Nếu trời tối, và trạng thái trước đó là đang Sáng -> Vừa giao mùa
      if (!isEnvironmentDark) { 
        isEnvironmentDark = true;
        setParkingLight(true); isLightOn = true; // LDR tự bật
      }
    } else if (lightVal > NGUONG_TAT_DEN) {
      // Nếu trời sáng, và trạng thái trước đó là đang Tối -> Vừa giao mùa
      if (isEnvironmentDark) {
        isEnvironmentDark = false;
        setParkingLight(false); isLightOn = false; // LDR tự tắt
      }
    }
    // LƯU Ý: Giữa các lần giao mùa, LDR sẽ im lặng. Do đó có thể bấm ThingsBoard thoải mái!
    // ==============================================================================

    if (lastMotionDetected == true && motionDetected == false) {
      if (distance > 0 && distance <= 10) {
        if (!isSlotFull) {
          isSlotFull = true;
          setBuzzer(true); delay(400); setBuzzer(false); hasParkedBeeped = true;
        }
      } else {
        if (isSlotFull) {
          isSlotFull = false;
          hasParkedBeeped = false; setBuzzer(false);
        }
      }
      fillCounter = 0;
      emptyCounter = 0;
    }
    lastMotionDetected = motionDetected; 

    if (!motionDetected) {
      if (distance > 0 && distance <= 10) {
        fillCounter++; emptyCounter = 0;
        if (fillCounter >= DEBOUNCE_TARGET && !isSlotFull) {
          isSlotFull = true;
          if (!hasParkedBeeped) { setBuzzer(true); delay(400); setBuzzer(false); hasParkedBeeped = true; }
        }
      } else {
        emptyCounter++; fillCounter = 0;
        if (emptyCounter >= DEBOUNCE_TARGET && isSlotFull) {
          isSlotFull = false;
          hasParkedBeeped = false; setBuzzer(false); 
        }
      }
    } else {
      fillCounter = 0; 
      emptyCounter = 0;
    }

    // Ưu tiên: Đỏ (có xe) > Vàng (PIR) > Xanh (trống)
    if (isSlotFull) {
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_VANG_PIN, LOW);
    } else if (motionDetected) {
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_VANG_PIN, HIGH);
    } else {
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_VANG_PIN, LOW);
    }

    lcd.setCursor(0, 0);
    lcd.print("D:");
    if (distance > 400 || distance <= 0) lcd.print("---");
    else lcd.print((int)distance);
    lcd.print("cm S:"); lcd.print(lightVal); lcd.print("%   ");

    lcd.setCursor(0, 1);
    lcd.print("T:"); lcd.print((int)t); lcd.print("C H:");
    lcd.print((int)h); lcd.print("%      ");
  }
  
  updateNode2State(t);
  delay(50); 
}
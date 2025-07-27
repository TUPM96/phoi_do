#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>
#include <DHT.h>
#include <SoftwareSerial.h>

// HC-06: RX=2, TX=3
SoftwareSerial btSerial(2, 3); // RX, TX

#define DHTPIN 5
#define DHTTYPE DHT11
const int stepsPerRevolution = 2048;
const int IN1 = 8;
const int IN2 = 9;
const int IN3 = 10;
const int IN4 = 7;

Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

const int buttonMode = 11;
const int buttonAction = 12;
const int rainSensorPin = 6;
const int lightSensorPin = 4;

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);

int lastButtonStateMode = HIGH;
int lastButtonStateAction = HIGH;
unsigned long lastDebounceTimeMode = 0;
unsigned long lastDebounceTimeAction = 0;
const unsigned long debounceDelay = 120;

bool isPhoi = true;
bool lastRainState = HIGH;
bool autoMode = true;

unsigned long lastLCDUpdate = 0;
const unsigned long lcdUpdateInterval = 500; // ms

void parseBTJson(String json) {
  json.trim();
  Serial.print("Nhan JSON: "); Serial.println(json);

  if (json.startsWith("{") && json.endsWith("}")) {
    // Mode command
    int modeIdx = json.indexOf("\"mode\"");
    if (modeIdx >= 0) {
      int valIdx = json.indexOf(":", modeIdx);
      if (valIdx >= 0) {
        String modeVal = json.substring(valIdx + 1);
        modeVal.trim();
        modeVal.replace("\"", ""); // remove quotes
        modeVal.replace("}", "");
        modeVal.trim();
        if (modeVal.equalsIgnoreCase("auto")) {
          autoMode = true;
          Serial.println("Bluetooth: Chuyen sang AUTO");
        } else if (modeVal.equalsIgnoreCase("manual")) {
          autoMode = false;
          Serial.println("Bluetooth: Chuyen sang MANUAL");
        }
      }
    }
    // Action command (manual only)
    int actIdx = json.indexOf("\"action\"");
    if (actIdx >= 0 && !autoMode) {
      int valIdx = json.indexOf(":", actIdx);
      if (valIdx >= 0) {
        String actVal = json.substring(valIdx + 1);
        actVal.trim();
        actVal.replace("\"", "");
        actVal.replace("}", "");
        actVal.trim();
        if (actVal.equalsIgnoreCase("phoi")) {
          if (!isPhoi) {
            isPhoi = true;
            Serial.println("Bluetooth: MANUAL PHOI");
            myStepper.step(stepsPerRevolution);
            Serial.println("Stepper: Quay PHOI");
          } else {
            Serial.println("Dang o trang thai PHOI!");
          }
        } else if (actVal.equalsIgnoreCase("thu")) {
          if (isPhoi) {
            isPhoi = false;
            Serial.println("Bluetooth: MANUAL THU");
            myStepper.step(-stepsPerRevolution);
            Serial.println("Stepper: Quay THU");
          } else {
            Serial.println("Dang o trang thai THU!");
          }
        }
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("===== KHOI DONG HE THONG =====");
  btSerial.begin(9600); // Khởi động Bluetooth
  Serial.println("Bluetooth (HC-06) khoi dong 9600 baud");
  pinMode(buttonMode, INPUT_PULLUP);
  pinMode(buttonAction, INPUT_PULLUP);
  pinMode(rainSensorPin, INPUT);
  pinMode(lightSensorPin, INPUT);
  myStepper.setSpeed(10);
  Serial.println("Stepper set speed 10rpm");

  dht.begin();
  Serial.println("DHT11 khoi dong");
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Khoi dong ...");
  delay(800);
  lcd.clear();
  Serial.println("LCD khoi dong OK");
  Serial.println("===== San sang hoat dong =====");
}

void loop() {
  unsigned long now = millis();

  // Đọc cảm biến DHT11
  static float t = 0, h = 0;
  static unsigned long lastDHTRead = 0;
  if (now - lastDHTRead > 2000) {
    t = dht.readTemperature();
    h = dht.readHumidity();
    lastDHTRead = now;
    if (isnan(t) || isnan(h)) {
      Serial.println("Loi: DHT11 khong doc duoc gia tri!");
    } else {
      Serial.print("Nhiet do: "); Serial.print(t);
      Serial.print("C | Do am: "); Serial.print(h); Serial.println("%");
    }
  }

  int lightValue = digitalRead(lightSensorPin);
  String lightStatus = (lightValue == HIGH) ? "SANG" : "TOI ";

  int rainValue = digitalRead(rainSensorPin);
  bool isRaining = (rainValue == LOW);

  int readButtonMode = digitalRead(buttonMode);
  if (readButtonMode != lastButtonStateMode) {
    lastDebounceTimeMode = now;
    lastButtonStateMode = readButtonMode;
  }
  static bool lastStableMode = HIGH;
  if ((now - lastDebounceTimeMode) > debounceDelay) {
    if (lastStableMode == HIGH && readButtonMode == LOW) {
      autoMode = !autoMode;
      Serial.print("Nhan nut che do: Chuyen sang ");
      Serial.println(autoMode ? "AUTO" : "MANUAL");
    }
    lastStableMode = readButtonMode;
  }

  int readButtonAction = digitalRead(buttonAction);
  if (readButtonAction != lastButtonStateAction) {
    lastDebounceTimeAction = now;
    lastButtonStateAction = readButtonAction;
  }
  static bool lastStableAction = HIGH;
  if ((now - lastDebounceTimeAction) > debounceDelay) {
    if (!autoMode && lastStableAction == HIGH && readButtonAction == LOW) {
      isPhoi = !isPhoi;
      if (isPhoi) {
        Serial.println("Nhan nut: MANUAL PHOI");
        myStepper.step(stepsPerRevolution);
        Serial.println("Stepper: Quay PHOI");
      } else {
        Serial.println("Nhan nut: MANUAL THU");
        myStepper.step(-stepsPerRevolution);
        Serial.println("Stepper: Quay THU");
      }
    }
    lastStableAction = readButtonAction;
  }

  static unsigned long lastAutoAction = 0;
  if (autoMode && (now - lastAutoAction > 400)) {
    // Điều kiện thu/ phơi thông minh
    bool doAmCao = (!isnan(h) && h > 80);        // Độ ẩm cao, ví dụ >80%
    bool doAmThap = (!isnan(h) && h < 70);       // Độ ẩm thấp, ví dụ <70%
    bool troiSang = (lightValue == HIGH);
    bool troiToi = (lightValue == LOW);

    // Điều kiện thu vào: trời tối, hoặc mưa, hoặc độ ẩm cao
    if (isPhoi && (isRaining || doAmCao)) {
        isPhoi = false;
        Serial.println("AUTO: Dieu kien xau (mua/toi/am cao), tu dong THU!");
        myStepper.step(-stepsPerRevolution);
        Serial.println("Stepper: Quay THU");
        lastAutoAction = now;
    }

    // Điều kiện phơi ra: trời sáng, không mưa, độ ẩm thấp
    if (!isPhoi && (!isRaining)  && doAmThap) {
        isPhoi = true;
        Serial.println("AUTO: Dieu kien tot (sang/ko mua/am thap), tu dong PHOI!");
        myStepper.step(stepsPerRevolution);
        Serial.println("Stepper: Quay PHOI");
        lastAutoAction = now;
    }
  }
  lastRainState = isRaining ? LOW : HIGH;

  // -- Hiển thị LCD và gửi Bluetooth định kỳ
  if (now - lastLCDUpdate > lcdUpdateInterval) {
    lastLCDUpdate = now;
    lcd.setCursor(0, 0);
    if (isnan(t) || isnan(h)) {
      lcd.print("DHT loi!        ");
    } else {
      lcd.print("ND:");
      if (t < 10) lcd.print("0");
      lcd.print((int)t);
      lcd.print(" DA:");
      if (h < 10) lcd.print("0");
      lcd.print((int)h);
      lcd.print(" ");
      lcd.print(lightStatus);
    }
    lcd.setCursor(0, 1);
    lcd.print("Mua:");
    lcd.print(isRaining ? "CO " : "KO ");
    lcd.print(isPhoi ? "PHOI" : "THU ");
    lcd.setCursor(15, 1);
    lcd.print(autoMode ? "A" : "M");

    // ==== Gửi trạng thái qua Bluetooth dạng JSON ====
    String btJson = "{";
    btJson += "\"temp\":"; btJson += isnan(t) ? "null" : String(t, 1);
    btJson += ",\"humi\":"; btJson += isnan(h) ? "null" : String(h, 1);
    btJson += ",\"light\":\""; btJson += lightStatus; btJson += "\"";
    btJson += ",\"rain\":"; btJson += isRaining ? "true" : "false";
    btJson += ",\"action\":\""; btJson += isPhoi ? "PHOI" : "THU"; btJson += "\"";
    btJson += ",\"mode\":\""; btJson += autoMode ? "AUTO" : "MANUAL"; btJson += "\"";
    btJson += "}";
    btSerial.println(btJson);
    Serial.print("Gui Bluetooth Json: "); 
    Serial.println(btJson);
  }

  // --- Đọc dữ liệu nhận được từ Bluetooth và log ra Serial ---
  if (btSerial.available()) {
    String received = btSerial.readStringUntil('\n');
    received.trim();
    if (received.length() > 0) {
      Serial.print("Data tu Bluetooth: ");
      Serial.println(received);
      parseBTJson(received);
    }
  }
}
/*
  stepper_angle_control_ble.ino
  ───────────────────────
  28BYJ-48 ステッピングモーターを指定した角度まで回転させるプログラム
  Arduino R4 WiFi用 - BLE対応版（最適化版）
  
  作成者: Koki Takagi
  作成日: 2025年6月16日
*/

#include <AccelStepper.h>
#include <ArduinoBLE.h>

// モーターのピン設定
#define MOTOR_PIN1 8
#define MOTOR_PIN2 9
#define MOTOR_PIN3 10
#define MOTOR_PIN4 11

// 定数
const float STEPS_PER_REVOLUTION = 4096.0;
const float STEPS_PER_DEGREE = STEPS_PER_REVOLUTION / 360.0;
const unsigned long BLE_CHECK_INTERVAL = 100;  // BLEチェック間隔(ms)

// AccelStepperライブラリの初期化
AccelStepper stepper(AccelStepper::HALF4WIRE, MOTOR_PIN1, MOTOR_PIN3, MOTOR_PIN2, MOTOR_PIN4);

// BLEサービスとキャラクタリスティック
BLEService motorService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEStringCharacteristic motorCommandChar("19B10001-E8F2-537E-4F6C-D104768A1214", BLEWrite, 20);
BLEStringCharacteristic motorStatusChar("19B10002-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify, 50);

// 速度プリセット構造体
struct SpeedPreset {
  float speed;
  float acceleration;
  const char* description;
};

// 速度プリセット（5段階）
const SpeedPreset speedPresets[5] = {
  {400.0, 800.0, "非常に遅い"},
  {700.0, 1400.0, "遅い"},
  {1000.0, 2000.0, "通常"},
  {2000.0, 4000.0, "速い"},
  {4000.0, 8000.0, "非常に速い"}
};

// グローバル変数
float currentAngle = 0.0;
bool powerSavingMode = true;
int currentSpeedPreset = 2;  // デフォルトは通常速度

// BLE管理用変数
static unsigned long lastBLECheck = 0;
static bool bleConnected = false;

void setup() {
  // シリアル通信の初期化
  Serial.begin(9600);
  
  // BLEの初期化
  if (!BLE.begin()) {
    Serial.println("BLE初期化失敗");
    while (1);
  }
  
  // BLE設定
  BLE.setLocalName("StepperMotor");
  BLE.setAdvertisedService(motorService);
  motorService.addCharacteristic(motorCommandChar);
  motorService.addCharacteristic(motorStatusChar);
  BLE.addService(motorService);
  BLE.advertise();
  
  // モーター初期化
  initMotor();
  
  // 初期速度設定
  applySpeedPreset(currentSpeedPreset);
  
  Serial.println("BLE Stepper Motor Ready");
}

void loop() {
  // モーター制御（最優先）
  stepper.run();
  
  // モーター停止時の処理
  if (!stepper.isRunning() && powerSavingMode) {
    disableMotor();
  }
  
  // シリアルコマンド処理（軽量）
  if (Serial.available()) {
    processSerialCommand();
  }
  
  // BLE処理（定期実行）
  if (millis() - lastBLECheck >= BLE_CHECK_INTERVAL) {
    lastBLECheck = millis();
    processBLE();
  }
}

void initMotor() {
  // ピン設定
  pinMode(MOTOR_PIN1, OUTPUT);
  pinMode(MOTOR_PIN2, OUTPUT);
  pinMode(MOTOR_PIN3, OUTPUT);
  pinMode(MOTOR_PIN4, OUTPUT);
  
  // 初期状態
  disableMotor();
  stepper.setCurrentPosition(0);
}

void processBLE() {
  BLEDevice central = BLE.central();
  
  if (central) {
    // 接続状態の変化を検出
    if (!bleConnected) {
      bleConnected = true;
      Serial.println("BLE接続");
      updateBLEStatus();
    }
    
    // コマンド受信
    if (motorCommandChar.written()) {
      String command = motorCommandChar.value();
      command.trim();
      if (command.length() > 0) {
        processCommand(command);
      }
    }
  } else if (bleConnected) {
    bleConnected = false;
    Serial.println("BLE切断");
  }
}

void processSerialCommand() {
  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command.length() > 0) {
    processCommand(command);
  }
}

void processCommand(const String& command) {
  if (command.length() == 0) return;
  
  char cmd = command.charAt(0);
  String param = command.substring(1);
  
  switch (cmd) {
    case 'a': case 'A':  // 絶対角度
      if (param.length() > 0) {
        moveToAngle(param.toFloat());
      }
      break;
      
    case 'r': case 'R':  // 相対角度
      if (param.length() > 0) {
        moveRelative(param.toFloat());
      }
      break;
      
    case 'z': case 'Z':  // ゼロリセット
      resetPosition();
      break;
      
    case 's': case 'S':  // ステータス
      printStatus();
      break;
      
    case 'p': case 'P':  // 電源節約モード
      powerSavingMode = !powerSavingMode;
      sendResponse("電源節約: " + String(powerSavingMode ? "ON" : "OFF"));
      break;
      
    case 'v': case 'V':  // 速度設定
      if (param.length() > 0) {
        int preset = param.toInt() - 1;
        if (preset >= 0 && preset < 5) {
          applySpeedPreset(preset);
        }
      } else {
        printSpeedPresets();
      }
      break;
      
    case 'h': case 'H':  // ヘルプ
      printHelp();
      break;
  }
}

void moveToAngle(float targetAngle) {
  // 角度正規化 (0-360)
  targetAngle = normalizeAngle(targetAngle);
  
  // 最短経路計算
  float diff = calculateShortestPath(currentAngle, targetAngle);
  
  // 移動実行
  executeMoveSteps(round(diff * STEPS_PER_DEGREE));
  
  // 状態更新
  currentAngle = targetAngle;
  
  sendResponse("移動: " + String(currentAngle, 1) + "° → " + String(targetAngle, 1) + "°");
}

void moveRelative(float angle) {
  float targetAngle = normalizeAngle(currentAngle + angle);
  
  // 移動実行
  executeMoveSteps(round(angle * STEPS_PER_DEGREE));
  
  // 状態更新
  currentAngle = targetAngle;
  
  sendResponse("相対移動: " + String(angle, 1) + "°");
}

void resetPosition() {
  stepper.setCurrentPosition(0);
  currentAngle = 0.0;
  sendResponse("位置リセット");
}

void executeMoveSteps(long steps) {
  enableMotor();
  stepper.move(steps);
}

void applySpeedPreset(int preset) {
  currentSpeedPreset = preset;
  const SpeedPreset& sp = speedPresets[preset];
  
  stepper.setMaxSpeed(sp.speed);
  stepper.setAcceleration(sp.acceleration);
  
  sendResponse("速度: " + String(preset + 1) + " - " + sp.description);
}

float normalizeAngle(float angle) {
  while (angle >= 360.0) angle -= 360.0;
  while (angle < 0.0) angle += 360.0;
  return angle;
}

float calculateShortestPath(float from, float to) {
  float diff = to - from;
  if (diff > 180.0) diff -= 360.0;
  else if (diff < -180.0) diff += 360.0;
  return diff;
}

void printStatus() {
  String status = "角度:" + String(currentAngle, 1) + "deg ";
  status += "速度:" + String(speedPresets[currentSpeedPreset].speed, 0) + " ";
  status += "状態:" + String(stepper.isRunning() ? "動作中" : "停止");
  sendResponse(status);
}

void printSpeedPresets() {
  String msg = "速度設定:\n";
  for (int i = 0; i < 5; i++) {
    msg += String(i + 1) + ":" + speedPresets[i].description + "\n";
  }
  sendResponse(msg);
}

void printHelp() {
  sendResponse("a[角度]:絶対移動 r[角度]:相対移動 z:リセット s:状態 p:省電力 v[1-5]:速度 h:ヘルプ");
}

void enableMotor() {
  // AccelStepperが自動制御
}

void disableMotor() {
  digitalWrite(MOTOR_PIN1, LOW);
  digitalWrite(MOTOR_PIN2, LOW);
  digitalWrite(MOTOR_PIN3, LOW);
  digitalWrite(MOTOR_PIN4, LOW);
}

void sendResponse(const String& message) {
  Serial.println(message);
  
  // BLE送信（接続時のみ）
  if (bleConnected) {
    // ASCII文字のみを使用してメッセージを送信
    String asciiMessage = convertToAscii(message);
    asciiMessage += "\n";
    
    // 20バイト以下なら一回で送信
    if (asciiMessage.length() <= 20) {
      motorStatusChar.writeValue(asciiMessage);
    } else {
      // 長いメッセージは分割送信（ASCII文字のみなので安全）
      for (int i = 0; i < asciiMessage.length(); i += 20) {
        int chunkLen = min(20, (int)asciiMessage.length() - i);
        motorStatusChar.writeValue(asciiMessage.substring(i, i + chunkLen));
      }
    }
  }
}

String convertToAscii(const String& message) {
  String result = message;
  // 日本語の度記号を ASCII の "deg" に置換
  result.replace("°", "deg");
  // その他の非ASCII文字も必要に応じて置換
  result.replace("→", "->");
  return result;
}

void updateBLEStatus() {
  if (bleConnected) {
    String status = String(currentAngle, 1) + "," + 
                   String(stepper.isRunning() ? "RUN" : "STOP") + "," +
                   String(currentSpeedPreset + 1);
    motorStatusChar.writeValue(status);
  }
}
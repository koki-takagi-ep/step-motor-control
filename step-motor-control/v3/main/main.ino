/*
  stepper_angle_control_ble.ino
  ───────────────────────
  28BYJ-48 ステッピングモーターを指定した角度まで回転させるプログラム
  Arduino R4 WiFi用 - BLE対応版（リファクタリング版）
  
  作成者: Koki Takagi
  最終更新: 2025年6月17日
*/

#include <AccelStepper.h>
#include <ArduinoBLE.h>

// ハードウェア設定
namespace HardwareConfig {
  constexpr uint8_t MOTOR_PIN1 = 8;
  constexpr uint8_t MOTOR_PIN2 = 9;
  constexpr uint8_t MOTOR_PIN3 = 10;
  constexpr uint8_t MOTOR_PIN4 = 11;
}

// モーター設定
namespace MotorConfig {
  constexpr float STEPS_PER_REVOLUTION = 4096.0f;
  constexpr float STEPS_PER_DEGREE = STEPS_PER_REVOLUTION / 360.0f;
  constexpr uint8_t NUM_SPEED_PRESETS = 5;
}

// BLE設定
namespace BLEConfig {
  constexpr unsigned long CHECK_INTERVAL_MS = 50;
  constexpr uint16_t MAX_MESSAGE_LENGTH = 20;
  const char* DEVICE_NAME = "StepperMotor";
  const char* SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214";
  const char* COMMAND_UUID = "19B10001-E8F2-537E-4F6C-D104768A1214";
  const char* STATUS_UUID = "19B10002-E8F2-537E-4F6C-D104768A1214";
}

// AccelStepperライブラリの初期化
AccelStepper stepper(AccelStepper::HALF4WIRE, 
                    HardwareConfig::MOTOR_PIN1, 
                    HardwareConfig::MOTOR_PIN3, 
                    HardwareConfig::MOTOR_PIN2, 
                    HardwareConfig::MOTOR_PIN4);

// BLEサービスとキャラクタリスティック
BLEService motorService(BLEConfig::SERVICE_UUID);
BLEStringCharacteristic motorCommandChar(BLEConfig::COMMAND_UUID, BLEWrite, BLEConfig::MAX_MESSAGE_LENGTH);
BLEStringCharacteristic motorStatusChar(BLEConfig::STATUS_UUID, BLERead | BLENotify, 50);

// 速度プリセット構造体
struct SpeedPreset {
  float speed;
  float acceleration;
  const char* description;
  
  SpeedPreset(float s, float a, const char* desc) : speed(s), acceleration(a), description(desc) {}
};

// 速度プリセット（5段階）
const SpeedPreset speedPresets[MotorConfig::NUM_SPEED_PRESETS] = {
  SpeedPreset(400.0f, 800.0f, "非常に遅い"),
  SpeedPreset(700.0f, 1400.0f, "遅い"),
  SpeedPreset(1000.0f, 2000.0f, "通常"),
  SpeedPreset(2000.0f, 4000.0f, "速い"),
  SpeedPreset(4000.0f, 8000.0f, "非常に速い")
};

// モーター状態管理クラス
class MotorState {
public:
  float currentAngle = 0.0f;
  bool powerSavingMode = true;
  uint8_t currentSpeedPreset = 2;  // デフォルトは通常速度
  
  void reset() {
    currentAngle = 0.0f;
    powerSavingMode = true;
    currentSpeedPreset = 2;
  }
  
  bool isValidSpeedPreset(uint8_t preset) const {
    return preset < MotorConfig::NUM_SPEED_PRESETS;
  }
};

// BLE管理クラス
class BLEManager {
public:
  unsigned long lastCheck = 0;
  bool isConnected = false;
  
  bool shouldCheckBLE() {
    unsigned long now = millis();
    if (now - lastCheck >= BLEConfig::CHECK_INTERVAL_MS) {
      lastCheck = now;
      return true;
    }
    return false;
  }
};

// グローバルインスタンス
MotorState motorState;
BLEManager bleManager;

void setup() {
  // シリアル通信の初期化
  Serial.begin(9600);
  while (!Serial && millis() < 3000);  // シリアル待機（最大3秒）
  
  Serial.println("=== BLE Stepper Motor Controller ===");
  
  // システム初期化
  if (!initializeBLE()) {
    Serial.println("致命的エラー: BLE初期化失敗");
    while (1) delay(1000);
  }
  
  initializeMotor();
  applySpeedPreset(motorState.currentSpeedPreset);
  
  Serial.println("システム初期化完了 - 制御可能");
  printStartupInfo();
}

void loop() {
  // 最優先: モーター制御
  stepper.run();
  
  // 電源管理
  managePowerSaving();
  
  // 通信処理
  handleCommunications();
}

void managePowerSaving() {
  if (!stepper.isRunning() && motorState.powerSavingMode) {
    disableMotorPower();
  }
}

void handleCommunications() {
  // シリアル通信処理（高優先度）
  if (Serial.available()) {
    processSerialCommand();
  }
  
  // BLE処理（定期実行）
  if (bleManager.shouldCheckBLE()) {
    processBLE();
  }
}

bool initializeBLE() {
  if (!BLE.begin()) {
    return false;
  }
  
  // BLE設定
  BLE.setLocalName(BLEConfig::DEVICE_NAME);
  BLE.setAdvertisedService(motorService);
  motorService.addCharacteristic(motorCommandChar);
  motorService.addCharacteristic(motorStatusChar);
  BLE.addService(motorService);
  BLE.advertise();
  
  Serial.println("BLE初期化完了");
  return true;
}

void initializeMotor() {
  // ピン設定
  const uint8_t motorPins[] = {
    HardwareConfig::MOTOR_PIN1,
    HardwareConfig::MOTOR_PIN2,
    HardwareConfig::MOTOR_PIN3,
    HardwareConfig::MOTOR_PIN4
  };
  
  for (uint8_t pin : motorPins) {
    pinMode(pin, OUTPUT);
  }
  
  // 初期状態設定
  disableMotorPower();
  stepper.setCurrentPosition(0);
  motorState.reset();
  
  Serial.println("モーター初期化完了");
}

void printStartupInfo() {
  Serial.println("\n=== コマンド一覧 ===");
  Serial.println("a[角度]: 絶対角度移動 (例: a90)");
  Serial.println("r[角度]: 相対角度移動 (例: r45)");
  Serial.println("z: 位置リセット");
  Serial.println("s: ステータス表示");
  Serial.println("p: 電源節約モード切替");
  Serial.println("v[1-5]: 速度設定");
  Serial.println("h: ヘルプ表示");
  Serial.println("==================\n");
}

void processBLE() {
  BLEDevice central = BLE.central();
  
  if (central) {
    handleBLEConnection();
    handleBLECommands();
  } else {
    handleBLEDisconnection();
  }
}

void handleBLEConnection() {
  if (!bleManager.isConnected) {
    bleManager.isConnected = true;
    Serial.println("BLE接続確立");
    sendBLEStatus();
  }
}

void handleBLEDisconnection() {
  if (bleManager.isConnected) {
    bleManager.isConnected = false;
    Serial.println("BLE接続切断");
  }
}

void handleBLECommands() {
  if (motorCommandChar.written()) {
    String command = motorCommandChar.value();
    command.trim();
    if (command.length() > 0) {
      Serial.println("BLE < " + command);
      processCommand(command);
    }
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
  if (command.length() == 0) {
    sendResponse("エラー: 空のコマンド");
    return;
  }
  
  char cmd = toLowerCase(command.charAt(0));
  String param = command.substring(1);
  
  switch (cmd) {
    case 'a':  // 絶対角度
      handleAbsoluteMove(param);
      break;
      
    case 'r':  // 相対角度
      handleRelativeMove(param);
      break;
      
    case 'z':  // ゼロリセット
      resetPosition();
      break;
      
    case 's':  // ステータス
      printStatus();
      break;
      
    case 'p':  // 電源節約モード
      togglePowerSaving();
      break;
      
    case 'v':  // 速度設定
      handleSpeedCommand(param);
      break;
      
    case 'h':  // ヘルプ
      printHelp();
      break;
      
    default:
      sendResponse("エラー: 無効なコマンド '" + String(cmd) + "' (hでヘルプ)");
      break;
  }
}

char toLowerCase(char c) {
  return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

void handleAbsoluteMove(const String& param) {
  if (param.length() == 0) {
    sendResponse("エラー: 角度を指定してください (例: a90)");
    return;
  }
  moveToAngle(param.toFloat());
}

void handleRelativeMove(const String& param) {
  if (param.length() == 0) {
    sendResponse("エラー: 角度を指定してください (例: r45)");
    return;
  }
  moveRelative(param.toFloat());
}

void handleSpeedCommand(const String& param) {
  if (param.length() == 0) {
    printSpeedPresets();
    return;
  }
  
  uint8_t preset = param.toInt();
  if (preset >= 1 && preset <= MotorConfig::NUM_SPEED_PRESETS) {
    applySpeedPreset(preset - 1);
  } else {
    sendResponse("エラー: 速度は1-" + String(MotorConfig::NUM_SPEED_PRESETS) + "を指定");
  }
}

void togglePowerSaving() {
  motorState.powerSavingMode = !motorState.powerSavingMode;
  sendResponse("電源節約: " + String(motorState.powerSavingMode ? "ON" : "OFF"));
  
  if (!motorState.powerSavingMode && !stepper.isRunning()) {
    enableMotorHolding();
  }
}

void moveToAngle(float targetAngle) {
  if (!isValidAngle(targetAngle)) {
    sendResponse("エラー: 無効な角度値 " + String(targetAngle));
    return;
  }
  
  // 角度正規化 (0-360)
  targetAngle = normalizeAngle(targetAngle);
  
  // 最短経路計算
  float diff = calculateShortestPath(motorState.currentAngle, targetAngle);
  
  // 移動実行
  executeMoveSteps(round(diff * MotorConfig::STEPS_PER_DEGREE));
  
  // 状態更新
  float previousAngle = motorState.currentAngle;
  motorState.currentAngle = targetAngle;
  
  sendResponse("絶対移動: " + String(previousAngle, 1) + "° → " + String(targetAngle, 1) + "°");
}

bool isValidAngle(float angle) {
  return !isnan(angle) && isfinite(angle) && angle >= -3600.0f && angle <= 3600.0f;
}

void moveRelative(float angle) {
  if (!isValidAngle(angle)) {
    sendResponse("エラー: 無効な角度値 " + String(angle));
    return;
  }
  
  float targetAngle = normalizeAngle(motorState.currentAngle + angle);
  
  // 移動実行
  executeMoveSteps(round(angle * MotorConfig::STEPS_PER_DEGREE));
  
  // 状態更新
  motorState.currentAngle = targetAngle;
  
  String direction = (angle >= 0) ? "時計回り" : "反時計回り";
  sendResponse("相対移動: " + String(abs(angle), 1) + "° (" + direction + ")");
}

void resetPosition() {
  stepper.setCurrentPosition(0);
  motorState.currentAngle = 0.0f;
  sendResponse("位置リセット完了: 現在位置を0°に設定");
}

void executeMoveSteps(long steps) {
  if (steps == 0) {
    sendResponse("移動なし: 既に目標位置");
    return;
  }
  
  enableMotorPower();
  stepper.move(steps);
  
  // デバッグ情報
  String direction = (steps > 0) ? "正方向" : "負方向";
  Serial.println("移動開始: " + String(abs(steps)) + "ステップ (" + direction + ")");
}

void applySpeedPreset(uint8_t preset) {
  if (!motorState.isValidSpeedPreset(preset)) {
    sendResponse("エラー: 無効な速度プリセット " + String(preset));
    return;
  }
  
  motorState.currentSpeedPreset = preset;
  const SpeedPreset& sp = speedPresets[preset];
  
  stepper.setMaxSpeed(sp.speed);
  stepper.setAcceleration(sp.acceleration);
  
  sendResponse("速度設定: レベル" + String(preset + 1) + " (" + sp.description + ")");
  Serial.println("速度: " + String(sp.speed) + " steps/sec, 加速度: " + String(sp.acceleration));
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
  String status = "=== ステータス ===\n";
  status += "現在角度: " + String(motorState.currentAngle, 1) + "°\n";
  status += "モーター: " + String(stepper.isRunning() ? "動作中" : "停止") + "\n";
  status += "速度レベル: " + String(motorState.currentSpeedPreset + 1) + " (" + speedPresets[motorState.currentSpeedPreset].description + ")\n";
  status += "電源節約: " + String(motorState.powerSavingMode ? "ON" : "OFF") + "\n";
  status += "残りステップ: " + String(stepper.distanceToGo()) + "\n";
  status += "BLE接続: " + String(bleManager.isConnected ? "接続中" : "切断");
  
  sendResponse(status);
}

void printSpeedPresets() {
  String msg = "=== 速度設定 ===\n";
  for (uint8_t i = 0; i < MotorConfig::NUM_SPEED_PRESETS; i++) {
    String indicator = (i == motorState.currentSpeedPreset) ? " [現在]" : "";
    msg += String(i + 1) + ": " + speedPresets[i].description + indicator + "\n";
  }
  msg += "使用例: v3 (通常速度に設定)";
  sendResponse(msg);
}

void printHelp() {
  String help = "=== コマンドヘルプ ===\n";
  help += "a[角度]: 絶対角度移動 (例: a90)\n";
  help += "r[角度]: 相対角度移動 (例: r45, r-30)\n";
  help += "z: 位置リセット (現在位置を0°に)\n";
  help += "s: 詳細ステータス表示\n";
  help += "p: 電源節約モード切替\n";
  help += "v[1-5]: 速度設定 (1:遅い～5:速い)\n";
  help += "v: 利用可能な速度一覧\n";
  help += "h: このヘルプ表示";
  
  sendResponse(help);
}

void enableMotorPower() {
  // AccelStepperライブラリが自動でピン制御
  // 必要に応じて追加の初期化処理をここに記述
}

void disableMotorPower() {
  digitalWrite(HardwareConfig::MOTOR_PIN1, LOW);
  digitalWrite(HardwareConfig::MOTOR_PIN2, LOW);
  digitalWrite(HardwareConfig::MOTOR_PIN3, LOW);
  digitalWrite(HardwareConfig::MOTOR_PIN4, LOW);
}

void enableMotorHolding() {
  // モーターにホールディングトルクを適用
  // 必要に応じて実装
}

void sendResponse(const String& message) {
  // シリアル出力
  Serial.println("OUT: " + message);
  
  // BLE送信（接続時のみ）
  if (bleManager.isConnected) {
    sendBLEMessage(message);
  }
}

void sendBLEMessage(const String& message) {
  String asciiMessage = convertToAscii(message);
  asciiMessage += "\n";
  
  // メッセージサイズに応じて送信方法を選択
  if (asciiMessage.length() <= BLEConfig::MAX_MESSAGE_LENGTH) {
    motorStatusChar.writeValue(asciiMessage);
  } else {
    // 長いメッセージは分割送信
    sendBLEMessageChunked(asciiMessage);
  }
}

void sendBLEMessageChunked(const String& message) {
  const uint16_t chunkSize = BLEConfig::MAX_MESSAGE_LENGTH;
  
  for (uint16_t i = 0; i < message.length(); i += chunkSize) {
    uint16_t chunkLen = min(chunkSize, (uint16_t)(message.length() - i));
    String chunk = message.substring(i, i + chunkLen);
    
    motorStatusChar.writeValue(chunk);
    delay(10);  // チャンク間の短い遅延
  }
}

String convertToAscii(const String& message) {
  String result = message;
  
  // 特殊文字のASCII変換
  result.replace("°", "deg");
  result.replace("→", "->");
  result.replace("←", "<-");
  result.replace("　", " ");  // 全角スペースを半角に
  
  // 改行文字の正規化
  result.replace("\r\n", "\n");
  result.replace("\r", "\n");
  
  return result;
}

void sendBLEStatus() {
  if (bleManager.isConnected) {
    String status = createStatusString();
    motorStatusChar.writeValue(status);
  }
}

String createStatusString() {
  return String(motorState.currentAngle, 1) + "," + 
         String(stepper.isRunning() ? "RUN" : "STOP") + "," +
         String(motorState.currentSpeedPreset + 1) + "," +
         String(motorState.powerSavingMode ? "PWR_SAVE" : "PWR_FULL");
}
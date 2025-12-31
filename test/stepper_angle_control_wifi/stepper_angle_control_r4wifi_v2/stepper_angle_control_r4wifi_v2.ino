/*
  stepper_angle_control_r4wifi_v2.ino
  ───────────────────────
  28BYJ-48 ステッピングモーターを指定した角度まで回転させるプログラム バージョン2
  AccelStepperライブラリを使用
  Arduino R4 Wi-Fi 内蔵無線通信対応バージョン
  
  【追加機能】
  - ウェブインターフェースによる制御
  - OTAアップデート対応
  - プリセット位置の保存機能
  - モード切替機能（角度制御/連続回転）
  - 簡易セキュリティ機能
  
  作成者: Koki Takagi
  最終更新: 2025年5月12日
*/

#include <AccelStepper.h>
#include <WiFiS3.h>    // Arduino R4 Wi-Fi用のWi-Fiライブラリ
#include <ArduinoOTA.h> // OTAアップデート用
#include <EEPROM.h>     // プリセット位置保存用

// Wi-Fi設定
const char* ssid = "YOUR_WIFI_SSID";      // ご使用のWi-FiのSSID
const char* password = "YOUR_WIFI_PASSWORD";  // Wi-Fiのパスワード

// 管理者パスワード（簡易的なセキュリティ対策）
const char* adminPassword = "admin123";    // 管理者パスワード（変更推奨）

// ネットワーク設定
WiFiServer telnetServer(23);  // Telnetサーバー（ポート23）
WiFiServer webServer(80);     // Webサーバー（ポート80）
WiFiClient telnetClient;

// モーターのピン設定
#define MOTOR_PIN1 8
#define MOTOR_PIN2 9
#define MOTOR_PIN3 10
#define MOTOR_PIN4 11

// ステータスLED（R4 Wi-Fi内蔵LED）
#define LED_PIN LED_BUILTIN

// AccelStepperライブラリの初期化
// HALF4WIRE モードは28BYJ-48モーターに適しています
AccelStepper stepper(AccelStepper::HALF4WIRE, MOTOR_PIN1, MOTOR_PIN3, MOTOR_PIN2, MOTOR_PIN4);

// 28BYJ-48の1回転のステップ数
const float STEPS_PER_REVOLUTION = 4096.0;

// 角度からステップ数への変換係数（1度あたりのステップ数）
const float STEPS_PER_DEGREE = STEPS_PER_REVOLUTION / 360.0;

// 現在の速度と加速度
float currentSpeed = 1000.0;
float currentAcceleration = 500.0;

// 動作モード
enum OperationMode {
  ANGLE_CONTROL,    // 角度制御モード
  CONTINUOUS_CW,    // 連続時計回り回転
  CONTINUOUS_CCW,   // 連続反時計回り回転
  PATTERN_MODE      // パターン回転モード
};

OperationMode currentMode = ANGLE_CONTROL;

// 電源節約モードのフラグ
bool powerSavingMode = true;

// 現在の角度
float currentAngle = 0.0;

// プリセット位置（EEPROM保存用）
struct PresetPosition {
  float angle;
  char name[16];
  bool isUsed;
};

const int MAX_PRESETS = 10;
const int EEPROM_SIGNATURE_ADDR = 0;
const int EEPROM_START_ADDR = 4;
const long EEPROM_SIGNATURE = 0xABCD1234;  // EEPROM初期化確認用シグネチャ

// パターン回転用変数
int currentPattern = 0;
unsigned long patternStartTime = 0;
const unsigned long PATTERN_INTERVAL = 5000; // パターン切り替え間隔（ミリ秒）

// システム状態
unsigned long lastHeartbeat = 0;
const int HEARTBEAT_INTERVAL = 1000;  // ハートビート間隔（ミリ秒）
bool isAuthenticated = false;         // 認証状態フラグ

// コマンドバッファ
String inputBuffer = "";

// Webクライアント処理用変数
WiFiClient webClient;
String httpHeader;

void setup() {
  // シリアル通信の初期化（デバッグ用）
  Serial.begin(9600);
  Serial.println("\n28BYJ-48 角度制御システム v2（Arduino R4 Wi-Fi対応）");
  
  // 内蔵LEDを初期化
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Wi-Fi接続の初期化
  connectToWiFi();
  
  // OTAアップデートの設定
  setupOTA();
  
  // EEPROMの初期化
  initializeEEPROM();
  
  // サーバーを開始
  telnetServer.begin();
  webServer.begin();
  Serial.println("Telnetサーバーを開始しました（ポート23）");
  Serial.println("Webサーバーを開始しました（ポート80）");
  
  // モーターピンを出力モードに設定
  pinMode(MOTOR_PIN1, OUTPUT);
  pinMode(MOTOR_PIN2, OUTPUT);
  pinMode(MOTOR_PIN3, OUTPUT);
  pinMode(MOTOR_PIN4, OUTPUT);
  
  // 初期状態ではすべてのピンをLOWに設定
  digitalWrite(MOTOR_PIN1, LOW);
  digitalWrite(MOTOR_PIN2, LOW);
  digitalWrite(MOTOR_PIN3, LOW);
  digitalWrite(MOTOR_PIN4, LOW);
  
  // モーターの最大速度と加速度を設定
  stepper.setMaxSpeed(currentSpeed);
  stepper.setAcceleration(currentAcceleration);
  
  // 初期位置を0に設定
  stepper.setCurrentPosition(0);
  
  // 使用方法を表示
  printCommands();
}

// Wi-Fiに接続する関数
void connectToWiFi() {
  Serial.print("Wi-Fi ネットワーク '");
  Serial.print(ssid);
  Serial.print("' に接続中...");
  
  // Wi-Fi接続を開始
  WiFi.begin(ssid, password);
  
  // 接続が確立されるまで待機（タイムアウト付き）
  int retryCount = 0;
  const int maxRetries = 20;  // 最大リトライ回数
  
  while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // LEDを点滅
    delay(500);
    Serial.print(".");
    retryCount++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fiに接続しました");
    Serial.print("IPアドレス: ");
    Serial.println(WiFi.localIP());
    
    // 接続成功を示すLEDパターン
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  } else {
    Serial.println("Wi-Fi接続に失敗しました。スケッチをリセットしてください。");
    // 接続失敗を示すLEDパターン
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);
      delay(50);
    }
  }
}

// OTAアップデート設定
void setupOTA() {
  ArduinoOTA.setHostname("StepperController");  // OTAホスト名
  ArduinoOTA.setPassword(adminPassword);        // OTAパスワード
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "スケッチ";
    } else {
      type = "ファイルシステム";
    }
    Serial.println("OTAアップデート開始: " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nアップデート完了");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("進行状況: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("エラー[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("認証失敗");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("開始失敗");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("接続失敗");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("受信失敗");
    } else if (error == OTA_END_ERROR) {
      Serial.println("終了失敗");
    }
  });
  
  ArduinoOTA.begin();
  Serial.println("OTAアップデートの準備完了");
}

// EEPROMの初期化
void initializeEEPROM() {
  // EEPROMサイズ確認
  if (EEPROM.length() < 512) {
    Serial.println("警告: EEPROM容量が不足しています");
    return;
  }
  
  // EEPROMが初期化されているか確認
  long signature;
  EEPROM.get(EEPROM_SIGNATURE_ADDR, signature);
  
  if (signature != EEPROM_SIGNATURE) {
    // EEPROMを初期化
    Serial.println("EEPROMを初期化しています...");
    
    // シグネチャを書き込み
    EEPROM.put(EEPROM_SIGNATURE_ADDR, EEPROM_SIGNATURE);
    
    // プリセット位置を初期化
    for (int i = 0; i < MAX_PRESETS; i++) {
      PresetPosition preset = {0.0, "プリセット", false};
      int addr = EEPROM_START_ADDR + (i * sizeof(PresetPosition));
      EEPROM.put(addr, preset);
    }
    
    Serial.println("EEPROM初期化完了");
  } else {
    Serial.println("既存のEEPROMデータを使用します");
  }
}

// プリセット位置を保存
void savePreset(int index, float angle, const char* name) {
  if (index < 0 || index >= MAX_PRESETS) {
    Serial.println("エラー: 無効なプリセットインデックス");
    return;
  }
  
  PresetPosition preset;
  strncpy(preset.name, name, 15);
  preset.name[15] = '\0';  // NULL終端を保証
  preset.angle = angle;
  preset.isUsed = true;
  
  int addr = EEPROM_START_ADDR + (index * sizeof(PresetPosition));
  EEPROM.put(addr, preset);
  
  Serial.print("プリセット ");
  Serial.print(index);
  Serial.print(" を保存しました: ");
  Serial.print(preset.name);
  Serial.print(" (");
  Serial.print(preset.angle);
  Serial.println("度)");
}

// プリセット位置を読み込み
PresetPosition loadPreset(int index) {
  PresetPosition preset = {0.0, "未使用", false};
  
  if (index < 0 || index >= MAX_PRESETS) {
    Serial.println("エラー: 無効なプリセットインデックス");
    return preset;
  }
  
  int addr = EEPROM_START_ADDR + (index * sizeof(PresetPosition));
  EEPROM.get(addr, preset);
  
  return preset;
}

// コマンド一覧を表示する関数
void printCommands() {
  // コマンドヘルプメッセージ
  String helpMessage = "\n------ コマンド一覧 ------\n";
  helpMessage += "【角度制御】\n";
  helpMessage += "a[角度]: 絶対角度を指定（例: a90 で90度の位置へ移動）\n";
  helpMessage += "r[角度]: 相対角度を指定（例: r45 で現在位置から45度移動）\n";
  helpMessage += "z: 現在位置を0度にリセット\n";
  
  helpMessage += "\n【モード設定】\n";
  helpMessage += "m0: 角度制御モード\n";
  helpMessage += "m1: 連続時計回り回転\n";
  helpMessage += "m2: 連続反時計回り回転\n";
  helpMessage += "m3: パターン回転モード\n";
  
  helpMessage += "\n【プリセット】\n";
  helpMessage += "ps[番号] [名前]: プリセットを保存（例: ps1 ホームポジション）\n";
  helpMessage += "pl[番号]: プリセットを読み込み（例: pl1）\n";
  helpMessage += "pd: プリセット一覧表示\n";
  
  helpMessage += "\n【システム設定】\n";
  helpMessage += "s: 現在の状態を表示\n";
  helpMessage += "p: 電源節約モードの切り替え（現在:" + String(powerSavingMode ? "有効" : "無効") + "）\n";
  helpMessage += "v[1-5]: 速度プリセットを選択（1:遅い, 3:通常, 5:高速）\n";
  helpMessage += "h: このヘルプメッセージを表示\n";
  helpMessage += "login [パスワード]: 管理者として認証\n";
  helpMessage += "reboot: システムを再起動（要管理者認証）\n";
  helpMessage += "------------------------";
  
  Serial.println(helpMessage);
  
  // Telnetクライアントが接続されている場合はそちらにも送信
  if (telnetClient && telnetClient.connected()) {
    telnetClient.println(helpMessage);
  }
}

void loop() {
  // OTAアップデートのハンドリング
  ArduinoOTA.handle();
  
  // 現在の動作モードに応じた処理
  handleOperationMode();
  
  // ハートビートLED
  handleHeartbeat();
  
  // Telnetクライアント処理
  handleTelnetClient();
  
  // Webサーバー処理
  handleWebServer();
  
  // USB シリアル入力をチェック
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();  // 空白や改行を削除
    
    if (command.length() > 0) {
      processCommand(command);
    }
  }
}

// 動作モードに応じた処理
void handleOperationMode() {
  switch (currentMode) {
    case ANGLE_CONTROL:
      // 通常の角度制御モード
      stepper.run();
      
      // 電源節約モードが有効で、モーターが目標位置に到達したら電源をオフ
      if (powerSavingMode && stepper.distanceToGo() == 0) {
        // すべてのモーターピンをLOWに設定して電流を止める
        digitalWrite(MOTOR_PIN1, LOW);
        digitalWrite(MOTOR_PIN2, LOW);
        digitalWrite(MOTOR_PIN3, LOW);
        digitalWrite(MOTOR_PIN4, LOW);
      }
      break;
      
    case CONTINUOUS_CW:
      // 連続時計回り回転
      stepper.setSpeed(currentSpeed);
      stepper.runSpeed();
      break;
      
    case CONTINUOUS_CCW:
      // 連続反時計回り回転
      stepper.setSpeed(-currentSpeed);
      stepper.runSpeed();
      break;
      
    case PATTERN_MODE:
      // パターン回転モード
      runPatternMode();
      break;
  }
}

// パターン回転モードの処理
void runPatternMode() {
  unsigned long currentTime = millis();
  
  // パターン切り替え時間になったか確認
  if (currentTime - patternStartTime >= PATTERN_INTERVAL) {
    // 次のパターンに切り替え
    currentPattern = (currentPattern + 1) % 4;
    patternStartTime = currentTime;
    
    String patternMsg = "パターン切り替え: ";
    
    switch (currentPattern) {
      case 0:  // 時計回り1回転
        patternMsg += "時計回り1回転";
        moveToRelativeAngle(360.0);
        break;
        
      case 1:  // 反時計回り1回転
        patternMsg += "反時計回り1回転";
        moveToRelativeAngle(-360.0);
        break;
        
      case 2:  // 時計回りに90度ずつ動く
        patternMsg += "90度ステップ（時計回り）";
        moveToRelativeAngle(90.0);
        break;
        
      case 3:  // 反時計回りに90度ずつ動く
        patternMsg += "90度ステップ（反時計回り）";
        moveToRelativeAngle(-90.0);
        break;
    }
    
    Serial.println(patternMsg);
    if (telnetClient && telnetClient.connected()) {
      telnetClient.println(patternMsg);
    }
  }
  
  // モーターを実行
  stepper.run();
}

// ハートビートLEDの処理
void handleHeartbeat() {
  unsigned long currentTime = millis();
  
  // ハートビート間隔ごとにLEDを点滅
  if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = currentTime;
    
    // 動作モードに応じたLEDパターン
    switch (currentMode) {
      case ANGLE_CONTROL:
        // 角度制御モード: ゆっくり点滅
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        break;
        
      case CONTINUOUS_CW:
      case CONTINUOUS_CCW:
        // 連続回転モード: 高速点滅
        digitalWrite(LED_PIN, HIGH);
        delay(50);
        digitalWrite(LED_PIN, LOW);
        break;
        
      case PATTERN_MODE:
        // パターンモード: 2回点滅
        digitalWrite(LED_PIN, HIGH);
        delay(50);
        digitalWrite(LED_PIN, LOW);
        delay(50);
        digitalWrite(LED_PIN, HIGH);
        delay(50);
        digitalWrite(LED_PIN, LOW);
        break;
    }
  }
}

// Telnetクライアントの処理
void handleTelnetClient() {
  // 新しいクライアント接続をチェック
  if (!telnetClient || !telnetClient.connected()) {
    telnetClient = telnetServer.available();
    if (telnetClient) {
      Serial.println("新しいTelnetクライアントが接続しました");
      telnetClient.println("28BYJ-48 角度制御システム v2に接続しました");
      telnetClient.println("コマンド一覧を表示するには 'h' を入力してください");
      isAuthenticated = false;  // 新しい接続は未認証状態から始める
    }
  }
  
  // TCP クライアントからのデータを処理
  if (telnetClient && telnetClient.connected() && telnetClient.available()) {
    char c = telnetClient.read();
    
    // 改行コードを検出したらコマンドを処理
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
}

// Webサーバーの処理
void handleWebServer() {
  // 新しいWebクライアント接続をチェック
  webClient = webServer.available();
  
  if (webClient) {
    Serial.println("新しいWebクライアントが接続しました");
    
    // リクエストの最初の行のみを変数に格納する
    String currentLine = "";
    httpHeader = "";
    
    // クライアントからデータが利用可能な間は
    unsigned long currentTime = millis();
    unsigned long previousTime = currentTime;
    const long timeoutTime = 2000; // タイムアウト時間（ミリ秒）
    
    while (webClient.connected() && currentTime - previousTime <= timeoutTime) {
      currentTime = millis();
      
      if (webClient.available()) {
        char c = webClient.read();
        httpHeader += c;
        
        // HTTPリクエストの終了を検出
        if (c == '\n' && currentLine.length() == 0) {
          // GETリクエストのパスを抽出
          int startPos = httpHeader.indexOf("GET ") + 4;
          int endPos = httpHeader.indexOf(" HTTP");
          String path = httpHeader.substring(startPos, endPos);
          
          // 認証チェック（簡易的な実装）
          boolean isAuth = httpHeader.indexOf("Authorization: Basic") >= 0;
          
          // APIリクエストの処理
          if (path.startsWith("/api/")) {
            handleApiRequest(path);
          } else {
            // HTMLページの送信
            sendWebPage();
          }
          
          break;
        }
        
        // 新しい行の開始
        if (c == '\n') {
          currentLine = "";
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    
    // 接続を閉じる
    httpHeader = "";
    webClient.stop();
    Serial.println("Webクライアントの接続を終了しました");
  }
}

// APIリクエストの処理
void handleApiRequest(String path) {
  // レスポンスヘッダー
  webClient.println("HTTP/1.1 200 OK");
  webClient.println("Content-Type: application/json");
  webClient.println("Connection: close");
  webClient.println("Access-Control-Allow-Origin: *");
  webClient.println();
  
  // パスの解析
  if (path == "/api/status") {
    // ステータス情報のJSON
    sendStatusJson();
  } else if (path.startsWith("/api/move/")) {
    // モーター移動コマンド
    int startPos = path.indexOf("/move/") + 6;
    int endPos = path.indexOf("/", startPos);
    String type = "";
    String value = "";
    
    if (endPos > 0) {
      type = path.substring(startPos, endPos);
      value = path.substring(endPos + 1);
    } else {
      type = path.substring(startPos);
    }
    
    handleMoveCommand(type, value);
  } else if (path == "/api/presets") {
    // プリセットリストJSON
    sendPresetsJson();
  } else if (path.startsWith("/api/preset/")) {
    // プリセット操作
    handlePresetCommand(path);
  } else if (path.startsWith("/api/mode/")) {
    // モード変更
    String mode = path.substring(path.lastIndexOf("/") + 1);
    int modeValue = mode.toInt();
    
    if (modeValue >= 0 && modeValue <= 3) {
      currentMode = (OperationMode)modeValue;
      
      // モード変更成功のJSON
      webClient.print("{\"status\":\"success\",\"mode\":");
      webClient.print(modeValue);
      webClient.println("}");
    } else {
      // エラーJSON
      webClient.println("{\"status\":\"error\",\"message\":\"Invalid mode value\"}");
    }
  } else {
    // 不明なAPIエンドポイント
    webClient.println("{\"status\":\"error\",\"message\":\"Unknown API endpoint\"}");
  }
}

// ステータス情報をJSON形式で送信
void sendStatusJson() {
  float currentSteps = stepper.currentPosition();
  float currentDegrees = (currentSteps / STEPS_PER_DEGREE);
  
  // 角度の値を正規化（0〜360度の範囲内に）
  while (currentDegrees >= 360) currentDegrees -= 360;
  while (currentDegrees < 0) currentDegrees += 360;
  
  String json = "{";
  json += "\"angle\":" + String(currentDegrees) + ",";
  json += "\"target_distance\":" + String(stepper.distanceToGo() / STEPS_PER_DEGREE) + ",";
  json += "\"mode\":" + String(currentMode) + ",";
  json += "\"power_saving\":" + String(powerSavingMode ? "true" : "false") + ",";
  json += "\"speed\":" + String(currentSpeed) + ",";
  json += "\"step_position\":" + String(currentSteps);
  json += "}";
  
  webClient.println(json);
}

// モーター移動コマンドの処理
void handleMoveCommand(String type, String value) {
  float angle = value.toFloat();
  bool success = false;
  
  if (type == "absolute") {
    moveToAbsoluteAngle(angle);
    success = true;
  } else if (type == "relative") {
    moveToRelativeAngle(angle);
    success = true;
  } else if (type == "preset" && value.toInt() >= 0 && value.toInt() < MAX_PRESETS) {
    int presetIndex = value.toInt();
    PresetPosition preset = loadPreset(presetIndex);
    
    if (preset.isUsed) {
      moveToAbsoluteAngle(preset.angle);
      success = true;
    }
  }
  
  if (success) {
    webClient.println("{\"status\":\"success\"}");
  } else {
    webClient.println("{\"status\":\"error\",\"message\":\"Invalid move command\"}");
  }
}

// プリセットリストをJSON形式で送信
void sendPresetsJson() {
  String json = "{\"presets\":[";
  
  for (int i = 0; i < MAX_PRESETS; i++) {
    PresetPosition preset = loadPreset(i);
    
    if (i > 0) {
      json += ",";
    }
    
    json += "{";
    json += "\"index\":" + String(i) + ",";
    json += "\"name\":\"" + String(preset.name) + "\",";
    json += "\"angle\":" + String(preset.angle) + ",";
    json += "\"used\":" + String(preset.isUsed ? "true" : "false");
    json += "}";
  }
  
  json += "]}";
  webClient.println(json);
}

// プリセットコマンドの処理
void handlePresetCommand(String path) {
  if (path.startsWith("/api/preset/save/")) {
    // プリセット保存 /api/preset/save/[index]/[angle]/[name]
    int startPos = path.indexOf("/save/") + 6;
    String remaining = path.substring(startPos);
    
    int firstSlash = remaining.indexOf("/");
    int secondSlash = remaining.indexOf("/", firstSlash + 1);
    
    if (firstSlash > 0 && secondSlash > 0) {
      int index = remaining.substring(0, firstSlash).toInt();
      float angle = remaining.substring(firstSlash + 1, secondSlash).toFloat();
      String name = remaining.substring(secondSlash + 1);
      
      // URLデコード
      name.replace("%20", " ");
      
      if (index >= 0 && index < MAX_PRESETS) {
        savePreset(index, angle, name.c_str());
        webClient.println("{\"status\":\"success\"}");
      } else {
        webClient.println("{\"status\":\"error\",\"message\":\"Invalid preset index\"}");
      }
    } else {
      webClient.println("{\"status\":\"error\",\"message\":\"Invalid preset save command\"}");
    }
  } else if (path.startsWith("/api/preset/load/")) {
    // プリセット読み込み /api/preset/load/[index]
    int index = path.substring(path.lastIndexOf("/") + 1).toInt();
    
    if (index >= 0 && index < MAX_PRESETS) {
      PresetPosition preset = loadPreset(index);
      
      if (preset.isUsed) {
        moveToAbsoluteAngle(preset.angle);
        webClient.println("{\"status\":\"success\"}");
      } else {
        webClient.println("{\"status\":\"error\",\"message\":\"Preset not used\"}");
      }
    } else {
      webClient.println("{\"status\":\"error\",\"message\":\"Invalid preset index\"}");
    }
  }
}

// Webページの送信
void sendWebPage() {
  // レスポンスヘッダー
  webClient.println("HTTP/1.1 200 OK");
  webClient.println("Content-Type: text/html");
  webClient.println("Connection: close");
  webClient.println();
  
  // HTML開始タグ
  webClient.println("<!DOCTYPE html>");
  webClient.println("<html lang=\"ja\">");
  webClient.println("<head>");
  webClient.println("<meta charset=\"UTF-8\">");
  webClient.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
  webClient.println("<title>ステッピングモーター制御システム</title>");
  
  // CSS
  webClient.println("<style>");
  webClient.println("body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f2f2f2; }");
  webClient.println(".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }");
  webClient.println("h1 { color: #333; text-align: center; }");
  webClient.println(".control-panel { display: flex; flex-wrap: wrap; gap: 20px; margin: 20px 0; }");
  webClient.println(".control-card { flex: 1; min-width: 200px; border: 1px solid #ddd; border-radius: 8px; padding: 15px; background: #f9f9f9; }");
  webClient.println("h2 { color: #555; font-size: 1.2em; margin-top: 0; }");
  webClient.println(".btn { background: #4CAF50; color: white; border: none; padding: 8px 15px; margin: 5px 0; border-radius: 4px; cursor: pointer; }");
  webClient.println(".btn:hover { background: #45a049; }");
  webClient.println(".btn-danger { background: #f44336; }");
  webClient.println(".btn-danger:hover { background: #d32f2f; }");
  webClient.println(".btn-info { background: #2196F3; }");
  webClient.println(".btn-info:hover { background: #0b7dda; }");
  webClient.println("input[type=range] { width: 100%; }");
  webClient.println("input[type=number], input[type=text] { width: 100%; padding: 8px; margin: 5px 0; box-sizing: border-box; border: 1px solid #ddd; border-radius: 4px; }");
  webClient.println(".status-display { border: 1px solid #ddd; border-radius: 4px; padding: 10px; margin: 10px 0; background: #fff; }");
  webClient.println(".status-value { font-weight: bold; }");
  webClient.println(".preset-list { max-height: 150px; overflow-y: auto; margin: 10px 0; border: 1px solid #ddd; border-radius: 4px; }");
  webClient.println(".preset-item { padding: 8px; border-bottom: 1px solid #eee; display: flex; justify-content: space-between; align-items: center; }");
  webClient.println(".preset-item:last-child { border-bottom: none; }");
  webClient.println(".slider-value { text-align: center; font-weight: bold; margin: 5px 0; }");
  webClient.println(".footer { margin-top: 30px; text-align: center; font-size: 0.8em; color: #777; }");
  webClient.println(".tab-container { display: flex; margin-bottom: 20px; }");
  webClient.println(".tab { padding: 10px 20px; background: #ddd; cursor: pointer; border-radius: 5px 5px 0 0; margin-right: 5px; }");
  webClient.println(".tab.active { background: #fff; border: 1px solid #ddd; border-bottom: none; }");
  webClient.println(".tab-content { display: none; }");
  webClient.println(".tab-content.active { display: block; }");
  webClient.println("@media (max-width: 600px) { .control-card { min-width: 100%; } }");
  webClient.println("</style>");
  
  webClient.println("</head>");
  webClient.println("<body>");
  
  // HTML本体
  webClient.println("<div class=\"container\">");
  webClient.println("<h1>ステッピングモーター制御システム</h1>");
  
  // タブナビゲーション
  webClient.println("<div class=\"tab-container\">");
  webClient.println("<div class=\"tab active\" data-tab=\"control\">制御パネル</div>");
  webClient.println("<div class=\"tab\" data-tab=\"presets\">プリセット</div>");
  webClient.println("<div class=\"tab\" data-tab=\"settings\">設定</div>");
  webClient.println("</div>");
  
  // 制御パネルタブ
  webClient.println("<div id=\"control\" class=\"tab-content active\">");
  webClient.println("<div class=\"control-panel\">");
  
  // 角度制御カード
  webClient.println("<div class=\"control-card\">");
  webClient.println("<h2>角度制御</h2>");
  webClient.println("<div class=\"status-display\">現在角度: <span id=\"current-angle\" class=\"status-value\">0</span>°</div>");
  webClient.println("<input type=\"range\" id=\"angle-slider\" min=\"0\" max=\"360\" value=\"0\" step=\"5\">");
  webClient.println("<div class=\"slider-value\"><span id=\"angle-value\">0</span>°</div>");
  webClient.println("<div>");
  webClient.println("<button class=\"btn\" onclick=\"moveToAngle()\">移動</button>");
  webClient.println("<button class=\"btn btn-info\" onclick=\"resetPosition()\">位置リセット</button>");
  webClient.println("</div>");
  webClient.println("</div>");
  
  // 相対移動カード
  webClient.println("<div class=\"control-card\">");
  webClient.println("<h2>相対移動</h2>");
  webClient.println("<input type=\"number\" id=\"relative-angle\" placeholder=\"相対角度 (-360〜360)\" min=\"-360\" max=\"360\">");
  webClient.println("<div>");
  webClient.println("<button class=\"btn\" onclick=\"moveRelative()\">移動</button>");
  webClient.println("<button class=\"btn btn-info\" onclick=\"moveRelative(-90)\">-90°</button>");
  webClient.println("<button class=\"btn btn-info\" onclick=\"moveRelative(90)\">+90°</button>");
  webClient.println("</div>");
  webClient.println("</div>");
  
  // 動作モードカード
  webClient.println("<div class=\"control-card\">");
  webClient.println("<h2>動作モード</h2>");
  webClient.println("<div class=\"status-display\">現在: <span id=\"current-mode\" class=\"status-value\">角度制御</span></div>");
  webClient.println("<div>");
  webClient.println("<button class=\"btn\" onclick=\"setMode(0)\">角度制御</button>");
  webClient.println("<button class=\"btn\" onclick=\"setMode(1)\">時計回り連続</button>");
  webClient.println("<button class=\"btn\" onclick=\"setMode(2)\">反時計回り連続</button>");
  webClient.println("<button class=\"btn\" onclick=\"setMode(3)\">パターン回転</button>");
  webClient.println("</div>");
  webClient.println("</div>");
  
  webClient.println("</div>"); // control-panel end
  webClient.println("</div>"); // control tab end
  
  // プリセットタブ
  webClient.println("<div id=\"presets\" class=\"tab-content\">");
  webClient.println("<div class=\"control-panel\">");
  
  // プリセット保存カード
  webClient.println("<div class=\"control-card\">");
  webClient.println("<h2>プリセット保存</h2>");
  webClient.println("<input type=\"text\" id=\"preset-name\" placeholder=\"プリセット名\">");
  webClient.println("<select id=\"preset-index\"></select>");
  webClient.println("<button class=\"btn\" onclick=\"saveCurrentPositionAsPreset()\">現在位置を保存</button>");
  webClient.println("</div>");
  
  // プリセットリストカード
  webClient.println("<div class=\"control-card\">");
  webClient.println("<h2>プリセット一覧</h2>");
  webClient.println("<div id=\"preset-list\" class=\"preset-list\"></div>");
  webClient.println("</div>");
  
  webClient.println("</div>"); // control-panel end
  webClient.println("</div>"); // presets tab end
  
  // 設定タブ
  webClient.println("<div id=\"settings\" class=\"tab-content\">");
  webClient.println("<div class=\"control-panel\">");
  
  // 速度設定カード
  webClient.println("<div class=\"control-card\">");
  webClient.println("<h2>速度設定</h2>");
  webClient.println("<input type=\"range\" id=\"speed-slider\" min=\"1\" max=\"5\" value=\"3\" step=\"1\">");
  webClient.println("<div class=\"slider-value\">プリセット: <span id=\"speed-value\">3</span></div>");
  webClient.println("<button class=\"btn\" onclick=\"setSpeedPreset()\">速度設定</button>");
  webClient.println("</div>");
  
  // 電源節約カード
  webClient.println("<div class=\"control-card\">");
  webClient.println("<h2>電源設定</h2>");
  webClient.println("<div class=\"status-display\">電源節約: <span id=\"power-saving\" class=\"status-value\">有効</span></div>");
  webClient.println("<button class=\"btn\" onclick=\"togglePowerSaving()\">切り替え</button>");
  webClient.println("</div>");
  
  webClient.println("</div>"); // control-panel end
  webClient.println("</div>"); // settings tab end
  
  // フッター
  webClient.println("<div class=\"footer\">");
  webClient.println("<p>ステッピングモーター角度制御システム v2 | Arduino R4 Wi-Fi</p>");
  webClient.println("<p>IPアドレス: " + WiFi.localIP().toString() + " | <span id=\"connection-status\">接続中</span></p>");
  webClient.println("</div>");
  
  webClient.println("</div>"); // container end
  
  // JavaScript
  webClient.println("<script>");
  // タブ切り替え
  webClient.println("document.querySelectorAll('.tab').forEach(tab => {");
  webClient.println("  tab.addEventListener('click', () => {");
  webClient.println("    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));");
  webClient.println("    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));");
  webClient.println("    tab.classList.add('active');");
  webClient.println("    document.getElementById(tab.dataset.tab).classList.add('active');");
  webClient.println("  });");
  webClient.println("});");
  
  // スライダーの値表示
  webClient.println("document.getElementById('angle-slider').addEventListener('input', function() {");
  webClient.println("  document.getElementById('angle-value').textContent = this.value;");
  webClient.println("});");
  
  webClient.println("document.getElementById('speed-slider').addEventListener('input', function() {");
  webClient.println("  document.getElementById('speed-value').textContent = this.value;");
  webClient.println("});");
  
  // APIアクセス関数
  webClient.println("async function getStatus() {");
  webClient.println("  try {");
  webClient.println("    const response = await fetch('/api/status');");
  webClient.println("    const data = await response.json();");
  webClient.println("    document.getElementById('current-angle').textContent = data.angle.toFixed(1);");
  webClient.println("    const modes = ['角度制御', '時計回り連続', '反時計回り連続', 'パターン回転'];");
  webClient.println("    document.getElementById('current-mode').textContent = modes[data.mode];");
  webClient.println("    document.getElementById('power-saving').textContent = data.power_saving ? '有効' : '無効';");
  webClient.println("  } catch (e) {");
  webClient.println("    document.getElementById('connection-status').textContent = '切断';");
  webClient.println("    document.getElementById('connection-status').style.color = 'red';");
  webClient.println("  }");
  webClient.println("}");
  
  // プリセット関数
  webClient.println("async function getPresets() {");
  webClient.println("  try {");
  webClient.println("    const response = await fetch('/api/presets');");
  webClient.println("    const data = await response.json();");
  webClient.println("    const presetList = document.getElementById('preset-list');");
  webClient.println("    const presetSelect = document.getElementById('preset-index');");
  webClient.println("    presetList.innerHTML = '';");
  webClient.println("    presetSelect.innerHTML = '';");
  
  webClient.println("    data.presets.forEach(preset => {");
  webClient.println("      if (preset.used) {");
  webClient.println("        const item = document.createElement('div');");
  webClient.println("        item.className = 'preset-item';");
  webClient.println("        item.innerHTML = `");
  webClient.println("          <span>${preset.name} (${preset.angle.toFixed(1)}°)</span>");
  webClient.println("          <button class=\"btn\" onclick=\"loadPreset(${preset.index})\">移動</button>");
  webClient.println("        `;");
  webClient.println("        presetList.appendChild(item);");
  webClient.println("      }");
  
  webClient.println("      const option = document.createElement('option');");
  webClient.println("      option.value = preset.index;");
  webClient.println("      option.textContent = `プリセット ${preset.index}${preset.used ? ` (${preset.name})` : ' (未使用)'}`;");
  webClient.println("      presetSelect.appendChild(option);");
  webClient.println("    });");
  webClient.println("  } catch (e) {");
  webClient.println("    console.error('プリセット取得エラー:', e);");
  webClient.println("  }");
  webClient.println("}");
  
  // 制御機能
  webClient.println("function moveToAngle() {");
  webClient.println("  const angle = document.getElementById('angle-slider').value;");
  webClient.println("  fetch(`/api/move/absolute/${angle}`);");
  webClient.println("}");
  
  webClient.println("function moveRelative(angle) {");
  webClient.println("  if (angle === undefined) {");
  webClient.println("    angle = document.getElementById('relative-angle').value;");
  webClient.println("  }");
  webClient.println("  fetch(`/api/move/relative/${angle}`);");
  webClient.println("}");
  
  webClient.println("function resetPosition() {");
  webClient.println("  fetch('/api/move/reset');");
  webClient.println("  setTimeout(getStatus, 500);");
  webClient.println("}");
  
  webClient.println("function setMode(mode) {");
  webClient.println("  fetch(`/api/mode/${mode}`);");
  webClient.println("  setTimeout(getStatus, 500);");
  webClient.println("}");
  
  webClient.println("function setSpeedPreset() {");
  webClient.println("  const preset = document.getElementById('speed-slider').value;");
  webClient.println("  fetch(`/api/speed/${preset}`);");
  webClient.println("}");
  
  webClient.println("function togglePowerSaving() {");
  webClient.println("  fetch('/api/power/toggle');");
  webClient.println("  setTimeout(getStatus, 500);");
  webClient.println("}");
  
  webClient.println("function saveCurrentPositionAsPreset() {");
  webClient.println("  const name = document.getElementById('preset-name').value || '未命名';");
  webClient.println("  const index = document.getElementById('preset-index').value;");
  webClient.println("  fetch(`/api/status`).then(r => r.json()).then(data => {");
  webClient.println("    fetch(`/api/preset/save/${index}/${data.angle}/${name}`);");
  webClient.println("    setTimeout(getPresets, 500);");
  webClient.println("  });");
  webClient.println("}");
  
  webClient.println("function loadPreset(index) {");
  webClient.println("  fetch(`/api/preset/load/${index}`);");
  webClient.println("}");
  
  // ページロード時の初期処理
  webClient.println("window.onload = function() {");
  webClient.println("  getStatus();");
  webClient.println("  getPresets();");
  webClient.println("  setInterval(getStatus, 2000);");
  webClient.println("};");
  
  webClient.println("</script>");
  
  webClient.println("</body>");
  webClient.println("</html>");
}

// コマンドを処理する関数
void processCommand(String command) {
  Serial.print("コマンド受信: ");
  Serial.println(command);
  
  char cmd = command.charAt(0);
  String valueStr = "";
  
  if (command.length() > 1) {
    valueStr = command.substring(1);
  }
  
  float value = 0;
  if (valueStr.length() > 0) {
    value = valueStr.toFloat();
  }
  
  switch (cmd) {
    case 'a':  // 絶対角度（0〜360度）
      if (valueStr.length() > 0) {
        moveToAbsoluteAngle(value);
      } else {
        sendErrorMessage("エラー: 角度を指定してください（例: a90）");
      }
      break;
      
    case 'r':  // 相対角度（現在位置からの相対的な角度）
      if (valueStr.length() > 0) {
        moveToRelativeAngle(value);
      } else {
        sendErrorMessage("エラー: 角度を指定してください（例: r45）");
      }
      break;
      
    case 'z':  // 現在位置を0度にリセット
      stepper.setCurrentPosition(0);
      currentAngle = 0;
      sendMessage("現在位置を0度にリセットしました");
      break;
      
    case 's':  // 状態表示
      sendStatusMessage();
      break;
      
    case 'p':  // 電源節約モードの切り替え
      powerSavingMode = !powerSavingMode;
      sendMessage("電源節約モードを" + String(powerSavingMode ? "有効にしました" : "無効にしました"));
      
      // 電源節約モードが無効の場合、コイルを保持するためにモーターを再起動
      if (!powerSavingMode && stepper.distanceToGo() == 0) {
        // AccelStepperがピンの状態を復元するために少し動かして戻す
        stepper.move(1);
        stepper.run();
        stepper.move(-1);
        stepper.run();
      }
      break;
      
    case 'v':  // 速度プリセット
      if (valueStr.length() > 0) {
        int speedPreset = valueStr.toInt();
        setSpeedPreset(speedPreset);
      } else {
        sendErrorMessage("エラー: 速度プリセットを指定してください（例: v3）");
      }
      break;
      
    case 'm':  // 動作モード設定
      if (valueStr.length() > 0) {
        int modeValue = valueStr.toInt();
        
        if (modeValue >= 0 && modeValue <= 3) {
          currentMode = (OperationMode)modeValue;
          
          String modeNames[] = {"角度制御", "連続時計回り回転", "連続反時計回り回転", "パターン回転"};
          sendMessage("動作モードを「" + modeNames[modeValue] + "」に設定しました");
          
          // パターンモードの場合、開始時間をリセット
          if (currentMode == PATTERN_MODE) {
            patternStartTime = millis();
            currentPattern = 0;
          }
        } else {
          sendErrorMessage("エラー: 無効なモード値です。0〜3の値を指定してください。");
        }
      } else {
        sendErrorMessage("エラー: モード番号を指定してください（例: m0）");
      }
      break;
      
    case 'p':
      if (command.startsWith("ps")) {  // プリセット保存
        // 書式: ps[番号] [名前]
        int spacePos = command.indexOf(' ');
        
        if (spacePos > 0) {
          int presetIndex = command.substring(2, spacePos).toInt();
          String presetName = command.substring(spacePos + 1);
          
          if (presetIndex >= 0 && presetIndex < MAX_PRESETS) {
            float currentSteps = stepper.currentPosition();
            float currentDegrees = (currentSteps / STEPS_PER_DEGREE);
            
            // 角度の値を正規化（0〜360度の範囲内に）
            while (currentDegrees >= 360) currentDegrees -= 360;
            while (currentDegrees < 0) currentDegrees += 360;
            
            savePreset(presetIndex, currentDegrees, presetName.c_str());
          } else {
            sendErrorMessage("エラー: 無効なプリセットインデックスです。0〜" + String(MAX_PRESETS - 1) + "の値を指定してください。");
          }
        } else {
          sendErrorMessage("エラー: プリセット名を指定してください（例: ps1 ホームポジション）");
        }
      } else if (command.startsWith("pl")) {  // プリセット読み込み
        // 書式: pl[番号]
        int presetIndex = valueStr.substring(1).toInt();
        
        if (presetIndex >= 0 && presetIndex < MAX_PRESETS) {
          PresetPosition preset = loadPreset(presetIndex);
          
          if (preset.isUsed) {
            moveToAbsoluteAngle(preset.angle);
            sendMessage("プリセット " + String(presetIndex) + " を読み込みました: " + String(preset.name) + " (" + String(preset.angle) + "度)");
          } else {
            sendErrorMessage("エラー: 指定されたプリセットは未使用です");
          }
        } else {
          sendErrorMessage("エラー: 無効なプリセットインデックスです。0〜" + String(MAX_PRESETS - 1) + "の値を指定してください。");
        }
      } else if (command == "pd") {  // プリセット一覧表示
        sendPresetListMessage();
      }
      break;
      
    case 'h':  // ヘルプ
      printCommands();
      break;
      
    case 'l':
      if (command.startsWith("login ")) {  // 管理者認証
        String inputPass = command.substring(6);
        
        if (inputPass == adminPassword) {
          isAuthenticated = true;
          sendMessage("管理者として認証されました");
        } else {
          sendErrorMessage("認証失敗: パスワードが一致しません");
        }
      }
      break;
      
    case 'r':
      if (command == "reboot") {  // システム再起動
        if (isAuthenticated) {
          sendMessage("システムを再起動します...");
          delay(1000);
          // ソフトウェアリセット
          NVIC_SystemReset();
        } else {
          sendErrorMessage("エラー: 管理者権限が必要です。'login [パスワード]'で認証してください。");
        }
      }
      break;
      
    default:
      sendErrorMessage("エラー: 無効なコマンドです。'h'でヘルプを表示します。");
      break;
  }
}

// 速度プリセット設定
void setSpeedPreset(int speedPreset) {
  String speedMsg = "";
  
  switch (speedPreset) {
    case 1:  // 非常に遅い
      currentSpeed = 400.0;
      currentAcceleration = 200.0;
      speedMsg = "速度: 非常に遅い（高トルク）";
      break;
    case 2:  // 遅い
      currentSpeed = 700.0;
      currentAcceleration = 350.0;
      speedMsg = "速度: 遅い";
      break;
    case 3:  // 通常
      currentSpeed = 1000.0;
      currentAcceleration = 500.0;
      speedMsg = "速度: 通常";
      break;
    case 4:  // 速い
      currentSpeed = 1300.0;
      currentAcceleration = 650.0;
      speedMsg = "速度: 速い";
      break;
    case 5:  // 非常に速い
      currentSpeed = 1600.0;
      currentAcceleration = 800.0;
      speedMsg = "速度: 非常に速い（低トルク）";
      break;
    default:
      sendErrorMessage("エラー: 無効な速度プリセットです。1〜5の値を指定してください。");
      return;
  }
  
  stepper.setMaxSpeed(currentSpeed);
  stepper.setAcceleration(currentAcceleration);
  
  sendMessage(speedMsg);
}

// 絶対角度への移動
void moveToAbsoluteAngle(float angle) {
  // 角度制御モードに切り替え
  if (currentMode != ANGLE_CONTROL) {
    currentMode = ANGLE_CONTROL;
    sendMessage("モードを角度制御に切り替えました");
  }
  
  // 角度の値を正規化（0〜360度の範囲内に）
  while (angle >= 360) angle -= 360;
  while (angle < 0) angle += 360;
  
  float targetSteps = angle * STEPS_PER_DEGREE;
  float currentSteps = stepper.currentPosition();
  float currentDegrees = currentSteps / STEPS_PER_DEGREE;
  
  // 現在の角度を更新
  currentAngle = angle;
  
  // 最短経路を計算（時計回りか反時計回りか）
  float diff = angle - currentDegrees;
  while (diff > 180) diff -= 360;
  while (diff <= -180) diff += 360;
  
  // 新しい目標位置を設定（最短経路で）
  long newPosition = currentSteps + (diff * STEPS_PER_DEGREE);
  stepper.moveTo(newPosition);
  
  sendMessage("絶対角度 " + String(angle) + " 度へ移動します");
}

// 相対角度への移動
void moveToRelativeAngle(float angle) {
  // 角度制御モードに切り替え
  if (currentMode != ANGLE_CONTROL) {
    currentMode = ANGLE_CONTROL;
    sendMessage("モードを角度制御に切り替えました");
  }
  
  // 現在のステップ数と角度を取得
  float currentSteps = stepper.currentPosition();
  float currentDegrees = currentSteps / STEPS_PER_DEGREE;
  
  // 相対角度を計算
  float targetDegrees = currentDegrees + angle;
  
  // 角度の値を正規化（0〜360度の範囲内に）
  while (targetDegrees >= 360) targetDegrees -= 360;
  while (targetDegrees < 0) targetDegrees += 360;
  
  // 現在の角度を更新
  currentAngle = targetDegrees;
  
  // 新しい目標位置を設定
  long newPosition = currentSteps + (angle * STEPS_PER_DEGREE);
  stepper.moveTo(newPosition);
  
  sendMessage("相対角度 " + String(angle) + " 度移動します（目標角度: " + String(targetDegrees) + " 度）");
}

// ステータスメッセージ送信
void sendStatusMessage() {
  float currentSteps = stepper.currentPosition();
  float currentDegrees = (currentSteps / STEPS_PER_DEGREE);
  
  // 角度の値を正規化（0〜360度の範囲内に）
  while (currentDegrees >= 360) currentDegrees -= 360;
  while (currentDegrees < 0) currentDegrees += 360;
  
  String modeNames[] = {"角度制御", "連続時計回り回転", "連続反時計回り回転", "パターン回転"};
  
  // ステータスメッセージの作成
  String statusMsg = "\n------ モーター状態 ------\n";
  statusMsg += "現在位置: " + String(currentDegrees) + " 度\n";
  statusMsg += "目標位置までの角度: " + String(stepper.distanceToGo() / STEPS_PER_DEGREE) + " 度\n";
  statusMsg += "現在のステップ数: " + String(currentSteps) + "\n";
  statusMsg += "動作モード: " + modeNames[currentMode] + "\n";
  statusMsg += "現在の速度: " + String(currentSpeed) + " steps/sec\n";
  statusMsg += "電源節約モード: " + String(powerSavingMode ? "有効" : "無効") + "\n";
  statusMsg += "------------------------";
  
  Serial.println(statusMsg);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.println(statusMsg);
  }
}

// プリセットリスト表示
void sendPresetListMessage() {
  String presetMsg = "\n------ プリセット一覧 ------\n";
  int usedCount = 0;
  
  for (int i = 0; i < MAX_PRESETS; i++) {
    PresetPosition preset = loadPreset(i);
    
    if (preset.isUsed) {
      presetMsg += String(i) + ": " + String(preset.name) + " (" + String(preset.angle) + "度)\n";
      usedCount++;
    }
  }
  
  if (usedCount == 0) {
    presetMsg += "保存されたプリセットはありません\n";
  }
  
  presetMsg += "------------------------";
  
  Serial.println(presetMsg);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.println(presetMsg);
  }
}

// 一般メッセージ送信
void sendMessage(String message) {
  Serial.println(message);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.println(message);
  }
}

// エラーメッセージ送信
void sendErrorMessage(String message) {
  Serial.println(message);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.println(message);
  }
}

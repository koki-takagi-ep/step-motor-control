/*
  stepper_angle_control_r4wifi.ino
  ───────────────────────
  28BYJ-48 ステッピングモーターを指定した角度まで回転させるプログラム
  AccelStepperライブラリを使用
  Arduino R4 Wi-Fi 内蔵無線通信対応バージョン
  
  作成者: Koki Takagi
  作成日: 2025年5月10日
*/

#include <AccelStepper.h>
#include <WiFiS3.h>    // Arduino R4 Wi-Fi用のWi-Fiライブラリ

// Wi-Fi設定
const char* ssid = "YOUR_WIFI_SSID";      // ご使用のWi-FiのSSID
const char* password = "YOUR_WIFI_PASSWORD";  // Wi-Fiのパスワード

// TCPサーバーの設定
WiFiServer server(23);  // ポート23（Telnet）を使用
WiFiClient client;

// モーターのピン設定
// 注意: Arduino R4ではピン配置が異なる場合があります。必要に応じて変更してください
#define MOTOR_PIN1 8
#define MOTOR_PIN2 9
#define MOTOR_PIN3 10
#define MOTOR_PIN4 11

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

// 電源節約モードのフラグ
bool powerSavingMode = true;

// 現在の角度
float currentAngle = 0.0;

// コマンドバッファ
String inputBuffer = "";

void setup() {
  // シリアル通信の初期化（デバッグ用）
  Serial.begin(9600);
  Serial.println("28BYJ-48 角度制御システム（Arduino R4 Wi-Fi対応）");
  
  // Wi-Fi接続の初期化
  connectToWiFi();
  
  // TCPサーバーを開始
  server.begin();
  Serial.println("TCPサーバーを開始しました（ポート23）");
  
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
  
  // 注意: 28BYJ-48の一般的な最大速度は約1100-1200 steps/secです
  // 負荷や電源電圧によって変わります
  
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
  
  // 接続が確立されるまで待機
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("Wi-Fiに接続しました");
  Serial.print("IPアドレス: ");
  Serial.println(WiFi.localIP());
}

// コマンド一覧を表示する関数
void printCommands() {
  // シリアル出力
  String helpMessage = "\n------ コマンド一覧 ------\n";
  helpMessage += "a[角度]: 絶対角度を指定（例: a90 で90度の位置へ移動）\n";
  helpMessage += "r[角度]: 相対角度を指定（例: r45 で現在位置から45度移動）\n";
  helpMessage += "z: 現在位置を0度にリセット\n";
  helpMessage += "s: 現在の状態を表示\n";
  helpMessage += "p: 電源節約モードの切り替え（現在:" + String(powerSavingMode ? "有効" : "無効") + "）\n";
  helpMessage += "v[1-5]: 速度プリセットを選択（1:遅い, 3:通常, 5:高速）\n";
  helpMessage += "h: このヘルプメッセージを表示\n";
  helpMessage += "------------------------";
  
  Serial.println(helpMessage);
  
  // クライアントが接続されている場合はそちらにも送信
  if (client && client.connected()) {
    client.println(helpMessage);
  }
}

void loop() {
  // モーターを実行（非ブロッキング）
  stepper.run();
  
  // 電源節約モードが有効で、モーターが目標位置に到達したら電源をオフ
  if (powerSavingMode && stepper.distanceToGo() == 0) {
    // すべてのモーターピンをLOWに設定して電流を止める
    digitalWrite(MOTOR_PIN1, LOW);
    digitalWrite(MOTOR_PIN2, LOW);
    digitalWrite(MOTOR_PIN3, LOW);
    digitalWrite(MOTOR_PIN4, LOW);
  }
  
  // 新しいクライアント接続をチェック
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("新しいクライアントが接続しました");
      client.println("28BYJ-48 角度制御システムに接続しました");
      client.println("コマンド一覧を表示するには 'h' を入力してください");
    }
  }
  
  // TCP クライアントからのデータを処理
  if (client && client.connected() && client.available()) {
    char c = client.read();
    
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
  
  // USB シリアル入力をチェック
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();  // 空白や改行を削除
    
    if (command.length() > 0) {
      processCommand(command);
    }
  }
}

// コマンドを処理する関数
void processCommand(String command) {
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
        // 角度の値を正規化（0〜360度の範囲内に）
        while (value >= 360) value -= 360;
        while (value < 0) value += 360;
        
        float targetSteps = value * STEPS_PER_DEGREE;
        float currentSteps = stepper.currentPosition();
        float currentDegrees = currentSteps / STEPS_PER_DEGREE;
        
        // 現在の角度を更新
        currentAngle = value;
        
        // 最短経路を計算（時計回りか反時計回りか）
        float diff = value - currentDegrees;
        while (diff > 180) diff -= 360;
        while (diff <= -180) diff += 360;
        
        // 新しい目標位置を設定（最短経路で）
        long newPosition = currentSteps + (diff * STEPS_PER_DEGREE);
        stepper.moveTo(newPosition);
        
        // メッセージ送信
        String responseMsg = "絶対角度 " + String(value) + " 度へ移動します";
        Serial.println(responseMsg);
        if (client && client.connected()) {
          client.println(responseMsg);
        }
      } else {
        String errorMsg = "エラー: 角度を指定してください（例: a90）";
        Serial.println(errorMsg);
        if (client && client.connected()) {
          client.println(errorMsg);
        }
      }
      break;
      
    case 'r':  // 相対角度（現在位置からの相対的な角度）
      if (valueStr.length() > 0) {
        // 現在のステップ数と角度を取得
        float currentSteps = stepper.currentPosition();
        float currentDegrees = currentSteps / STEPS_PER_DEGREE;
        
        // 相対角度を計算
        float targetDegrees = currentDegrees + value;
        
        // 角度の値を正規化（0〜360度の範囲内に）
        while (targetDegrees >= 360) targetDegrees -= 360;
        while (targetDegrees < 0) targetDegrees += 360;
        
        // 現在の角度を更新
        currentAngle = targetDegrees;
        
        // 新しい目標位置を設定
        long newPosition = currentSteps + (value * STEPS_PER_DEGREE);
        stepper.moveTo(newPosition);
        
        // メッセージ送信
        String responseMsg = "相対角度 " + String(value) + " 度移動します（目標角度: " + String(targetDegrees) + " 度）";
        Serial.println(responseMsg);
        if (client && client.connected()) {
          client.println(responseMsg);
        }
      } else {
        String errorMsg = "エラー: 角度を指定してください（例: r45）";
        Serial.println(errorMsg);
        if (client && client.connected()) {
          client.println(errorMsg);
        }
      }
      break;
      
    case 'z':  // 現在位置を0度にリセット
      stepper.setCurrentPosition(0);
      currentAngle = 0;
      String resetMsg = "現在位置を0度にリセットしました";
      Serial.println(resetMsg);
      if (client && client.connected()) {
        client.println(resetMsg);
      }
      break;
      
    case 's':  // 状態表示
      {
        float currentSteps = stepper.currentPosition();
        float currentDegrees = (currentSteps / STEPS_PER_DEGREE);
        
        // 角度の値を正規化（0〜360度の範囲内に）
        while (currentDegrees >= 360) currentDegrees -= 360;
        while (currentDegrees < 0) currentDegrees += 360;
        
        // ステータスメッセージの作成
        String statusMsg = "\n------ モーター状態 ------\n";
        statusMsg += "現在位置: " + String(currentDegrees) + " 度\n";
        statusMsg += "目標位置までの角度: " + String(stepper.distanceToGo() / STEPS_PER_DEGREE) + " 度\n";
        statusMsg += "現在のステップ数: " + String(currentSteps) + "\n";
        statusMsg += "現在の速度: " + String(currentSpeed) + " steps/sec\n";
        statusMsg += "電源節約モード: " + String(powerSavingMode ? "有効" : "無効") + "\n";
        statusMsg += "------------------------";
        
        Serial.println(statusMsg);
        if (client && client.connected()) {
          client.println(statusMsg);
        }
      }
      break;
      
    case 'p':  // 電源節約モードの切り替え
      powerSavingMode = !powerSavingMode;
      String powerMsg = "電源節約モードを" + String(powerSavingMode ? "有効にしました" : "無効にしました");
      Serial.println(powerMsg);
      if (client && client.connected()) {
        client.println(powerMsg);
      }
      
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
            speedMsg = "エラー: 無効な速度プリセットです。1〜5の値を指定してください。";
            Serial.println(speedMsg);
            if (client && client.connected()) {
              client.println(speedMsg);
            }
            return;
        }
        
        stepper.setMaxSpeed(currentSpeed);
        stepper.setAcceleration(currentAcceleration);
        
        Serial.println(speedMsg);
        if (client && client.connected()) {
          client.println(speedMsg);
        }
      } else {
        String errorMsg = "エラー: 速度プリセットを指定してください（例: v3）";
        Serial.println(errorMsg);
        if (client && client.connected()) {
          client.println(errorMsg);
        }
      }
      break;
      
    case 'h':  // ヘルプ
      printCommands();
      break;
      
    default:
      String errorMsg = "エラー: 無効なコマンドです。'h'でヘルプを表示します。";
      Serial.println(errorMsg);
      if (client && client.connected()) {
        client.println(errorMsg);
      }
      break;
  }
}

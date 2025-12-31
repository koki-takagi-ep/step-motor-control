/*
  stepper_test.ino
  ───────────────
  28BYJ-48 ステッピングモーターの簡単なテスト
  AccelStepperライブラリを使用
  
  作成者: Koki Takagi
  作成日: 2025年5月10日
*/

#include <AccelStepper.h>

// モーターのピン設定
#define MOTOR_PIN1 8
#define MOTOR_PIN2 9
#define MOTOR_PIN3 10
#define MOTOR_PIN4 11

// AccelStepperライブラリの初期化
// HALF4WIRE モードは28BYJ-48モーターに適しています
AccelStepper stepper(AccelStepper::HALF4WIRE, MOTOR_PIN1, MOTOR_PIN3, MOTOR_PIN2, MOTOR_PIN4);

// 28BYJ-48の1回転のステップ数
const int STEPS_PER_REVOLUTION = 4096;

void setup() {
  // シリアル通信の初期化
  Serial.begin(9600);
  Serial.println("28BYJ-48 Stepper Motor Test");
  
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
  stepper.setMaxSpeed(1000.0);
  stepper.setAcceleration(500.0);
  
  // 注意: 28BYJ-48の一般的な最大速度は約1100-1200 steps/secです
  // 負荷や電源電圧によって変わります
  
  // 初期位置を0に設定
  stepper.setCurrentPosition(0);
  
  // 使用方法の表示
  Serial.println("コマンド一覧:");
  Serial.println("1: 時計回りに1回転");
  Serial.println("2: 反時計回りに1回転");
  Serial.println("3: 時計回りに半回転");
  Serial.println("4: 反時計回りに半回転");
  Serial.println("5: 速度を上げる (+200 steps/sec)");
  Serial.println("6: 速度を下げる (-200 steps/sec)");
  Serial.println("f: 高速モード (1600 steps/sec)");
  Serial.println("t: 超高速モード (2000 steps/sec - 脱調の可能性あり)");
  Serial.println("n: 通常速度に戻す (1000 steps/sec)");
  Serial.println("0: 停止してモーターの電源を切る");
  Serial.println("s: モーターの状態を表示");
  Serial.println("p: 電源節約モードの切り替え(現在: 有効)");
}

// 現在の速度と加速度
float currentSpeed = 1000.0;
float currentAcceleration = 500.0;

// 電源節約モードのフラグ
bool powerSavingMode = true;

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
  
  // シリアル入力をチェック
  if (Serial.available() > 0) {
    char command = Serial.read();
    
    switch (command) {
      case '1':
        // 時計回りに1回転
        Serial.println("時計回りに1回転...");
        stepper.moveTo(stepper.currentPosition() + STEPS_PER_REVOLUTION);
        break;
        
      case '2':
        // 反時計回りに1回転
        Serial.println("反時計回りに1回転...");
        stepper.moveTo(stepper.currentPosition() - STEPS_PER_REVOLUTION);
        break;
        
      case '3':
        // 時計回りに半回転
        Serial.println("時計回りに半回転...");
        stepper.moveTo(stepper.currentPosition() + STEPS_PER_REVOLUTION/2);
        break;
        
      case '4':
        // 反時計回りに半回転
        Serial.println("反時計回りに半回転...");
        stepper.moveTo(stepper.currentPosition() - STEPS_PER_REVOLUTION/2);
        break;
        
      case '5':
        // 速度を上げる
        currentSpeed += 200.0;
        currentAcceleration += 100.0;
        stepper.setMaxSpeed(currentSpeed);
        stepper.setAcceleration(currentAcceleration);
        Serial.print("速度を上げました: ");
        Serial.print(currentSpeed);
        Serial.println(" steps/sec");
        
        // 警告表示
        if (currentSpeed > 1200.0) {
          Serial.println("警告: 1200 steps/secを超える速度ではモーターが脱調する可能性があります");
        }
        break;
        
      case '6':
        // 速度を下げる
        if (currentSpeed > 200) {
          currentSpeed -= 200.0;
          currentAcceleration -= 100.0;
          if (currentAcceleration < 50.0) currentAcceleration = 50.0;
          stepper.setMaxSpeed(currentSpeed);
          stepper.setAcceleration(currentAcceleration);
          Serial.print("速度を下げました: ");
          Serial.print(currentSpeed);
          Serial.println(" steps/sec");
        } else {
          Serial.println("最低速度に達しました");
        }
        break;
        
      case '0':
        // 停止してモーターの電源を切る
        Serial.println("モーターを停止します...");
        stepper.stop(); // 加速度を考慮して停止
        stepper.setCurrentPosition(stepper.currentPosition()); // 現在位置を目標位置に設定
        
        // モーターの電源を切る
        digitalWrite(MOTOR_PIN1, LOW);
        digitalWrite(MOTOR_PIN2, LOW);
        digitalWrite(MOTOR_PIN3, LOW);
        digitalWrite(MOTOR_PIN4, LOW);
        Serial.println("モーターの電源を切りました");
        break;
        
      case 's':
        // モーターの状態を表示
        Serial.print("現在位置: ");
        Serial.println(stepper.currentPosition());
        Serial.print("目標位置までの距離: ");
        Serial.println(stepper.distanceToGo());
        Serial.print("現在の速度: ");
        Serial.print(currentSpeed);
        Serial.println(" steps/sec");
        Serial.print("電源節約モード: ");
        Serial.println(powerSavingMode ? "有効" : "無効");
        break;
        
      case 'f':
        // 高速モード (1600 steps/sec)
        currentSpeed = 1600.0;
        currentAcceleration = 800.0;
        stepper.setMaxSpeed(currentSpeed);
        stepper.setAcceleration(currentAcceleration);
        Serial.println("高速モードに設定しました (1600 steps/sec)");
        Serial.println("注意: 高速ではトルクが低下します");
        break;
        
      case 't':
        // 超高速モード (2000 steps/sec - 脱調の可能性あり)
        currentSpeed = 2000.0;
        currentAcceleration = 1000.0;
        stepper.setMaxSpeed(currentSpeed);
        stepper.setAcceleration(currentAcceleration);
        Serial.println("超高速モードに設定しました (2000 steps/sec)");
        Serial.println("警告: この速度ではモーターが脱調する可能性が高いです");
        Serial.println("赤信号で停止する準備をしてください");
        break;
        
      case 'n':
        // 通常速度に戻す (1000 steps/sec)
        currentSpeed = 1000.0;
        currentAcceleration = 500.0;
        stepper.setMaxSpeed(currentSpeed);
        stepper.setAcceleration(currentAcceleration);
        Serial.println("通常速度に設定しました (1000 steps/sec)");
        break;
        
      case 'p':
        // 電源節約モードの切り替え
        powerSavingMode = !powerSavingMode;
        Serial.print("電源節約モードを");
        Serial.println(powerSavingMode ? "有効にしました" : "無効にしました");
        
        // 電源節約モードが無効の場合、コイルを保持するためにモーターを再起動
        if (!powerSavingMode && stepper.distanceToGo() == 0) {
          // AccelStepperがピンの状態を復元するために少し動かして戻す
          stepper.move(1);
          stepper.run();
          stepper.move(-1);
          stepper.run();
        }
        break;
        
      default:
        // 無効なコマンドは無視
        if (command != '\n' && command != '\r') {
          Serial.println("無効なコマンドです。以下のいずれかを入力してください:");
          Serial.println("1-6: 基本動作");
          Serial.println("0: 停止");
          Serial.println("s: 状態表示");
          Serial.println("p: 電源節約モード切り替え");
          Serial.println("f: 高速モード");
          Serial.println("t: 超高速モード");
          Serial.println("n: 通常速度に戻す");
        }
    }
  }
}

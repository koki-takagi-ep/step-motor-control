/*
  stepper_angle_memory.ino
  ───────────────────────
  28BYJ-48 ステッピングモーターを指定した角度まで回転させ、
  角度を記憶するプログラム（電源を切っても記憶）
  AccelStepperライブラリとEEPROMを使用
  
  作成者: Koki Takagi
  作成日: 2025年5月10日
*/

#include <AccelStepper.h>
#include <EEPROM.h>

// モーターのピン設定
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

// メモリアドレス定義
#define MEMORY_VALID_FLAG_ADDR 0    // 有効なデータがあるかのフラグアドレス
#define CURRENT_ANGLE_ADDR 4        // 角度を保存するアドレス

// メモリデータが有効であることを示す値
#define MEMORY_VALID_FLAG 0xAB12CD34

void setup() {
  // シリアル通信の初期化
  Serial.begin(9600);
  Serial.println("28BYJ-48 角度制御システム（角度記憶機能付き）");
  
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
  
  // 保存されたデータがあれば読み込む
  loadAngleFromMemory();
  
  // 使用方法の表示
  printCommands();
}

// 角度をEEPROMから読み込む関数
void loadAngleFromMemory() {
  // メモリが有効かチェック
  long memoryFlag;
  EEPROM.get(MEMORY_VALID_FLAG_ADDR, memoryFlag);
  
  if (memoryFlag == MEMORY_VALID_FLAG) {
    // 有効なデータがあれば読み込み
    EEPROM.get(CURRENT_ANGLE_ADDR, currentAngle);
    
    // 角度の値を正規化（0〜360度の範囲内に）
    while (currentAngle >= 360) currentAngle -= 360;
    while (currentAngle < 0) currentAngle += 360;
    
    // 現在位置を設定
    stepper.setCurrentPosition(currentAngle * STEPS_PER_DEGREE);
    
    Serial.print("メモリから角度を読み込みました: ");
    Serial.print(currentAngle);
    Serial.println(" 度");
  } else {
    // 有効なデータがなければ初期位置を0に設定
    currentAngle = 0;
    stepper.setCurrentPosition(0);
    Serial.println("保存された角度情報が見つかりません。初期位置を0度に設定しました。");
    
    // 現在の角度をメモリに保存
    saveAngleToMemory();
  }
}

// 角度をEEPROMに保存する関数
void saveAngleToMemory() {
  // 有効フラグを書き込み
  EEPROM.put(MEMORY_VALID_FLAG_ADDR, (long)MEMORY_VALID_FLAG);
  
  // 角度を書き込み
  EEPROM.put(CURRENT_ANGLE_ADDR, currentAngle);
  
  Serial.print("角度 ");
  Serial.print(currentAngle);
  Serial.println(" 度をメモリに保存しました");
}

// コマンド一覧を表示する関数
void printCommands() {
  Serial.println("\n------ コマンド一覧 ------");
  Serial.println("a[角度]: 絶対角度を指定（例: a90 で90度の位置へ移動）");
  Serial.println("r[角度]: 相対角度を指定（例: r45 で現在位置から45度移動）");
  Serial.println("z: 現在位置を0度にリセット");
  Serial.println("s: 現在の状態を表示");
  Serial.println("m: メモリに保存された角度を表示");
  Serial.println("w: 現在の角度をメモリに保存");
  Serial.println("l: メモリから角度を読み込み");
  Serial.println("c: メモリをクリア（工場出荷状態にリセット）");
  Serial.println("p: 電源節約モードの切り替え（現在:" + String(powerSavingMode ? "有効" : "無効") + "）");
  Serial.println("v[1-5]: 速度プリセットを選択（1:遅い, 3:通常, 5:高速）");
  Serial.println("h: このヘルプメッセージを表示");
  Serial.println("------------------------");
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
  
  // シリアル入力をチェック
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
        
        Serial.print("絶対角度 ");
        Serial.print(value);
        Serial.println(" 度へ移動します");
        
        // 移動後に角度を自動保存
        saveAngleToMemory();
      } else {
        Serial.println("エラー: 角度を指定してください（例: a90）");
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
        
        Serial.print("相対角度 ");
        Serial.print(value);
        Serial.print(" 度移動します（目標角度: ");
        Serial.print(targetDegrees);
        Serial.println(" 度）");
        
        // 移動後に角度を自動保存
        saveAngleToMemory();
      } else {
        Serial.println("エラー: 角度を指定してください（例: r45）");
      }
      break;
      
    case 'z':  // 現在位置を0度にリセット
      stepper.setCurrentPosition(0);
      currentAngle = 0;
      Serial.println("現在位置を0度にリセットしました");
      
      // リセット後に角度を自動保存
      saveAngleToMemory();
      break;
      
    case 's':  // 状態表示
      {
        float currentSteps = stepper.currentPosition();
        float currentDegrees = (currentSteps / STEPS_PER_DEGREE);
        
        // 角度の値を正規化（0〜360度の範囲内に）
        while (currentDegrees >= 360) currentDegrees -= 360;
        while (currentDegrees < 0) currentDegrees += 360;
        
        Serial.println("\n------ モーター状態 ------");
        Serial.print("現在位置: ");
        Serial.print(currentDegrees);
        Serial.println(" 度");
        Serial.print("目標位置までの角度: ");
        Serial.print(stepper.distanceToGo() / STEPS_PER_DEGREE);
        Serial.println(" 度");
        Serial.print("現在のステップ数: ");
        Serial.println(currentSteps);
        Serial.print("現在の速度: ");
        Serial.print(currentSpeed);
        Serial.println(" steps/sec");
        Serial.print("電源節約モード: ");
        Serial.println(powerSavingMode ? "有効" : "無効");
        Serial.println("------------------------");
      }
      break;
      
    case 'm':  // メモリに保存された値を表示
      {
        // メモリが有効かチェック
        long memoryFlag;
        EEPROM.get(MEMORY_VALID_FLAG_ADDR, memoryFlag);
        
        if (memoryFlag == MEMORY_VALID_FLAG) {
          float savedAngle;
          EEPROM.get(CURRENT_ANGLE_ADDR, savedAngle);
          
          // 角度の値を正規化（0〜360度の範囲内に）
          while (savedAngle >= 360) savedAngle -= 360;
          while (savedAngle < 0) savedAngle += 360;
          
          Serial.print("メモリに保存された角度: ");
          Serial.print(savedAngle);
          Serial.println(" 度");
        } else {
          Serial.println("メモリに保存された有効なデータがありません");
        }
      }
      break;
      
    case 'w':  // 現在の角度をメモリに保存
      saveAngleToMemory();
      break;
      
    case 'l':  // メモリから角度を読み込み
      loadAngleFromMemory();
      break;
      
    case 'c':  // メモリをクリア
      // メモリを無効化（フラグをクリア）
      EEPROM.put(MEMORY_VALID_FLAG_ADDR, (long)0);
      Serial.println("メモリをクリアしました");
      break;
      
    case 'p':  // 電源節約モードの切り替え
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
      
    case 'v':  // 速度プリセット
      if (valueStr.length() > 0) {
        int speedPreset = valueStr.toInt();
        switch (speedPreset) {
          case 1:  // 非常に遅い
            currentSpeed = 400.0;
            currentAcceleration = 200.0;
            Serial.println("速度: 非常に遅い（高トルク）");
            break;
          case 2:  // 遅い
            currentSpeed = 700.0;
            currentAcceleration = 350.0;
            Serial.println("速度: 遅い");
            break;
          case 3:  // 通常
            currentSpeed = 1000.0;
            currentAcceleration = 500.0;
            Serial.println("速度: 通常");
            break;
          case 4:  // 速い
            currentSpeed = 1300.0;
            currentAcceleration = 650.0;
            Serial.println("速度: 速い");
            break;
          case 5:  // 非常に速い
            currentSpeed = 1600.0;
            currentAcceleration = 800.0;
            Serial.println("速度: 非常に速い（低トルク）");
            break;
          default:
            Serial.println("エラー: 無効な速度プリセットです。1〜5の値を指定してください。");
            return;
        }
        stepper.setMaxSpeed(currentSpeed);
        stepper.setAcceleration(currentAcceleration);
      } else {
        Serial.println("エラー: 速度プリセットを指定してください（例: v3）");
      }
      break;
      
    case 'h':  // ヘルプ
      printCommands();
      break;
      
    default:
      Serial.println("エラー: 無効なコマンドです。'h'でヘルプを表示します。");
      break;
  }
}

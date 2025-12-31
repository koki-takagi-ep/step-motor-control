/*
  stepper_simple_angle.ino
  ───────────────
  28BYJ-48 ステッピングモーターを使った
  シンプルな角度制御プログラム
  
  作成者: Koki Takagi
  作成日: 2025年5月10日
*/

#include <Stepper.h>

// 28BYJ-48の1回転のステップ数
const int STEPS_PER_REVOLUTION = 2048;  // 半ステップモードでは約4096だが、標準ライブラリでは2048が適切

// ピン設定
const int IN1 = 8;
const int IN2 = 9;
const int IN3 = 10;
const int IN4 = 11;

// Stepperライブラリの初期化
// このモーターでは、IN1, IN3, IN2, IN4の順で配線する必要があることが多い
Stepper stepper(STEPS_PER_REVOLUTION, IN1, IN3, IN2, IN4);

// 回転速度 (RPM - 1分間の回転数)
const int MOTOR_SPEED = 10;  // 10 RPM

// 変数
int currentAngle = 0;  // 現在の角度
int targetAngle = 0;   // 目標角度

void setup() {
  // シリアル通信の初期化
  Serial.begin(9600);
  
  // モーターの回転速度を設定
  stepper.setSpeed(MOTOR_SPEED);
  
  // 使用方法の表示
  Serial.println("28BYJ-48 シンプル角度制御");
  Serial.println("0-360の数字を入力して目標角度を設定");
  Serial.println("0: ホームポジション");
  Serial.println("90: 90度に移動");
  Serial.println("180: 180度に移動");
  Serial.println("270: 270度に移動");
  Serial.println("360または0: 0度(ホーム)に移動");
  Serial.println("r: 現在の角度をリセット");
  Serial.println("p: 現在の角度を表示");
}

void loop() {
  // シリアル入力を確認
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();  // 前後の空白を削除
    
    // 数字が入力された場合は角度として処理
    if (isDigit(input.charAt(0))) {
      targetAngle = input.toInt();
      moveToAngle(targetAngle);
    }
    // 特別コマンドの処理
    else if (input == "r" || input == "reset") {
      resetPosition();
    }
    else if (input == "p" || input == "pos") {
      printPosition();
    }
    else {
      Serial.println("0-360の角度、または r (リセット)、p (位置表示) を入力してください");
    }
  }
}

// 指定した角度に移動する関数
void moveToAngle(int angle) {
  // 角度を 0-360 の範囲に正規化
  while (angle >= 360) angle -= 360;
  while (angle < 0) angle += 360;
  
  // 現在の角度と目標角度の差分を計算
  int angleDiff = angle - currentAngle;
  
  // 最短経路を選択 (もし angleDiff > 180 なら反対方向が短い)
  if (angleDiff > 180) angleDiff -= 360;
  else if (angleDiff < -180) angleDiff += 360;
  
  // 角度をステップ数に変換
  int steps = map(abs(angleDiff), 0, 360, 0, STEPS_PER_REVOLUTION);
  
  Serial.print("移動中: ");
  Serial.print(currentAngle);
  Serial.print("度 → ");
  Serial.print(angle);
  Serial.println("度");
  
  // ステッピングモーターを回転
  if (angleDiff > 0) {
    stepper.step(steps);  // 正の場合は時計回り
  } else {
    stepper.step(-steps); // 負の場合は反時計回り
  }
  
  // 現在の角度を更新
  currentAngle = angle;
  
  // 消費電力を抑えるためにピンをLOWに設定
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  
  Serial.print("移動完了: 現在の角度は ");
  Serial.print(currentAngle);
  Serial.println("度");
}

// 現在の位置をリセットする関数
void resetPosition() {
  currentAngle = 0;
  Serial.println("現在の位置を 0度 にリセットしました");
}

// 現在の位置を表示する関数
void printPosition() {
  Serial.print("現在の角度: ");
  Serial.print(currentAngle);
  Serial.println("度");
}

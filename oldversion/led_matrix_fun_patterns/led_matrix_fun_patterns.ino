/*
  Arduino R4 WiFi - 楽しいLEDマトリックスパターン
  2025年5月10日にKokiさん用に作成
  
  このスケッチは、Arduino UNO R4 WiFiボードの内蔵12x8 LEDマトリックスで
  様々な面白いパターンを表示するものです。パターンは自動的に切り替わります。
  
  特徴:
  - 滑らかな遷移を持つ5つの興味深いパターン
  - 最大の視覚効果のために完全なLEDマトリックスを活用
  - アニメーション、波の効果、パーティクルシステムなどを含む
  
  ハードウェア:
  - Arduino UNO R4 WiFi（内蔵12x8 LEDマトリックス付き）
*/

#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;

// LEDマトリックス用のフレームバッファ
uint8_t frame[8][12] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// パターンモード変数
int currentPattern = 0;
const int totalPatterns = 5; // パターンの総数
unsigned long lastPatternChange = 0;
const unsigned long patternDuration = 10000; // パターンごとに10秒

// パーティクルシステム変数（パターン3用）
const int MAX_PARTICLES = 8;
int particleX[MAX_PARTICLES];
int particleY[MAX_PARTICLES];
int particleVelocityX[MAX_PARTICLES];
int particleVelocityY[MAX_PARTICLES];
bool particleActive[MAX_PARTICLES];

// 波効果変数（パターン4用）
float wavePhase = 0.0;

// スパイラルアニメーション変数（パターン5用）
int spiralX = 6;
int spiralY = 4;
int spiralRadius = 0;
int spiralDirection = 1;

void setup() {
  Serial.begin(9600);
  delay(1500); // シリアル接続のための短い遅延

  // LEDマトリックスの初期化
  matrix.begin();
  
  // ウェルカムメッセージを表示
  Serial.println("Arduino R4 WiFi - 楽しいLEDマトリックスパターン");
  Serial.println("パターンは10秒ごとに自動的に切り替わります");
  
  // パターン3用のパーティクルを初期化
  initializeParticles();
}

void loop() {
  // パターンを変更する時間かどうかをチェック
  if (millis() - lastPatternChange > patternDuration) {
    currentPattern = (currentPattern + 1) % totalPatterns;
    lastPatternChange = millis();
    
    // パターン固有の変数をリセット
    clearFrame();
    
    // パターン3に切り替える場合、パーティクルを初期化
    if (currentPattern == 2) {
      initializeParticles();
    }
    
    // 現在のパターン情報を表示
    Serial.print("パターンを変更: ");
    Serial.println(currentPattern + 1);
  }
  
  // 現在のパターンを実行
  switch (currentPattern) {
    case 0:
      // パターン1: 拡大・縮小する円
      patternExpandingCircles();
      break;
    case 1:
      // パターン2: 雨の効果
      patternRain();
      break;
    case 2:
      // パターン3: 跳ねるパーティクル
      patternParticles();
      break;
    case 3:
      // パターン4: 波の効果
      patternWave();
      break;
    case 4:
      // パターン5: スパイラルアニメーション
      patternSpiral();
      break;
  }
  
  // LEDマトリックスを更新
  displayFrame();
  
  // アニメーション速度を制御するための小さな遅延
  delay(100);
}

// フレームバッファをクリアするユーティリティ関数
void clearFrame() {
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 12; x++) {
      frame[y][x] = 0;
    }
  }
}

// フレームをLEDマトリックスに表示するユーティリティ関数
void displayFrame() {
  matrix.renderBitmap(frame, 8, 12);
}

// パターン1: 拡大・縮小する円
void patternExpandingCircles() {
  clearFrame();
  
  // 時間に基づいて円の半径を計算
  static unsigned long lastUpdate = 0;
  static int radius = 0;
  static int radiusDirection = 1;
  
  if (millis() - lastUpdate > 150) {
    radius += radiusDirection;
    
    // 最小または最大半径に達したときに方向を変更
    if (radius >= 8 || radius <= 0) {
      radiusDirection *= -1;
    }
    
    lastUpdate = millis();
  }
  
  // マトリックスの中心に円を描画
  drawCircle(6, 4, radius);
}

// LEDマトリックスに円を描画
void drawCircle(int centerX, int centerY, int radius) {
  // シンプルな円描画アルゴリズム
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 12; x++) {
      // 中心からの距離を計算
      float distance = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));
      
      // 円の縁にあるピクセルを点灯
      if (distance >= radius - 0.5 && distance < radius + 0.5) {
        frame[y][x] = 1;
      }
    }
  }
}

// パターン2: 雨の効果
void patternRain() {
  // すべてを1ピクセル下に移動
  for (int y = 7; y > 0; y--) {
    for (int x = 0; x < 12; x++) {
      frame[y][x] = frame[y-1][x];
    }
  }
  
  // 最上行に新しい雨粒を生成
  for (int x = 0; x < 12; x++) {
    // 雨粒を作成するランダムな確率
    frame[0][x] = (random(100) < 15) ? 1 : 0;
  }
}

// パーティクルシステム用のパーティクルを初期化
void initializeParticles() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    particleX[i] = random(12);
    particleY[i] = random(8);
    particleVelocityX[i] = random(3) - 1; // -1, 0, または 1
    particleVelocityY[i] = random(3) - 1; // -1, 0, または 1
    
    // パーティクルが静止していないことを確認
    if (particleVelocityX[i] == 0 && particleVelocityY[i] == 0) {
      particleVelocityX[i] = 1;
    }
    
    particleActive[i] = true;
  }
}

// パターン3: 跳ねるパーティクル
void patternParticles() {
  clearFrame();
  
  // パーティクルの位置を更新
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particleActive[i]) {
      // 位置の更新
      particleX[i] += particleVelocityX[i];
      particleY[i] += particleVelocityY[i];
      
      // 壁からの跳ね返り
      if (particleX[i] < 0 || particleX[i] >= 12) {
        particleVelocityX[i] *= -1;
        particleX[i] += particleVelocityX[i];
      }
      
      if (particleY[i] < 0 || particleY[i] >= 8) {
        particleVelocityY[i] *= -1;
        particleY[i] += particleVelocityY[i];
      }
      
      // パーティクルを描画
      if (particleX[i] >= 0 && particleX[i] < 12 && 
          particleY[i] >= 0 && particleY[i] < 8) {
        frame[particleY[i]][particleX[i]] = 1;
      }
    }
  }
}

// パターン4: 波の効果
void patternWave() {
  clearFrame();
  
  // 波のフェーズを更新
  wavePhase += 0.2;
  
  // 波を描画
  for (int x = 0; x < 12; x++) {
    // サイン波を使用してY位置を計算
    int y = 4 + sin(wavePhase + x * 0.5) * 3;
    
    // Yが範囲内にあることを確認
    if (y >= 0 && y < 8) {
      frame[y][x] = 1;
    }
  }
}

// パターン5: スパイラルアニメーション
void patternSpiral() {
  static unsigned long lastUpdate = 0;
  static int angle = 0;
  
  if (millis() - lastUpdate > 100) {
    clearFrame();
    
    // 角度を更新
    angle = (angle + 15) % 360;
    
    // 時々半径を更新
    if (angle % 120 == 0) {
      spiralRadius = (spiralRadius + spiralDirection) % 8;
      if (spiralRadius <= 0 || spiralRadius >= 7) {
        spiralDirection *= -1;
      }
    }
    
    // スパイラルに沿って複数のポイントを描画
    for (int i = 0; i < 12; i++) {
      int pointAngle = (angle + i * 30) % 360;
      float radians = pointAngle * PI / 180.0;
      
      int x = spiralX + cos(radians) * spiralRadius;
      int y = spiralY + sin(radians) * (spiralRadius * 0.7); // LEDマトリックスのアスペクト比を調整
      
      // 範囲内の場合描画
      if (x >= 0 && x < 12 && y >= 0 && y < 8) {
        frame[y][x] = 1;
      }
    }
    
    lastUpdate = millis();
  }
}

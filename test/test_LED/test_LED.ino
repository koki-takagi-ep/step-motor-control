/*
  interactive_LED_patterns.ino
  ───────────────
  内蔵 LED (LED_BUILTIN = D13) で様々な点滅パターンを表示
  ボタンを押すとパターンが切り替わります
*/

// ボタンピンの設定
const int buttonPin = 2;      // ボタンを接続するD2ピン

// モールス信号の長さを定義
const int dotDuration = 200;      // ドット（短い点滅）の長さ
const int dashDuration = dotDuration * 3;  // ダッシュ（長い点滅）の長さ
const int elementSpace = dotDuration;     // 同じ文字内の要素間のスペース
const int letterSpace = dotDuration * 3;  // 文字間のスペース
const int wordSpace = dotDuration * 7;    // 単語間のスペース

// 心臓の鼓動を表現するための変数
const int heartbeatCount = 5;     // 心臓の鼓動回数
const int heartbeatShortPulse = 100;  // 短いパルス
const int heartbeatLongPulse = 350;   // 長いパルス
const int heartbeatPause = 150;       // パルス間の短い休止
const int heartbeatDelay = 2000;      // 一連の鼓動後の休止時間

// レインドロップ模様の設定
const int rainMaxDrops = 10;      // ドロップの最大数
const int rainMinDuration = 50;   // 最短のドロップ時間
const int rainMaxDuration = 150;  // 最長のドロップ時間
const int rainPause = 300;        // 雨の中断時間

// ボタン状態管理用変数
int buttonState = 0;          // 現在のボタン状態
int lastButtonState = 0;      // 前回のボタン状態
int patternMode = 0;          // 現在のパターンモード
const int totalPatterns = 4;   // 利用可能なパターンの数

// デバウンス用変数
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // デバウンス時間

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);  // D13 (内蔵LED) を出力に設定
  pinMode(buttonPin, INPUT_PULLUP); // D2を入力、内部プルアップ付き
  
  Serial.begin(9600);           // デバッグ用にシリアル通信を開始
  Serial.println("Interactive LED Patterns Started!");
  Serial.println("Press the button to change patterns");
  printCurrentMode();
  
  // 乱数生成の初期化
  randomSeed(analogRead(0));
}

void loop() {
  // ボタン状態をチェック
  checkButtonPress();
  
  // 現在のモードに応じて点滅パターンを実行
  switch (patternMode) {
    case 0:
      // モールス信号 SOS
      blinkSOS();
      break;
    case 1:
      // 心臓の鼓動
      blinkHeartbeat();
      break;
    case 2:
      // 雨のような点滅
      blinkRaindrop();
      break;
    case 3:
      // ストロボ点滅
      blinkStrobe();
      break;
  }
}

// ボタン押下をチェックする関数
void checkButtonPress() {
  // ボタン状態を読み取り
  int reading = digitalRead(buttonPin);
  
  // 前回の状態と異なる場合、デバウンスタイマーをリセット
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  // 一定時間経過後、状態が安定していれば有効なボタン押下と判断
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // 状態が変化した場合
    if (reading != buttonState) {
      buttonState = reading;
      
      // LOWはボタンが押された状態（プルアップ抵抗使用時）
      if (buttonState == LOW) {
        // パターンを次に進める
        patternMode = (patternMode + 1) % totalPatterns;
        printCurrentMode();
      }
    }
  }
  
  // 現在の状態を保存
  lastButtonState = reading;
}

// 現在のモードを表示
void printCurrentMode() {
  Serial.print("Current Pattern Mode: ");
  switch (patternMode) {
    case 0:
      Serial.println("SOS Morse Code");
      break;
    case 1:
      Serial.println("Heartbeat");
      break;
    case 2:
      Serial.println("Raindrop");
      break;
    case 3:
      Serial.println("Strobe");
      break;
  }
}

// SOSモールス信号パターン
void blinkSOS() {
  // S (...)
  for (int i = 0; i < 3; i++) {
    blinkDot();
  }
  delay(letterSpace);
  
  // O (---)
  for (int i = 0; i < 3; i++) {
    blinkDash();
  }
  delay(letterSpace);
  
  // S (...)
  for (int i = 0; i < 3; i++) {
    blinkDot();
  }
  
  // 単語間のスペース
  delay(wordSpace);
}

// 心臓の鼓動パターン
void blinkHeartbeat() {
  for (int i = 0; i < heartbeatCount; i++) {
    // 最初の短いパルス
    digitalWrite(LED_BUILTIN, HIGH);
    delay(heartbeatShortPulse);
    digitalWrite(LED_BUILTIN, LOW);
    delay(heartbeatPause);
    
    // 2つ目の長いパルス
    digitalWrite(LED_BUILTIN, HIGH);
    delay(heartbeatLongPulse);
    digitalWrite(LED_BUILTIN, LOW);
    delay(heartbeatPause * 4); // 次の鼓動までの間隔
  }
  
  // 次のパターンの前に少し待機
  delay(heartbeatDelay);
}

// 雨のドロップのようなランダムな点滅パターン
void blinkRaindrop() {
  int drops = random(3, rainMaxDrops);
  
  for (int i = 0; i < drops; i++) {
    // ランダムな長さの点滅
    int duration = random(rainMinDuration, rainMaxDuration);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(duration);
    digitalWrite(LED_BUILTIN, LOW);
    
    // ランダムな間隔
    delay(random(rainMinDuration, rainMaxDuration));
  }
  
  // 雨の一連のパターンの間の休止
  delay(rainPause);
}

// 高速なストロボパターン
void blinkStrobe() {
  // 高速なパルスを複数回
  for (int i = 0; i < 15; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(20);  // 非常に短いパルス
    digitalWrite(LED_BUILTIN, LOW);
    delay(20);  // 短い消灯時間
  }
  
  // 長い休止
  delay(1000);
}

// ドット（短い点滅）を表示する関数
void blinkDot() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(dotDuration);
  digitalWrite(LED_BUILTIN, LOW);
  delay(elementSpace);
}

// ダッシュ（長い点滅）を表示する関数
void blinkDash() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(dashDuration);
  digitalWrite(LED_BUILTIN, LOW);
  delay(elementSpace);
}
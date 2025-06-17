# ステッピングモーター角度制御 v2（BLE対応版）

Arduino UNO R4 WiFiの内蔵Bluetooth LEを使用したワイヤレス制御版です。

## 🎯 こんな方におすすめ

- ワイヤレスでモーターを制御したい
- スマートフォンから操作したい
- Arduino UNO R4 WiFiをお持ちの方
- IoTプロジェクトを作りたい
- GUIで視覚的に操作したい

## 🆕 v1からの追加機能

1. **Bluetooth LE通信** - ケーブル不要でワイヤレス制御
2. **デュアル通信** - USBとBLEの両方で同時制御可能
3. **Python GUIツール** - 視覚的な操作インターフェース
4. **スマホ対応** - iOSやAndroidから直接制御

## 📋 必要なもの

### ハードウェア
- **Arduino UNO R4 WiFi**（BLE内蔵必須）
- 28BYJ-48 ステッピングモーター
- ULN2003 ドライバーボード
- ジャンパーワイヤー 6本

### ソフトウェア
- Arduino IDE
- AccelStepperライブラリ
- ArduinoBLEライブラリ
- Python 3.x + bleak（PCツール使用時）

## 💻 セットアップ

### 1. Arduino側の準備

```bash
1. Arduino IDEでライブラリをインストール
   - AccelStepper
   - ArduinoBLE

2. ボード設定
   - ツール → ボード → Arduino UNO R4 WiFi

3. main.inoを書き込み
```

### 2. Python環境の準備（オプション）

```bash
# venv環境の作成（推奨）
python -m venv venv
source venv/bin/activate  # Mac/Linux
# または
venv\Scripts\activate     # Windows

# bleakのインストール
pip install bleak
```

## 📱 使い方

### スマートフォンから（最も簡単）

1. **BLEアプリをインストール**
   - iOS: LightBlue
   - Android: nRF Connect

2. **接続**
   - アプリで「StepperMotor」または「Arduino」を検索
   - 接続をタップ

3. **コマンド送信**
   - Write Characteristicを選択
   - テキストで`a90`などを送信

### PCから（GUIツール）

```bash
cd motor/v2
./run_monitor.sh

# メニューから選択
1) コマンドライン版
2) GUI版（おすすめ）
```

#### GUI版の機能
- 🎚️ スライダーで角度を視覚的に指定
- 🔘 ワンクリックで0°、90°、180°、270°へ移動
- 📊 リアルタイムでステータス表示
- 📝 送受信ログの確認

### シリアルモニターから（従来通り）

v1と同じようにUSB接続でも使用可能です。

## 🔧 BLE仕様

| 項目 | 値 |
|------|-----|
| デバイス名 | StepperMotor または Arduino |
| サービスUUID | 19B10000-E8F2-537E-4F6C-D104768A1214 |
| コマンド送信 | 19B10001-... (Write) |
| ステータス受信 | 19B10002-... (Read/Notify) |

## 📝 Pythonツールの詳細

### ble_monitor.py（CLIツール）
```python
# 対話型コマンドライン
コマンド> a90   # 90度へ移動
コマンド> s     # ステータス表示
コマンド> q     # 終了
```

### ble_gui.py（GUIツール）
- Tkinterベースの使いやすいインターフェース
- 接続状態の可視化
- エラーハンドリング

### ble_scanner.py（デバッグ用）
```bash
python ble_scanner.py
# 周辺のBLEデバイスをスキャン
```

## 🚨 トラブルシューティング

### BLE接続できない

1. **Bluetooth設定を確認**
   ```
   Mac: システム設定 → Bluetooth → ON
   Windows: 設定 → Bluetooth → ON
   ```

2. **デバイス名を確認**
   ```bash
   python ble_scanner.py
   # "Arduino"が表示されるか確認
   ```

3. **Arduino側の確認**
   - シリアルモニターでBLE初期化成功を確認
   - LEDの点滅パターンを確認

### Pythonツールのエラー

```bash
# bleakがない場合
pip install bleak

# 権限エラー（Mac/Linux）
sudo python ble_monitor.py

# Tkinterエラー（Linux）
sudo apt-get install python3-tk
```

### コマンドが効かない

- BLEアプリで改行（\n）を含めて送信
- 文字コードをUTF-8に設定

## 🔐 セキュリティ注意事項

- BLEはペアリング不要で接続可能
- 重要な制御には認証機能の追加を推奨
- 通信範囲は約10m（障害物なし）

## 🎓 応用例

1. **自動カーテン** - 時間で開閉
2. **カメラ雲台** - 360度パノラマ撮影
3. **自動給餌器** - 定量供給
4. **展示物の回転台** - ショーケース用
#!/bin/bash
# BLEモニターを実行するスクリプト

# venv環境をアクティベート
source /Users/koki/Desktop/venv/bin/activate

# スクリプトのディレクトリに移動
cd "$(dirname "$0")"

echo "BLEステッピングモーター制御ツールを起動します"
echo "1) コマンドライン版 (ble_monitor.py)"
echo "2) GUI版 (ble_gui.py)"
echo -n "選択してください (1 or 2): "
read choice

case $choice in
    1)
        echo "コマンドライン版を起動します..."
        python ble_monitor.py
        ;;
    2)
        echo "GUI版を起動します..."
        python ble_gui.py
        ;;
    *)
        echo "無効な選択です"
        exit 1
        ;;
esac
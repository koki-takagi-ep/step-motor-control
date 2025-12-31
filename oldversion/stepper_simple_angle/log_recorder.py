#!/usr/bin/env python3
"""
log_recorder.py
--------------
Arduino から送信されるステッパーモーターの角度設定ログを
シリアル通信で受信し、CSVファイルとして保存するスクリプト

使用方法:
python log_recorder.py [シリアルポート] [ボーレート]

例:
python log_recorder.py /dev/ttyACM0 9600
python log_recorder.py COM3 9600
"""

import serial
import time
import sys
import os
from datetime import datetime

def main():
    # コマンドライン引数の処理
    if len(sys.argv) < 2:
        print("使用方法: python log_recorder.py [シリアルポート] [ボーレート]")
        print("例: python log_recorder.py /dev/ttyACM0 9600")
        print("    python log_recorder.py COM3 9600")
        
        # 引数がない場合はデフォルト値を使用
        port = '/dev/ttyACM0'  # macOS/Linux用デフォルト
        if sys.platform.startswith('win'):
            port = 'COM3'      # Windows用デフォルト
        baudrate = 9600
    else:
        port = sys.argv[1]
        baudrate = int(sys.argv[2]) if len(sys.argv) > 2 else 9600
    
    # ログファイル名の設定
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_filename = f"angle_log_{timestamp}.csv"
    
    print(f"Arduino シリアルログレコーダー")
    print(f"--------------------------------")
    print(f"ポート: {port}")
    print(f"ボーレート: {baudrate}")
    print(f"ログファイル: {log_filename}")
    print(f"Ctrl+C で終了")
    print(f"--------------------------------")
    
    try:
        # シリアルポートを開く
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"シリアルポート {port} に接続しました")
        
        # ヘッダー行があるかどうかのフラグ
        header_written = False
        
        # ログファイルを開く
        with open(log_filename, 'w') as log_file:
            try:
                while True:
                    # シリアルからデータを読み込む
                    line = ser.readline().decode('utf-8').strip()
                    
                    if line:
                        # 通常のメッセージはそのまま表示
                        if not line.startswith("LOG:"):
                            print(f"Arduino: {line}")
                        else:
                            # LOGプレフィックスを取り除く
                            log_data = line[4:]
                            
                            # CSVヘッダーの場合
                            if log_data.startswith("timestamp"):
                                if not header_written:
                                    log_file.write(log_data + "\n")
                                    log_file.flush()
                                    header_written = True
                                    print(f"CSVヘッダーを書き込みました: {log_data}")
                            else:
                                # CSVデータの場合
                                if not header_written:
                                    # ヘッダーがまだ書き込まれていない場合はデフォルトのヘッダーを書き込む
                                    log_file.write("timestamp,angle\n")
                                    header_written = True
                                
                                # データをファイルに書き込む
                                log_file.write(log_data + "\n")
                                log_file.flush()
                                print(f"ログを記録: {log_data}")
                    
                    # 少し待機
                    time.sleep(0.01)
                
            except KeyboardInterrupt:
                print("\nログ記録を終了します")
    
    except serial.SerialException as e:
        print(f"エラー: シリアルポートに接続できません: {e}")
        return
    
    finally:
        # シリアルポートを閉じる
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print(f"シリアルポート {port} を閉じました")
        
        print(f"ログファイル '{log_filename}' に記録しました")

if __name__ == "__main__":
    main()

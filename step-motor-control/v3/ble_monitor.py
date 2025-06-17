#!/usr/bin/env python3
"""
BLE Stepper Motor Monitor (Refactored)
Arduino R4 WiFiのステッピングモーター制御をPCから操作するCLIツール

必要なライブラリ:
pip install bleak asyncio
"""

import asyncio
import sys
import signal
import logging
from typing import Optional
from datetime import datetime

# 共通ライブラリをインポート
from ble_common import BLEStepperClient, MotorCommands, BLEConnectionError, format_angle

# ログ設定
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class StepperMotorCLI:
    """ステッピングモーターのCLI制御クラス"""
    
    def __init__(self):
        self.ble_client: Optional[BLEStepperClient] = None
        self.running = True
        self.current_angle = 0.0
        
        # シグナルハンドラー設定
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
    
    def _signal_handler(self, signum, frame):
        """シグナルハンドラー"""
        print("\n\n終了シグナルを受信しました...")
        self.running = False
    
    def _status_handler(self, message: str):
        """ステータス受信ハンドラー"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        print(f"[{timestamp}] 受信: {message}")
        
        # 現在角度の更新
        from ble_common import extract_angle_from_message
        angle = extract_angle_from_message(message)
        if angle is not None:
            self.current_angle = angle
    
    async def connect(self) -> bool:
        """デバイスに接続"""
        try:
            print("=== BLEステッピングモーター制御 ===")
            print("デバイスを検索しています...")
            
            self.ble_client = BLEStepperClient(on_status_received=self._status_handler)
            await self.ble_client.connect()
            
            print("✓ 接続成功！")
            return True
            
        except BLEConnectionError as e:
            print(f"✗ 接続エラー: {e}")
            return False
        except Exception as e:
            print(f"✗ 予期しないエラー: {e}")
            return False
    
    async def disconnect(self):
        """切断"""
        if self.ble_client:
            await self.ble_client.disconnect()
            print("接続を切断しました")
    
    def print_help(self):
        """ヘルプを表示"""
        help_text = """
=== コマンド一覧 ===
基本操作:
  a[角度]    - 絶対角度移動 (例: a90, a180)
  r[角度]    - 相対角度移動 (例: r45, r-30)
  z          - 位置リセット (現在位置を0度に設定)

状態確認:
  s          - 詳細ステータス表示
  angle      - 現在角度のみ表示

設定:
  p          - 電源節約モード切替
  v[1-5]     - 速度設定 (1:最遅 ～ 5:最速)
  v          - 利用可能な速度一覧

その他:
  help, h    - このヘルプを表示
  quit, q    - 終了
  clear      - 画面クリア

プリセット角度:
  0, 90, 180, 270  - よく使う角度への移動

使用例:
  a90        # 90度の位置へ移動
  r45        # 現在位置から45度回転
  r-30       # 現在位置から-30度（反時計回り）
  v3         # 速度を通常レベルに設定
==================
"""
        print(help_text)
    
    def print_status_summary(self):
        """ステータス概要を表示"""
        if self.ble_client and self.ble_client.is_connected:
            print(f"状態: 接続中 | 現在角度: {format_angle(self.current_angle)}")
        else:
            print("状態: 切断中")
    
    async def process_command(self, command: str) -> bool:
        """コマンドを処理"""
        if not command:
            return True
        
        command = command.strip().lower()
        
        # 終了コマンド
        if command in ['quit', 'q', 'exit']:
            return False
        
        # ヘルプコマンド
        if command in ['help', 'h']:
            self.print_help()
            return True
        
        # 画面クリア
        if command == 'clear':
            print('\033[2J\033[H')  # ANSI escape codes
            return True
        
        # 角度表示
        if command == 'angle':
            print(f"現在角度: {format_angle(self.current_angle)}")
            return True
        
        # 接続チェック
        if not self.ble_client or not self.ble_client.is_connected:
            print("✗ デバイスが接続されていません")
            return True
        
        # プリセット角度
        if command in ['0', '90', '180', '270']:
            angle = float(command)
            motor_cmd = MotorCommands.absolute_move(angle)
            success = await self.ble_client.send_command(motor_cmd)
            if success:
                print(f"→ プリセット移動: {format_angle(angle)}")
            return True
        
        # モーターコマンド
        try:
            # 基本的なコマンド変換
            if command.startswith('a'):
                # 絶対角度
                angle_str = command[1:]
                if angle_str:
                    angle = float(angle_str)
                    motor_cmd = MotorCommands.absolute_move(angle)
                    success = await self.ble_client.send_command(motor_cmd)
                    if success:
                        print(f"→ 絶対移動: {format_angle(angle)}")
                else:
                    print("✗ 角度を指定してください (例: a90)")
            
            elif command.startswith('r'):
                # 相対角度
                angle_str = command[1:]
                if angle_str:
                    angle = float(angle_str)
                    motor_cmd = MotorCommands.relative_move(angle)
                    success = await self.ble_client.send_command(motor_cmd)
                    if success:
                        direction = "時計回り" if angle >= 0 else "反時計回り"
                        print(f"→ 相対移動: {abs(angle):.1f}° ({direction})")
                else:
                    print("✗ 角度を指定してください (例: r45)")
            
            elif command == 'z':
                # リセット
                motor_cmd = MotorCommands.reset_position()
                success = await self.ble_client.send_command(motor_cmd)
                if success:
                    self.current_angle = 0.0
                    print("→ 位置をリセットしました")
            
            elif command == 's':
                # ステータス
                motor_cmd = MotorCommands.get_status()
                success = await self.ble_client.send_command(motor_cmd)
                if success:
                    print("→ ステータスを要求しました")
            
            elif command == 'p':
                # 電源節約
                motor_cmd = MotorCommands.toggle_power_saving()
                success = await self.ble_client.send_command(motor_cmd)
                if success:
                    print("→ 電源節約モードを切替ました")
            
            elif command.startswith('v'):
                # 速度設定
                speed_str = command[1:]
                if speed_str:
                    try:
                        speed = int(speed_str)
                        motor_cmd = MotorCommands.set_speed(speed)
                        success = await self.ble_client.send_command(motor_cmd)
                        if success:
                            print(f"→ 速度をレベル{speed}に設定しました")
                    except ValueError:
                        print("✗ 速度は1-5の数値で指定してください")
                else:
                    # 速度一覧表示
                    motor_cmd = MotorCommands.get_help()  # v コマンドでヘルプ表示
                    await self.ble_client.send_command("v")
            
            else:
                # 不明なコマンド
                print(f"✗ 不明なコマンド: {command}")
                print("'help' でコマンド一覧を表示します")
        
        except ValueError as e:
            print(f"✗ 入力エラー: {e}")
        except Exception as e:
            print(f"✗ コマンド処理エラー: {e}")
        
        return True
    
    async def interactive_mode(self):
        """対話モード"""
        print("\n=== 対話モード開始 ===")
        print("'help' でコマンド一覧、'q' で終了")
        
        # 初期ステータス取得
        if self.ble_client and self.ble_client.is_connected:
            await self.ble_client.send_command(MotorCommands.get_status())
        
        while self.running:
            try:
                self.print_status_summary()
                command = input("\nコマンド> ").strip()
                
                if not await self.process_command(command):
                    break
                
                # 短い待機（レスポンス受信のため）
                await asyncio.sleep(0.1)
                
            except KeyboardInterrupt:
                print("\n\n終了します...")
                break
            except EOFError:
                print("\n\n入力終了")
                break
            except Exception as e:
                print(f"✗ エラー: {e}")
        
        print("対話モードを終了します")
    
    async def run(self):
        """メイン実行"""
        try:
            # 接続
            if not await self.connect():
                print("接続に失敗しました")
                return 1
            
            # 対話モード
            await self.interactive_mode()
            
        except Exception as e:
            print(f"実行エラー: {e}")
            return 1
        
        finally:
            # 切断
            await self.disconnect()
        
        return 0

async def main():
    """メイン関数"""
    try:
        cli = StepperMotorCLI()
        exit_code = await cli.run()
        sys.exit(exit_code)
    
    except KeyboardInterrupt:
        print("\n\n中断されました")
        sys.exit(1)
    except Exception as e:
        print(f"致命的エラー: {e}")
        sys.exit(1)

if __name__ == "__main__":
    # Python 3.7+ 対応
    try:
        asyncio.run(main())
    except AttributeError:
        # Python 3.6 対応
        loop = asyncio.get_event_loop()
        try:
            loop.run_until_complete(main())
        finally:
            loop.close()
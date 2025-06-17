#!/usr/bin/env python3
"""
BLE Stepper Motor Monitor
Arduino R4 WiFiのステッピングモーター制御をPCから操作するツール

必要なライブラリ:
pip install bleak asyncio
"""

import asyncio
from bleak import BleakClient, BleakScanner
import sys

# BLE UUIDs
SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214"
COMMAND_UUID = "19B10001-E8F2-537E-4F6C-D104768A1214"
STATUS_UUID = "19B10002-E8F2-537E-4F6C-D104768A1214"

class StepperMotorBLE:
    def __init__(self):
        self.client = None
        self.device = None
        
    async def find_device(self):
        """StepperMotorデバイスを検索"""
        print("BLEデバイスを検索中...")
        devices = await BleakScanner.discover()
        
        for device in devices:
            if device.name == "StepperMotor" or device.name == "Arduino":
                self.device = device
                print(f"デバイスを発見: {device.name} ({device.address})")
                return True
                
        print("StepperMotorデバイスが見つかりません")
        return False
        
    async def connect(self):
        """デバイスに接続"""
        if not self.device:
            return False
            
        try:
            self.client = BleakClient(self.device.address)
            await self.client.connect()
            print("接続成功！")
            
            # 通知を有効化
            await self.client.start_notify(STATUS_UUID, self.notification_handler)
            return True
            
        except Exception as e:
            print(f"接続エラー: {e}")
            return False
            
    def notification_handler(self, sender, data):
        """ステータス通知を処理"""
        status = data.decode('utf-8')
        print(f"ステータス: {status}")
        
    async def send_command(self, command):
        """コマンドを送信"""
        if not self.client or not self.client.is_connected:
            print("デバイスが接続されていません")
            return
            
        try:
            await self.client.write_gatt_char(COMMAND_UUID, command.encode('utf-8'))
            print(f"送信: {command}")
            
        except Exception as e:
            print(f"送信エラー: {e}")
            
    async def interactive_mode(self):
        """対話モード"""
        print("\n=== ステッピングモーター制御 ===")
        print("コマンド:")
        print("  a[角度] - 絶対角度 (例: a90)")
        print("  r[角度] - 相対角度 (例: r45)")
        print("  z - ゼロリセット")
        print("  s - ステータス表示")
        print("  p - 電源節約切替")
        print("  v[1-5] - 速度設定")
        print("  h - ヘルプ")
        print("  q - 終了")
        print("========================\n")
        
        while True:
            try:
                command = input("コマンド> ").strip()
                
                if command.lower() == 'q':
                    break
                    
                if command:
                    await self.send_command(command)
                    await asyncio.sleep(0.1)  # レスポンス待機
                    
            except KeyboardInterrupt:
                break
                
    async def disconnect(self):
        """切断"""
        if self.client:
            await self.client.disconnect()
            print("\n切断しました")

async def main():
    motor = StepperMotorBLE()
    
    # デバイス検索
    if not await motor.find_device():
        return
        
    # 接続
    if not await motor.connect():
        return
        
    try:
        # 対話モード
        await motor.interactive_mode()
        
    finally:
        # 切断
        await motor.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
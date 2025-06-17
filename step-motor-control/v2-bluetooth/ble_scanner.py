#!/usr/bin/env python3
"""
BLEデバイススキャナー
利用可能なすべてのBLEデバイスを表示
"""

import asyncio
from bleak import BleakScanner

async def scan_devices():
    print("BLEデバイスをスキャン中... (10秒間)")
    print("-" * 50)
    
    devices = await BleakScanner.discover(timeout=10.0)
    
    if not devices:
        print("BLEデバイスが見つかりませんでした")
        print("\n確認事項:")
        print("1. BluetoothがONになっているか")
        print("2. Arduinoに電源が入っているか")
        print("3. Arduinoのプログラムが正常に動作しているか")
        return
    
    print(f"\n{len(devices)}個のデバイスが見つかりました:\n")
    
    for i, device in enumerate(devices, 1):
        print(f"{i}. 名前: {device.name or 'Unknown'}")
        print(f"   アドレス: {device.address}")
        print(f"   RSSI: {device.rssi} dBm")
        print("-" * 30)
        
        if device.name == "StepperMotor":
            print("   *** StepperMotorが見つかりました! ***")
            print("-" * 30)

if __name__ == "__main__":
    asyncio.run(scan_devices())
#!/usr/bin/env python3
"""
BLE Device Scanner (Refactored)
利用可能なすべてのBLEデバイスを表示し、ステッピングモーターを検索

必要なライブラリ:
pip install bleak
"""

import asyncio
import sys
import argparse
from datetime import datetime
from typing import List, Dict

# 共通ライブラリをインポート
from ble_common import BLEStepperClient, BLEConfig

class BLEDeviceScanner:
    """高機能BLEデバイススキャナー"""
    
    def __init__(self, scan_timeout: float = 10.0):
        self.scan_timeout = scan_timeout
        self.target_devices = []
        self.other_devices = []
    
    async def scan(self, show_all: bool = True, target_only: bool = False) -> List[Dict]:
        """デバイススキャンを実行"""
        print(f"BLEデバイスをスキャン中... ({self.scan_timeout:.1f}秒間)")
        print("=" * 60)
        
        try:
            # 共通ライブラリを使用してスキャン
            client = BLEStepperClient()
            devices = await client.scan_devices()
            
            if not devices:
                self._print_no_devices_found()
                return []
            
            # デバイスを分類
            self._categorize_devices(devices)
            
            # 結果表示
            if target_only:
                self._print_target_devices_only()
            elif show_all:
                self._print_all_devices()
            else:
                self._print_summary()
            
            return devices
            
        except Exception as e:
            print(f"スキャンエラー: {e}")
            return []
    
    def _categorize_devices(self, devices: List[Dict]):
        """デバイスを分類"""
        self.target_devices = [d for d in devices if d['is_target']]
        self.other_devices = [d for d in devices if not d['is_target']]
    
    def _print_no_devices_found(self):
        """デバイスが見つからない場合の表示"""
        print("⚠️  BLEデバイスが見つかりませんでした")
        print("\n🔍 トラブルシューティング:")
        print("  1. 📶 BluetoothがONになっているか確認")
        print("  2. ⚡ Arduinoに電源が入っているか確認")
        print("  3. 💻 Arduinoのプログラムが正常動作しているか確認")
        print("  4. 📍 Arduinoがアドバタイジングモードになっているか確認")
        print("  5. 📷 距離が近いか確認（推奨: 3m以内）")
    
    def _print_target_devices_only(self):
        """対象デバイスのみ表示"""
        if self.target_devices:
            print(f"🎯 対象デバイス ({len(self.target_devices)}個):")
            print("=" * 40)
            for i, device in enumerate(self.target_devices, 1):
                self._print_device_detail(device, i, highlight=True)
        else:
            print("⚠️  対象デバイスが見つかりません")
            print(f"     検索対象: {', '.join(BLEConfig.DEVICE_NAMES)}")
    
    def _print_all_devices(self):
        """全デバイスを表示"""
        total = len(self.target_devices) + len(self.other_devices)
        print(f"📱 発見したデバイス: {total}個")
        
        # 対象デバイス
        if self.target_devices:
            print(f"\n🎯 対象デバイス ({len(self.target_devices)}個):")
            print("-" * 50)
            for i, device in enumerate(self.target_devices, 1):
                self._print_device_detail(device, i, highlight=True)
        
        # その他のデバイス
        if self.other_devices:
            print(f"\n📶 その他のデバイス ({len(self.other_devices)}個):")
            print("-" * 50)
            for i, device in enumerate(self.other_devices, 1):
                self._print_device_summary(device, i)
    
    def _print_summary(self):
        """概要表示"""
        total = len(self.target_devices) + len(self.other_devices)
        print(f"📊 スキャン結果概要:")
        print(f"  総デバイス数: {total}個")
        print(f"  対象デバイス: {len(self.target_devices)}個")
        print(f"  その他: {len(self.other_devices)}個")
        
        if self.target_devices:
            print("\n🎯 対象デバイス:")
            for device in self.target_devices:
                rssi_bar = self._get_signal_strength_bar(device['rssi'])
                print(f"  ✅ {device['name']} ({device['address']}) {rssi_bar}")
    
    def _print_device_detail(self, device: Dict, index: int, highlight: bool = False):
        """デバイス詳細情報を表示"""
        prefix = "✅" if highlight else "📱"
        rssi_bar = self._get_signal_strength_bar(device['rssi'])
        signal_quality = self._get_signal_quality(device['rssi'])
        
        print(f"{prefix} {index}. {device['name']}")
        print(f"     アドレス: {device['address']}")
        print(f"     電波強度: {device['rssi']} dBm {rssi_bar} ({signal_quality})")
        
        if highlight:
            print(f"     🎉 接続可能なステッピングモーター!")
        
        print()
    
    def _print_device_summary(self, device: Dict, index: int):
        """デバイス概要情報を表示"""
        rssi_bar = self._get_signal_strength_bar(device['rssi'])
        print(f"  {index}. {device['name']} - {device['rssi']} dBm {rssi_bar}")
    
    def _get_signal_strength_bar(self, rssi: int) -> str:
        """電波強度をバーで表示"""
        if rssi >= -50:
            return "🟢🟢🟢🟢🟢"  # 非常に強い
        elif rssi >= -60:
            return "🟢🟢🟢🟢⚪"  # 強い
        elif rssi >= -70:
            return "🟡🟡🟡⚪⚪"  # 中程度
        elif rssi >= -80:
            return "🟠🟠⚪⚪⚪"  # 弱い
        else:
            return "🔴⚪⚪⚪⚪"  # 非常に弱い
    
    def _get_signal_quality(self, rssi: int) -> str:
        """電波品質を表示"""
        if rssi >= -50:
            return "優秀"
        elif rssi >= -60:
            return "良好"
        elif rssi >= -70:
            return "普通"
        elif rssi >= -80:
            return "弱い"
        else:
            return "非常に弱い"

async def main():
    """メイン関数"""
    parser = argparse.ArgumentParser(
        description="BLEデバイススキャナー - ステッピングモーターを検索",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用例:
  python ble_scanner.py                    # 全デバイスを表示
  python ble_scanner.py --target-only      # 対象デバイスのみ表示
  python ble_scanner.py --summary          # 概要のみ表示
  python ble_scanner.py --timeout 15       # 15秒間スキャン
        """
    )
    
    parser.add_argument(
        '--target-only', '-t',
        action='store_true',
        help='対象デバイス（StepperMotor/Arduino）のみ表示'
    )
    
    parser.add_argument(
        '--summary', '-s',
        action='store_true',
        help='概要のみ表示'
    )
    
    parser.add_argument(
        '--timeout',
        type=float,
        default=10.0,
        help='スキャンタイムアウト秒数 (デフォルト: 10.0)'
    )
    
    parser.add_argument(
        '--continuous', '-c',
        action='store_true',
        help='連続スキャンモード (Ctrl+Cで停止)'
    )
    
    args = parser.parse_args()
    
    scanner = BLEDeviceScanner(scan_timeout=args.timeout)
    
    try:
        if args.continuous:
            print("🔄 連続スキャンモード (Ctrl+Cで停止)\n")
            scan_count = 1
            
            while True:
                print(f"\n🔍 スキャン #{scan_count} - {datetime.now().strftime('%H:%M:%S')}")
                
                devices = await scanner.scan(
                    show_all=not args.summary,
                    target_only=args.target_only
                )
                
                if scanner.target_devices:
                    print(f"\n✅ 対象デバイスを {len(scanner.target_devices)} 個発見しました!")
                
                scan_count += 1
                print(f"\n⏳ 次のスキャンまで 5 秒待機...")
                await asyncio.sleep(5)
        
        else:
            devices = await scanner.scan(
                show_all=not args.summary,
                target_only=args.target_only
            )
            
            # 結果概要
            if devices:
                target_count = len(scanner.target_devices)
                if target_count > 0:
                    print(f"\n✅ スキャン完了: 対象デバイスを {target_count} 個発見しました!")
                    print("🚀 BLEモニターやGUIで接続できます")
                else:
                    print(f"\n⚠️  スキャン完了: 対象デバイスが見つかりませんでした")
            
            return 0 if scanner.target_devices else 1
    
    except KeyboardInterrupt:
        print("\n\n⏹️  スキャンを中断しました")
        return 1
    
    except Exception as e:
        print(f"\n⚠️  エラーが発生しました: {e}")
        return 1

if __name__ == "__main__":
    try:
        exit_code = asyncio.run(main())
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n中断されました")
        sys.exit(1)
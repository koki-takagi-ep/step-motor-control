#!/usr/bin/env python3
"""
BLE Stepper Motor Common Library
共通のBLE制御機能とユーティリティ
"""

import asyncio
import logging
from typing import Optional, Callable, List
from bleak import BleakClient, BleakScanner
from datetime import datetime

# BLE設定
class BLEConfig:
    SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214"
    COMMAND_UUID = "19B10001-E8F2-537E-4F6C-D104768A1214"
    STATUS_UUID = "19B10002-E8F2-537E-4F6C-D104768A1214"
    
    DEVICE_NAMES = ["StepperMotor", "Arduino"]
    SCAN_TIMEOUT = 10.0
    CONNECTION_TIMEOUT = 10.0
    
    # メッセージサイズ制限
    MAX_MESSAGE_SIZE = 20

# ログ設定
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class BLEConnectionError(Exception):
    """BLE接続関連のエラー"""
    pass

class BLEStepperClient:
    """BLE経由でステッピングモーターを制御するクライアント"""
    
    def __init__(self, on_status_received: Optional[Callable[[str], None]] = None):
        self.client: Optional[BleakClient] = None
        self.device = None
        self.is_connected = False
        self.message_buffer = ""
        self.on_status_received = on_status_received or self._default_status_handler
        
    async def scan_devices(self) -> List[dict]:
        """利用可能なBLEデバイスをスキャン"""
        logger.info(f"BLEデバイスをスキャン中... ({BLEConfig.SCAN_TIMEOUT}秒)")
        
        try:
            devices = await BleakScanner.discover(timeout=BLEConfig.SCAN_TIMEOUT)
            device_list = []
            
            for device in devices:
                device_info = {
                    'name': device.name or 'Unknown',
                    'address': device.address,
                    'rssi': device.rssi,
                    'is_target': device.name in BLEConfig.DEVICE_NAMES
                }
                device_list.append(device_info)
                
            logger.info(f"{len(device_list)}個のデバイスを発見")
            return device_list
            
        except Exception as e:
            logger.error(f"デバイススキャンエラー: {e}")
            return []
    
    async def find_target_device(self):
        """対象デバイスを検索"""
        logger.info("対象デバイスを検索中...")
        devices = await BleakScanner.discover(timeout=BLEConfig.SCAN_TIMEOUT)
        
        for device in devices:
            if device.name in BLEConfig.DEVICE_NAMES:
                self.device = device
                logger.info(f"対象デバイスを発見: {device.name} ({device.address})")
                return True
                
        logger.warning("対象デバイスが見つかりません")
        return False
    
    async def connect(self) -> bool:
        """デバイスに接続"""
        if not self.device:
            if not await self.find_target_device():
                raise BLEConnectionError("接続対象のデバイスが見つかりません")
        
        try:
            logger.info(f"接続中: {self.device.name} ({self.device.address})")
            self.client = BleakClient(self.device.address)
            
            await asyncio.wait_for(
                self.client.connect(),
                timeout=BLEConfig.CONNECTION_TIMEOUT
            )
            
            # 通知を有効化
            await self.client.start_notify(BLEConfig.STATUS_UUID, self._notification_handler)
            
            self.is_connected = True
            logger.info("接続成功")
            return True
            
        except asyncio.TimeoutError:
            logger.error("接続タイムアウト")
            raise BLEConnectionError("接続タイムアウト")
        except Exception as e:
            logger.error(f"接続エラー: {e}")
            raise BLEConnectionError(f"接続に失敗: {e}")
    
    async def disconnect(self):
        """切断"""
        if self.client and self.is_connected:
            try:
                await self.client.disconnect()
                logger.info("切断完了")
            except Exception as e:
                logger.error(f"切断エラー: {e}")
            finally:
                self.is_connected = False
                self.client = None
    
    async def send_command(self, command: str) -> bool:
        """コマンドを送信"""
        if not self.is_connected or not self.client:
            logger.error("デバイスが接続されていません")
            return False
        
        try:
            # コマンドのバリデーション
            if not self._validate_command(command):
                logger.error(f"無効なコマンド: {command}")
                return False
            
            await self.client.write_gatt_char(
                BLEConfig.COMMAND_UUID, 
                command.encode('utf-8')
            )
            logger.debug(f"コマンド送信: {command}")
            return True
            
        except Exception as e:
            logger.error(f"コマンド送信エラー: {e}")
            return False
    
    def _validate_command(self, command: str) -> bool:
        """コマンドの基本バリデーション"""
        if not command or len(command.strip()) == 0:
            return False
        
        # 基本的なコマンド形式チェック
        valid_commands = ['a', 'r', 'z', 's', 'p', 'v', 'h']
        first_char = command.lower().strip()[0]
        return first_char in valid_commands
    
    def _notification_handler(self, sender, data):
        """通知ハンドラー"""
        try:
            # データをデコード（エラーハンドリング付き）
            try:
                decoded_data = data.decode('utf-8')
            except UnicodeDecodeError:
                decoded_data = data.decode('utf-8', errors='ignore')
                logger.warning("文字エンコーディングエラーを検出")
            
            self.message_buffer += decoded_data
            
            # 完全なメッセージを処理
            while '\n' in self.message_buffer:
                line_end = self.message_buffer.index('\n')
                message = self.message_buffer[:line_end].strip()
                self.message_buffer = self.message_buffer[line_end + 1:]
                
                if message:  # 空行でない場合
                    self.on_status_received(message)
                    
        except Exception as e:
            logger.error(f"通知処理エラー: {e}")
            self.message_buffer = ""  # バッファをクリア
    
    def _default_status_handler(self, message: str):
        """デフォルトのステータスハンドラー"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        print(f"[{timestamp}] 受信: {message}")

# ユーティリティ関数
class MotorCommands:
    """モーターコマンドのヘルパークラス"""
    
    @staticmethod
    def absolute_move(angle: float) -> str:
        """絶対角度移動コマンド"""
        return f"a{angle}"
    
    @staticmethod
    def relative_move(angle: float) -> str:
        """相対角度移動コマンド"""
        return f"r{angle}"
    
    @staticmethod
    def reset_position() -> str:
        """位置リセットコマンド"""
        return "z"
    
    @staticmethod
    def get_status() -> str:
        """ステータス取得コマンド"""
        return "s"
    
    @staticmethod
    def toggle_power_saving() -> str:
        """電源節約切替コマンド"""
        return "p"
    
    @staticmethod
    def set_speed(level: int) -> str:
        """速度設定コマンド"""
        if 1 <= level <= 5:
            return f"v{level}"
        raise ValueError("速度レベルは1-5の範囲で指定してください")
    
    @staticmethod
    def get_help() -> str:
        """ヘルプコマンド"""
        return "h"

def format_angle(angle: float) -> str:
    """角度をフォーマット"""
    return f"{angle:.1f}°"

def extract_angle_from_message(message: str) -> Optional[float]:
    """メッセージから角度情報を抽出"""
    try:
        # "deg"または"°"を含むメッセージから角度を抽出
        for separator in ["deg", "°"]:
            if separator in message:
                angle_str = message.split(separator)[0].split(":")[-1].strip()
                return float(angle_str)
        return None
    except (ValueError, IndexError):
        return None

async def quick_scan() -> List[dict]:
    """クイックスキャン（単体使用）"""
    client = BLEStepperClient()
    return await client.scan_devices()

if __name__ == "__main__":
    # テスト用
    async def test_scan():
        devices = await quick_scan()
        print(f"\n{len(devices)}個のデバイスが見つかりました:")
        for i, device in enumerate(devices, 1):
            status = " [対象デバイス]" if device['is_target'] else ""
            print(f"{i}. {device['name']} ({device['address']}) RSSI: {device['rssi']}dBm{status}")
    
    asyncio.run(test_scan())
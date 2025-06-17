#!/usr/bin/env python3
"""
BLE Stepper Motor GUI
Arduino R4 WiFiのステッピングモーター制御用GUI

必要なライブラリ:
pip install bleak asyncio tkinter
"""

import asyncio
import tkinter as tk
from tkinter import ttk, scrolledtext
import threading
from bleak import BleakClient, BleakScanner
from datetime import datetime

# BLE UUIDs
SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214"
COMMAND_UUID = "19B10001-E8F2-537E-4F6C-D104768A1214"
STATUS_UUID = "19B10002-E8F2-537E-4F6C-D104768A1214"

class StepperMotorGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("ステッピングモーター BLE制御")
        self.root.geometry("600x500")
        
        self.client = None
        self.device = None
        self.loop = None
        self.current_angle = 0.0
        self.message_buffer = ""  # メッセージバッファを追加
        
        self.setup_gui()
        
    def setup_gui(self):
        """GUIのセットアップ"""
        # 接続フレーム
        conn_frame = ttk.LabelFrame(self.root, text="接続", padding=10)
        conn_frame.pack(fill="x", padx=10, pady=5)
        
        self.conn_button = ttk.Button(conn_frame, text="接続", command=self.toggle_connection)
        self.conn_button.pack(side="left", padx=5)
        
        self.conn_status = ttk.Label(conn_frame, text="未接続", foreground="red")
        self.conn_status.pack(side="left", padx=20)
        
        # 角度制御フレーム
        angle_frame = ttk.LabelFrame(self.root, text="角度制御", padding=10)
        angle_frame.pack(fill="x", padx=10, pady=5)
        
        # 絶対角度
        abs_frame = ttk.Frame(angle_frame)
        abs_frame.pack(fill="x", pady=5)
        
        ttk.Label(abs_frame, text="絶対角度:").pack(side="left", padx=5)
        self.abs_angle = ttk.Scale(abs_frame, from_=0, to=360, orient="horizontal", length=300)
        self.abs_angle.pack(side="left", padx=5)
        self.abs_value = ttk.Label(abs_frame, text="0°")
        self.abs_value.pack(side="left", padx=5)
        self.abs_angle.configure(command=lambda v: self.abs_value.config(text=f"{float(v):.0f}°"))
        
        ttk.Button(abs_frame, text="移動", command=self.move_absolute).pack(side="left", padx=5)
        
        # 相対角度
        rel_frame = ttk.Frame(angle_frame)
        rel_frame.pack(fill="x", pady=5)
        
        ttk.Label(rel_frame, text="相対角度:").pack(side="left", padx=5)
        self.rel_entry = ttk.Entry(rel_frame, width=10)
        self.rel_entry.pack(side="left", padx=5)
        self.rel_entry.insert(0, "45")
        
        ttk.Button(rel_frame, text="+", command=lambda: self.move_relative(True)).pack(side="left", padx=2)
        ttk.Button(rel_frame, text="-", command=lambda: self.move_relative(False)).pack(side="left", padx=2)
        
        # プリセットボタン
        preset_frame = ttk.Frame(angle_frame)
        preset_frame.pack(fill="x", pady=5)
        
        for angle in [0, 90, 180, 270]:
            ttk.Button(preset_frame, text=f"{angle}°", 
                      command=lambda a=angle: self.quick_move(a)).pack(side="left", padx=5)
        
        # 制御フレーム
        ctrl_frame = ttk.LabelFrame(self.root, text="制御", padding=10)
        ctrl_frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Button(ctrl_frame, text="ゼロリセット", command=lambda: self.send_command("z")).pack(side="left", padx=5)
        ttk.Button(ctrl_frame, text="ステータス", command=lambda: self.send_command("s")).pack(side="left", padx=5)
        
        self.power_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(ctrl_frame, text="電源節約", variable=self.power_var,
                       command=lambda: self.send_command("p")).pack(side="left", padx=5)
        
        # 速度選択
        speed_frame = ttk.Frame(ctrl_frame)
        speed_frame.pack(side="left", padx=20)
        ttk.Label(speed_frame, text="速度:").pack(side="left")
        self.speed_var = tk.StringVar(value="3")
        speed_combo = ttk.Combobox(speed_frame, textvariable=self.speed_var, 
                                  values=["1", "2", "3", "4", "5"], width=5)
        speed_combo.pack(side="left", padx=5)
        speed_combo.bind("<<ComboboxSelected>>", lambda e: self.send_command(f"v{self.speed_var.get()}"))
        
        # ログエリア
        log_frame = ttk.LabelFrame(self.root, text="ログ", padding=10)
        log_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.log_text = scrolledtext.ScrolledText(log_frame, height=10, wrap=tk.WORD)
        self.log_text.pack(fill="both", expand=True)
        
        # ステータスバー
        self.status_bar = ttk.Label(self.root, text="準備完了", relief=tk.SUNKEN)
        self.status_bar.pack(fill="x", side="bottom")
        
    def log(self, message):
        """ログに追加"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.insert(tk.END, f"[{timestamp}] {message}\n")
        self.log_text.see(tk.END)
        
    def toggle_connection(self):
        """接続/切断の切り替え"""
        if self.client and self.client.is_connected:
            self.disconnect()
        else:
            self.connect()
            
    def connect(self):
        """BLE接続を開始"""
        self.conn_button.config(state="disabled")
        self.log("接続を開始します...")
        
        # 別スレッドで非同期処理を実行
        thread = threading.Thread(target=self._connect_thread)
        thread.daemon = True
        thread.start()
        
    def _connect_thread(self):
        """接続処理（別スレッド）"""
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        self.loop.run_until_complete(self._connect_async())
        
    async def _connect_async(self):
        """非同期接続処理"""
        try:
            # デバイス検索
            self.log("BLEデバイスを検索中...")
            devices = await BleakScanner.discover()
            
            for device in devices:
                if device.name == "StepperMotor" or device.name == "Arduino":
                    self.device = device
                    self.log(f"デバイスを発見: {device.address}")
                    break
            else:
                self.log("StepperMotorが見つかりません")
                self.root.after(0, lambda: self.conn_button.config(state="normal"))
                return
                
            # 接続
            self.client = BleakClient(self.device.address)
            await self.client.connect()
            
            # 通知を有効化
            await self.client.start_notify(STATUS_UUID, self.notification_handler)
            
            self.log("接続成功！")
            self.root.after(0, self.update_connected_state)
            
            # 接続を維持
            while self.client and self.client.is_connected:
                await asyncio.sleep(1)
                
        except Exception as e:
            self.log(f"接続エラー: {e}")
            self.root.after(0, lambda: self.conn_button.config(state="normal"))
            
    def update_connected_state(self):
        """接続状態のUIを更新"""
        self.conn_button.config(text="切断", state="normal")
        self.conn_status.config(text="接続済み", foreground="green")
        
    def disconnect(self):
        """切断"""
        if self.client:
            asyncio.run_coroutine_threadsafe(self.client.disconnect(), self.loop)
            self.client = None
            
        self.conn_button.config(text="接続")
        self.conn_status.config(text="未接続", foreground="red")
        self.log("切断しました")
        
    def notification_handler(self, sender, data):
        """通知ハンドラー"""
        try:
            # 受信データをバッファに追加（エラー処理付き）
            try:
                decoded_data = data.decode('utf-8')
            except UnicodeDecodeError:
                # UTF-8デコードに失敗した場合、エラー文字を無視してデコード
                decoded_data = data.decode('utf-8', errors='ignore')
                self.root.after(0, lambda: self.log("警告: 文字化けを検出しました"))
            
            self.message_buffer += decoded_data
            
            # 改行文字で区切られた完全なメッセージを処理
            while '\n' in self.message_buffer:
                line_end = self.message_buffer.index('\n')
                message = self.message_buffer[:line_end].strip()
                self.message_buffer = self.message_buffer[line_end + 1:]
                
                if message:  # 空行でない場合
                    self.root.after(0, lambda m=message: self.log(f"受信: {m}"))
                    
                    # 角度情報の抽出（"deg"または"°"に対応）
                    if "deg" in message or "°" in message:
                        try:
                            # "deg"の場合
                            if "deg" in message:
                                angle_str = message.split("deg")[0].split(":")[-1].strip()
                            # "°"の場合
                            else:
                                angle_str = message.split("°")[0].split(":")[-1].strip()
                            
                            angle = float(angle_str)
                            self.current_angle = angle
                            self.root.after(0, lambda a=angle: self.status_bar.config(text=f"現在角度: {a:.1f}°"))
                        except (ValueError, IndexError):
                            pass
                            
        except Exception as e:
            # 予期しないエラーの場合
            self.root.after(0, lambda: self.log(f"通信エラー: {str(e)}"))
            self.message_buffer = ""  # バッファをクリア
                
    def send_command(self, command):
        """コマンド送信"""
        if not self.client or not self.client.is_connected:
            self.log("デバイスが接続されていません")
            return
            
        self.log(f"送信: {command}")
        asyncio.run_coroutine_threadsafe(
            self.client.write_gatt_char(COMMAND_UUID, command.encode('utf-8')),
            self.loop
        )
        
    def move_absolute(self):
        """絶対角度移動"""
        angle = int(self.abs_angle.get())
        self.send_command(f"a{angle}")
        
    def move_relative(self, positive):
        """相対角度移動"""
        try:
            angle = float(self.rel_entry.get())
            if not positive:
                angle = -angle
            self.send_command(f"r{angle}")
        except ValueError:
            self.log("相対角度は数値を入力してください")
            
    def quick_move(self, angle):
        """クイック移動"""
        self.send_command(f"a{angle}")
        self.abs_angle.set(angle)
        
    def run(self):
        """GUIを実行"""
        try:
            self.root.mainloop()
        finally:
            # 終了時の処理
            if self.client and self.client.is_connected and self.loop:
                # 既存のイベントループで切断を実行
                asyncio.run_coroutine_threadsafe(
                    self.client.disconnect(), 
                    self.loop
                ).result(timeout=2)
            
            # スレッドが実行中の場合は停止
            if self.thread and self.thread.is_alive():
                self.loop.call_soon_threadsafe(self.loop.stop)
                self.thread.join(timeout=2)

if __name__ == "__main__":
    app = StepperMotorGUI()
    app.run()
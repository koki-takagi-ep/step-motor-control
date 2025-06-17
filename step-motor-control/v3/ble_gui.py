#!/usr/bin/env python3
"""
BLE Stepper Motor GUI (Refactored)
Arduino R4 WiFiのステッピングモーター制御用GUI

必要なライブラリ:
pip install bleak asyncio tkinter
"""

import asyncio
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import threading
import logging
from datetime import datetime
from typing import Optional

# 共通ライブラリをインポート
from ble_common import BLEStepperClient, MotorCommands, BLEConnectionError, format_angle, extract_angle_from_message

# ログ設定
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class StepperMotorGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("ステッピングモーター BLE制御 v2.0")
        self.root.geometry("700x600")
        self.root.minsize(600, 500)
        
        # 状態管理
        self.ble_client: Optional[BLEStepperClient] = None
        self.loop: Optional[asyncio.AbstractEventLoop] = None
        self.connection_thread: Optional[threading.Thread] = None
        self.current_angle = 0.0
        self.is_moving = False
        
        # GUI初期化
        self.setup_gui()
        self.setup_menu()
        
        # 終了時の処理
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        
    def setup_menu(self):
        """メニューバーの設定"""
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)
        
        # ファイルメニュー
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="ファイル", menu=file_menu)
        file_menu.add_command(label="ログをクリア", command=self.clear_log)
        file_menu.add_separator()
        file_menu.add_command(label="終了", command=self.on_closing)
        
        # ツールメニュー
        tools_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="ツール", menu=tools_menu)
        tools_menu.add_command(label="デバイススキャン", command=self.scan_devices)
        
        # ヘルプメニュー
        help_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="ヘルプ", menu=help_menu)
        help_menu.add_command(label="コマンド一覧", command=self.show_command_help)
        help_menu.add_command(label="バージョン情報", command=self.show_about)
    
    def setup_gui(self):
        """GUIのセットアップ"""
        # メインフレーム
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        # 接続フレーム
        conn_frame = ttk.LabelFrame(main_frame, text="接続", padding=10)
        conn_frame.pack(fill="x", pady=(0, 5))
        
        # 接続ボタンとステータス
        conn_left_frame = ttk.Frame(conn_frame)
        conn_left_frame.pack(side="left", fill="x", expand=True)
        
        self.conn_button = ttk.Button(conn_left_frame, text="接続", command=self.toggle_connection)
        self.conn_button.pack(side="left", padx=(0, 10))
        
        self.conn_status = ttk.Label(conn_left_frame, text="未接続", foreground="red")
        self.conn_status.pack(side="left", padx=(0, 10))
        
        # 現在角度表示
        self.angle_display = ttk.Label(conn_left_frame, text="角度: --°", font=('Arial', 10, 'bold'))
        self.angle_display.pack(side="left", padx=(0, 10))
        
        # 動作状態表示
        self.status_display = ttk.Label(conn_left_frame, text="停止", foreground="green")
        self.status_display.pack(side="left")
        
        # スキャンボタン
        self.scan_button = ttk.Button(conn_frame, text="スキャン", command=self.scan_devices)
        self.scan_button.pack(side="right")
        
        # 角度制御フレーム
        angle_frame = ttk.LabelFrame(main_frame, text="角度制御", padding=10)
        angle_frame.pack(fill="x", pady=5)
        
        # 絶対角度制御
        abs_frame = ttk.LabelFrame(angle_frame, text="絶対角度", padding=5)
        abs_frame.pack(fill="x", pady=(0, 5))
        
        abs_control_frame = ttk.Frame(abs_frame)
        abs_control_frame.pack(fill="x")
        
        ttk.Label(abs_control_frame, text="目標:").pack(side="left", padx=(0, 5))
        self.abs_angle = ttk.Scale(abs_control_frame, from_=0, to=360, orient="horizontal", length=200)
        self.abs_angle.pack(side="left", padx=(0, 5))
        self.abs_value = ttk.Label(abs_control_frame, text="0°", width=6)
        self.abs_value.pack(side="left", padx=(0, 5))
        self.abs_angle.configure(command=lambda v: self.abs_value.config(text=f"{float(v):.0f}°"))
        
        self.abs_move_button = ttk.Button(abs_control_frame, text="移動", command=self.move_absolute)
        self.abs_move_button.pack(side="left", padx=(0, 10))
        
        # エントリー入力
        ttk.Label(abs_control_frame, text="直接入力:").pack(side="left", padx=(10, 5))
        self.abs_entry = ttk.Entry(abs_control_frame, width=8)
        self.abs_entry.pack(side="left", padx=(0, 5))
        self.abs_entry.bind('<Return>', lambda e: self.move_absolute_from_entry())
        
        ttk.Button(abs_control_frame, text="移動", command=self.move_absolute_from_entry).pack(side="left")
        
        # 相対角度制御
        rel_frame = ttk.LabelFrame(angle_frame, text="相対角度", padding=5)
        rel_frame.pack(fill="x", pady=(0, 5))
        
        rel_control_frame = ttk.Frame(rel_frame)
        rel_control_frame.pack(fill="x")
        
        ttk.Label(rel_control_frame, text="角度:").pack(side="left", padx=(0, 5))
        self.rel_entry = ttk.Entry(rel_control_frame, width=10)
        self.rel_entry.pack(side="left", padx=(0, 5))
        self.rel_entry.insert(0, "45")
        self.rel_entry.bind('<Return>', lambda e: self.move_relative(True))
        
        ttk.Button(rel_control_frame, text="+ (時計回り)", command=lambda: self.move_relative(True)).pack(side="left", padx=(5, 2))
        ttk.Button(rel_control_frame, text="- (反時計回り)", command=lambda: self.move_relative(False)).pack(side="left", padx=2)
        
        # クイック相対移動
        quick_rel_frame = ttk.Frame(rel_frame)
        quick_rel_frame.pack(fill="x", pady=(5, 0))
        
        ttk.Label(quick_rel_frame, text="クイック:").pack(side="left", padx=(0, 5))
        for angle in [15, 30, 45, 90]:
            ttk.Button(quick_rel_frame, text=f"+{angle}°", width=6,
                      command=lambda a=angle: self.quick_relative_move(a)).pack(side="left", padx=1)
        for angle in [15, 30, 45, 90]:
            ttk.Button(quick_rel_frame, text=f"-{angle}°", width=6,
                      command=lambda a=angle: self.quick_relative_move(-a)).pack(side="left", padx=1)
        
        # プリセット角度
        preset_frame = ttk.LabelFrame(angle_frame, text="プリセット角度", padding=5)
        preset_frame.pack(fill="x")
        
        preset_angles = [0, 45, 90, 135, 180, 225, 270, 315]
        for i, angle in enumerate(preset_angles):
            if i == 4:  # 4個目で改行
                ttk.Frame(preset_frame).pack()  # 改行用の空フレーム
            ttk.Button(preset_frame, text=f"{angle}°", width=6,
                      command=lambda a=angle: self.quick_move(a)).pack(side="left", padx=2, pady=2)
        
        # 制御フレーム
        ctrl_frame = ttk.LabelFrame(main_frame, text="制御", padding=10)
        ctrl_frame.pack(fill="x", pady=5)
        
        # 基本制御
        basic_ctrl_frame = ttk.Frame(ctrl_frame)
        basic_ctrl_frame.pack(fill="x", pady=(0, 5))
        
        ttk.Button(basic_ctrl_frame, text="ゼロリセット", command=self.reset_position).pack(side="left", padx=(0, 5))
        ttk.Button(basic_ctrl_frame, text="ステータス取得", command=self.get_status).pack(side="left", padx=5)
        ttk.Button(basic_ctrl_frame, text="停止", command=self.emergency_stop).pack(side="left", padx=5)
        
        # 設定
        settings_frame = ttk.Frame(ctrl_frame)
        settings_frame.pack(fill="x")
        
        self.power_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(settings_frame, text="電源節約モード", variable=self.power_var,
                       command=self.toggle_power_saving).pack(side="left", padx=(0, 20))
        
        # 速度選択
        ttk.Label(settings_frame, text="速度:").pack(side="left")
        self.speed_var = tk.StringVar(value="3")
        speed_combo = ttk.Combobox(settings_frame, textvariable=self.speed_var, 
                                  values=["1 (最遅)", "2 (遅い)", "3 (通常)", "4 (速い)", "5 (最速)"], 
                                  width=12, state="readonly")
        speed_combo.pack(side="left", padx=(5, 0))
        speed_combo.bind("<<ComboboxSelected>>", self.on_speed_changed)
        
        # 進捗バー（動作中表示用）
        self.progress_frame = ttk.Frame(ctrl_frame)
        self.progress_frame.pack(fill="x", pady=(10, 0))
        
        self.progress_label = ttk.Label(self.progress_frame, text="")
        self.progress_label.pack(anchor="w")
        
        self.progress_bar = ttk.Progressbar(self.progress_frame, mode='indeterminate')
        self.progress_bar.pack(fill="x", pady=(2, 0))
        
        # ログエリア
        log_frame = ttk.LabelFrame(main_frame, text="ログ", padding=5)
        log_frame.pack(fill="both", expand=True, pady=5)
        
        # ログツールバー
        log_toolbar = ttk.Frame(log_frame)
        log_toolbar.pack(fill="x", pady=(0, 5))
        
        ttk.Button(log_toolbar, text="クリア", command=self.clear_log).pack(side="left")
        self.auto_scroll_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(log_toolbar, text="自動スクロール", variable=self.auto_scroll_var).pack(side="left", padx=(10, 0))
        
        # ログテキスト
        self.log_text = scrolledtext.ScrolledText(log_frame, height=12, wrap=tk.WORD, font=('Consolas', 9))
        self.log_text.pack(fill="both", expand=True)
        
        # ステータスバー
        status_frame = ttk.Frame(self.root)
        status_frame.pack(fill="x", side="bottom")
        
        self.status_bar = ttk.Label(status_frame, text="準備完了", relief=tk.SUNKEN, anchor="w")
        self.status_bar.pack(side="left", fill="x", expand=True)
        
        # 時刻表示
        self.time_label = ttk.Label(status_frame, text="", relief=tk.SUNKEN, width=12)
        self.time_label.pack(side="right")
        
        # 時刻更新
        self.update_time()
        
    def log(self, message, level="INFO"):
        """ログに追加"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        
        # レベルに応じた色分け
        colors = {
            "INFO": "black",
            "ERROR": "red",
            "WARNING": "orange",
            "SUCCESS": "green",
            "RECEIVED": "blue"
        }
        
        # テキスト挿入
        self.log_text.insert(tk.END, f"[{timestamp}] [{level}] {message}\n")
        
        # 色設定
        if level in colors:
            line_start = self.log_text.index(tk.END + "-2l")
            line_end = self.log_text.index(tk.END + "-1l")
            tag_name = f"level_{level}"
            self.log_text.tag_add(tag_name, line_start, line_end)
            self.log_text.tag_config(tag_name, foreground=colors[level])
        
        # 自動スクロール
        if self.auto_scroll_var.get():
            self.log_text.see(tk.END)
        
        # ログサイズ制限（1000行を超えたら古い行を削除）
        lines = int(self.log_text.index(tk.END).split('.')[0])
        if lines > 1000:
            self.log_text.delete('1.0', '100.0')
    
    def clear_log(self):
        """ログをクリア"""
        self.log_text.delete('1.0', tk.END)
        self.log("ログをクリアしました")
    
    def update_time(self):
        """時刻表示を更新"""
        current_time = datetime.now().strftime("%H:%M:%S")
        self.time_label.config(text=current_time)
        self.root.after(1000, self.update_time)
        
    def toggle_connection(self):
        """接続/切断の切り替え"""
        if self.ble_client and self.ble_client.is_connected:
            self.disconnect()
        else:
            self.connect()
    
    def scan_devices(self):
        """デバイススキャンを実行"""
        self.log("デバイススキャンを開始...")
        thread = threading.Thread(target=self._scan_devices_thread)
        thread.daemon = True
        thread.start()
    
    def _scan_devices_thread(self):
        """デバイススキャン（別スレッド）"""
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        try:
            devices = loop.run_until_complete(self._scan_devices_async())
            self.root.after(0, lambda: self._show_scan_results(devices))
        finally:
            loop.close()
    
    async def _scan_devices_async(self):
        """非同期デバイススキャン"""
        try:
            client = BLEStepperClient()
            return await client.scan_devices()
        except Exception as e:
            self.root.after(0, lambda: self.log(f"スキャンエラー: {e}", "ERROR"))
            return []
    
    def _show_scan_results(self, devices):
        """スキャン結果を表示"""
        if not devices:
            self.log("デバイスが見つかりませんでした", "WARNING")
            return
        
        self.log(f"{len(devices)}個のデバイスを発見:")
        for device in devices:
            status = " [対象デバイス]" if device['is_target'] else ""
            self.log(f"  {device['name']} ({device['address']}) RSSI: {device['rssi']}dBm{status}")
            
    def connect(self):
        """BLE接続を開始"""
        self.conn_button.config(state="disabled", text="接続中...")
        self.scan_button.config(state="disabled")
        self.log("接続を開始します...")
        
        # 別スレッドで非同期処理を実行
        self.connection_thread = threading.Thread(target=self._connect_thread)
        self.connection_thread.daemon = True
        self.connection_thread.start()
        
    def _connect_thread(self):
        """接続処理（別スレッド）"""
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        try:
            self.loop.run_until_complete(self._connect_async())
        except Exception as e:
            self.root.after(0, lambda: self._handle_connection_error(e))
        finally:
            # 接続失敗時のUI復旧
            if not (self.ble_client and self.ble_client.is_connected):
                self.root.after(0, self._reset_connection_ui)
        
    async def _connect_async(self):
        """非同期接続処理"""
        try:
            # BLEクライアント作成
            self.ble_client = BLEStepperClient(on_status_received=self._handle_status_message)
            
            # 接続実行
            await self.ble_client.connect()
            
            # UI更新
            self.root.after(0, self._update_connected_state)
            
            # 接続維持ループ
            while self.ble_client and self.ble_client.is_connected:
                await asyncio.sleep(1)
                
        except BLEConnectionError as e:
            self.root.after(0, lambda: self.log(f"接続エラー: {e}", "ERROR"))
        except Exception as e:
            self.root.after(0, lambda: self.log(f"予期しないエラー: {e}", "ERROR"))
    
    def _handle_connection_error(self, error):
        """接続エラーハンドリング"""
        self.log(f"接続に失敗しました: {error}", "ERROR")
        messagebox.showerror("接続エラー", f"デバイスに接続できませんでした。\n\nエラー: {error}")
    
    def _reset_connection_ui(self):
        """接続UI状態をリセット"""
        self.conn_button.config(state="normal", text="接続")
        self.scan_button.config(state="normal")
        self.conn_status.config(text="未接続", foreground="red")
            
    def _update_connected_state(self):
        """接続状態のUIを更新"""
        self.conn_button.config(text="切断", state="normal")
        self.scan_button.config(state="normal")
        self.conn_status.config(text="接続済み", foreground="green")
        self.log("接続が確立されました", "SUCCESS")
        
        # 初期ステータス取得
        self.get_status()
        
    def disconnect(self):
        """切断"""
        if self.ble_client and self.loop:
            asyncio.run_coroutine_threadsafe(self.ble_client.disconnect(), self.loop)
            
        self._reset_connection_ui()
        self.angle_display.config(text="角度: --°")
        self.status_display.config(text="切断済み", foreground="gray")
        self.log("切断しました")
        
        # 進捗バー停止
        self.progress_bar.stop()
        self.progress_label.config(text="")
        
    def _handle_status_message(self, message: str):
        """ステータスメッセージハンドラー"""
        self.root.after(0, lambda: self._process_status_message(message))
    
    def _process_status_message(self, message: str):
        """ステータスメッセージを処理"""
        self.log(f"受信: {message}", "RECEIVED")
        
        # 角度情報の抽出と表示更新
        angle = extract_angle_from_message(message)
        if angle is not None:
            self.current_angle = angle
            self.angle_display.config(text=f"角度: {format_angle(angle)}")
            self.status_bar.config(text=f"現在角度: {format_angle(angle)}")
        
        # 動作状態の更新
        if "動作中" in message or "RUN" in message:
            self.status_display.config(text="動作中", foreground="orange")
            if not self.is_moving:
                self.is_moving = True
                self.progress_bar.start()
                self.progress_label.config(text="モーター動作中...")
        elif "停止" in message or "STOP" in message:
            self.status_display.config(text="停止", foreground="green")
            if self.is_moving:
                self.is_moving = False
                self.progress_bar.stop()
                self.progress_label.config(text="")
        
        # エラーメッセージの処理
        if "エラー" in message or "ERROR" in message:
            self.log(f"デバイスエラー: {message}", "ERROR")
            messagebox.showwarning("デバイスエラー", message)
                
    def send_command(self, command: str) -> bool:
        """コマンド送信"""
        if not self.ble_client or not self.ble_client.is_connected:
            self.log("デバイスが接続されていません", "WARNING")
            messagebox.showwarning("接続エラー", "デバイスに接続されていません")
            return False
        
        self.log(f"送信: {command}")
        future = asyncio.run_coroutine_threadsafe(
            self.ble_client.send_command(command),
            self.loop
        )
        
        try:
            success = future.result(timeout=2.0)
            if not success:
                self.log(f"コマンド送信失敗: {command}", "ERROR")
            return success
        except asyncio.TimeoutError:
            self.log(f"コマンド送信タイムアウト: {command}", "ERROR")
            return False
        
    def move_absolute(self):
        """絶対角度移動（スライダー）"""
        angle = float(self.abs_angle.get())
        self._execute_absolute_move(angle)
    
    def move_absolute_from_entry(self):
        """絶対角度移動（エントリー入力）"""
        try:
            angle = float(self.abs_entry.get())
            if not (0 <= angle <= 360):
                messagebox.showerror("入力エラー", "角度は0-360度の範囲で入力してください")
                return
            
            # スライダーも更新
            self.abs_angle.set(angle)
            self._execute_absolute_move(angle)
            
        except ValueError:
            messagebox.showerror("入力エラー", "有効な数値を入力してください")
    
    def _execute_absolute_move(self, angle: float):
        """絶対角度移動を実行"""
        command = MotorCommands.absolute_move(angle)
        if self.send_command(command):
            self.log(f"絶対角度移動: {format_angle(angle)}")
        
        # エントリーをクリア
        self.abs_entry.delete(0, tk.END)
        
    def move_relative(self, positive: bool):
        """相対角度移動"""
        try:
            angle = float(self.rel_entry.get())
            if not positive:
                angle = -angle
            
            command = MotorCommands.relative_move(angle)
            if self.send_command(command):
                direction = "時計回り" if angle >= 0 else "反時計回り"
                self.log(f"相対角度移動: {abs(angle):.1f}° ({direction})")
                
        except ValueError:
            messagebox.showerror("入力エラー", "相対角度は数値を入力してください")
    
    def quick_relative_move(self, angle: float):
        """クイック相対角度移動"""
        command = MotorCommands.relative_move(angle)
        if self.send_command(command):
            direction = "時計回り" if angle >= 0 else "反時計回り"
            self.log(f"クイック相対移動: {abs(angle):.1f}° ({direction})")
            
    def quick_move(self, angle: float):
        """クイック移動"""
        command = MotorCommands.absolute_move(angle)
        if self.send_command(command):
            self.abs_angle.set(angle)
            self.log(f"プリセット移動: {format_angle(angle)}")
    
    def reset_position(self):
        """位置リセット"""
        command = MotorCommands.reset_position()
        if self.send_command(command):
            self.current_angle = 0.0
            self.abs_angle.set(0)
            self.angle_display.config(text="角度: 0.0°")
            self.log("位置をリセットしました")
    
    def get_status(self):
        """ステータス取得"""
        command = MotorCommands.get_status()
        if self.send_command(command):
            self.log("ステータスを要求しました")
    
    def toggle_power_saving(self):
        """電源節約モード切替"""
        command = MotorCommands.toggle_power_saving()
        if self.send_command(command):
            mode = "ON" if self.power_var.get() else "OFF"
            self.log(f"電源節約モード: {mode}")
    
    def on_speed_changed(self, event):
        """速度変更時の処理"""
        speed_text = self.speed_var.get()
        speed_level = int(speed_text.split()[0])  # "1 (最遅)" -> 1
        
        try:
            command = MotorCommands.set_speed(speed_level)
            if self.send_command(command):
                self.log(f"速度設定: レベル{speed_level}")
        except ValueError as e:
            self.log(f"速度設定エラー: {e}", "ERROR")
    
    def emergency_stop(self):
        """緊急停止（将来の実装用）"""
        self.log("緊急停止は現在未実装です", "WARNING")
        messagebox.showinfo("情報", "緊急停止機能は現在開発中です")
    
    def show_command_help(self):
        """コマンドヘルプを表示"""
        help_text = """
コマンド一覧:

a[角度]: 絶対角度移動 (例: a90)
r[角度]: 相対角度移動 (例: r45, r-30)
z: 位置リセット
s: ステータス表示
p: 電源節約モード切替
v[1-5]: 速度設定 (1:最遅 ～ 5:最速)
h: ヘルプ表示

※角度は0-360度の範囲で指定
※相対角度では負の値で反時計回り
        """.strip()
        
        messagebox.showinfo("コマンドヘルプ", help_text)
    
    def show_about(self):
        """バージョン情報を表示"""
        about_text = """
BLE Stepper Motor Controller v2.0

Arduino UNO R4 WiFi用
28BYJ-48ステッピングモーター制御GUI

開発: Koki Takagi
更新: 2025年6月17日
        """.strip()
        
        messagebox.showinfo("バージョン情報", about_text)
    
    def on_closing(self):
        """アプリケーション終了時の処理"""
        if self.ble_client and self.ble_client.is_connected:
            self.log("接続を切断しています...")
            self.disconnect()
            # 切断完了まで少し待機
            self.root.after(500, self.root.destroy)
        else:
            self.root.destroy()
        
    def run(self):
        """GUIを実行"""
        self.root.mainloop()

if __name__ == "__main__":
    app = StepperMotorGUI()
    app.run()
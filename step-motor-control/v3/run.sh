#!/bin/bash
# BLEステッピングモーター制御ツール起動スクリプト (v2.0)

set -e  # エラー時に終了

# venv環境の設定
VENV_PATH="/Users/koki/Desktop/venv"
VENV_ACTIVATED=false

# 色付き出力用
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# スクリプトのディレクトリに移動
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ヘルプ関数
show_help() {
    echo -e "${CYAN}BLEステッピングモーター制御ツール v2.0${NC}"
    echo -e "${YELLOW}使用方法:${NC}"
    echo "  $0 [OPTION]"
    echo ""
    echo -e "${YELLOW}オプション:${NC}"
    echo "  -h, --help     このヘルプを表示"
    echo "  -i, --info     システム情報を表示"
    echo "  -s, --scan     デバイススキャンのみ実行"
    echo "  -c, --cli      CLI版を直接起動"
    echo "  -g, --gui      GUI版を直接起動"
    echo "  --check-deps   依存関係をチェック"
    echo ""
}

# システム情報表示
show_info() {
    echo -e "${CYAN}=== システム情報 ===${NC}"
    echo -e "OS: $(uname -s)"
    echo -e "Python: $(python3 --version 2>/dev/null || echo '未インストール')"
    echo -e "Bluetooth: $(if command -v bluetoothctl >/dev/null 2>&1; then echo '利用可能'; else echo '未確認'; fi)"
    echo -e "現在ディレクトリ: $PWD"
    if [[ "$VENV_ACTIVATED" == "true" ]]; then
        echo -e "仮想環境: アクティブ ($VENV_PATH)"
    else
        echo -e "仮想環境: 非アクティブ"
    fi
    echo ""
}

# venv環境をアクティベート
activate_venv() {
    if [[ -d "$VENV_PATH" && -f "$VENV_PATH/bin/activate" ]]; then
        echo -e "${CYAN}🐍 Python仮想環境をアクティベート中...${NC}"
        source "$VENV_PATH/bin/activate"
        VENV_ACTIVATED=true
        echo -e "${GREEN}✅ 仮想環境をアクティベートしました: $VENV_PATH${NC}"
    else
        echo -e "${YELLOW}⚠️  仮想環境が見つかりません: $VENV_PATH${NC}"
        echo -e "${YELLOW}システムのPython環境を使用します${NC}"
    fi
}

# 不足している依存関係を自動インストール
install_missing_dependencies() {
    local libs=("$@")
    
    for lib in "${libs[@]}"; do
        if [[ "$lib" == "tkinter" ]]; then
            echo -e "${YELLOW}ℹ️  tkinterはPythonの標準ライブラリです。Python再インストールが必要かもしれません${NC}"
            continue
        fi
        
        echo -e "${YELLOW}📦 $lib をインストール中...${NC}"
        
        if pip install "$lib"; then
            echo -e "${GREEN}✅ $lib のインストールが完了しました${NC}"
        else
            echo -e "${RED}❌ $lib のインストールに失敗しました${NC}"
            echo -e "${YELLOW}手動でインストールしてください: pip install $lib${NC}"
            return 1
        fi
    done
    
    echo -e "${GREEN}🎉 依存関係のインストールが完了しました${NC}"
    return 0
}

# 依存関係チェック
check_dependencies() {
    echo -e "${YELLOW}🔍 依存関係をチェック中...${NC}"
    
    # venv環境をアクティベート
    activate_venv
    
    # Pythonチェック
    if ! command -v python3 >/dev/null 2>&1; then
        echo -e "${RED}⚠️  Python3がインストールされていません${NC}"
        return 1
    fi
    
    echo -e "Python環境: $(python3 --version) ($(which python3))"
    
    # 必要なファイルのチェック
    local required_files=("ble_common.py" "ble_monitor.py" "ble_gui.py" "ble_scanner.py")
    local missing_files=()
    
    for file in "${required_files[@]}"; do
        if [[ ! -f "$file" ]]; then
            missing_files+=("$file")
        fi
    done
    
    if [[ ${#missing_files[@]} -gt 0 ]]; then
        echo -e "${RED}⚠️  以下のファイルが見つかりません:${NC}"
        for file in "${missing_files[@]}"; do
            echo "    - $file"
        done
        return 1
    fi
    
    # Pythonライブラリのチェック
    echo -e "${YELLOW}📚 Pythonライブラリをチェック中...${NC}"
    
    local missing_libs=()
    
    # bleakのチェック
    if ! python3 -c "import bleak" 2>/dev/null; then
        missing_libs+=("bleak")
    else
        local bleak_version=$(pip show bleak 2>/dev/null | grep Version | cut -d' ' -f2 || echo "不明")
        echo -e "  ✅ bleak: $bleak_version"
    fi
    
    # tkinterのチェック
    if ! python3 -c "import tkinter" 2>/dev/null; then
        missing_libs+=("tkinter")
    else
        echo -e "  ✅ tkinter: 利用可能"
    fi
    
    # asyncioのチェック（Python 3.7+で標準）
    if ! python3 -c "import asyncio" 2>/dev/null; then
        missing_libs+=("asyncio")
    else
        echo -e "  ✅ asyncio: 利用可能"
    fi
    
    if [[ ${#missing_libs[@]} -gt 0 ]]; then
        echo -e "${RED}⚠️  以下のPythonライブラリがインストールされていません:${NC}"
        for lib in "${missing_libs[@]}"; do
            echo "    - $lib"
        done
        
        # 自動インストールを試行
        echo -e "${YELLOW}📦 不足している依存関係を自動インストールします...${NC}"
        install_missing_dependencies "${missing_libs[@]}"
        return $?
    fi
    
    echo -e "${GREEN}✅ すべての依存関係が満たされています${NC}"
    if [[ "$VENV_ACTIVATED" == "true" ]]; then
        echo -e "${CYAN}🐍 仮想環境使用中: $VENV_PATH${NC}"
    fi
    return 0
}

# メインメニュー
show_main_menu() {
    echo -e "${CYAN}"
    echo "┌──────────────────────────────────────────────────────┐"
    echo "│   BLEステッピングモーター制御ツール v2.0         │"
    echo "│   Arduino UNO R4 WiFi + 28BYJ-48                  │"
    echo "└──────────────────────────────────────────────────────┘"
    echo -e "${NC}"
    
    echo -e "${YELLOW}🛠️  選択してください:${NC}"
    echo ""
    echo -e "${GREEN}1)${NC} 📱 コマンドライン版 (CLI) - 軽量、高速"
    echo -e "${GREEN}2)${NC} 🖥️  GUI版 - 直感的、初心者向け"
    echo -e "${GREEN}3)${NC} 🔍 デバイススキャン - 接続可能デバイスを検索"
    echo -e "${GREEN}4)${NC} 📊 システム情報 - 環境確認"
    echo -e "${GREEN}5)${NC} 🔧 依存関係チェック - トラブルシューティング"
    echo -e "${RED}q)${NC} ❌ 終了"
    echo ""
    echo -ne "${BLUE}選択 (1-5, q):${NC} "
}

# アプリ起動関数
launch_app() {
    local app_type="$1"
    local app_name="$2"
    local script_name="$3"
    
    echo -e "${GREEN}🚀 ${app_name}を起動します...${NC}"
    echo -e "${YELLOW}ℹ️  終了するには Ctrl+C を押してください${NC}"
    echo ""
    
    # venv環境を確実にアクティベート
    if [[ "$VENV_ACTIVATED" == "false" ]]; then
        activate_venv
    fi
    
    # 短い遅延で読みやすくする
    sleep 1
    
    if [[ ! -f "$script_name" ]]; then
        echo -e "${RED}⚠️  エラー: $script_name が見つかりません${NC}"
        return 1
    fi
    
    # Python環境の確認
    echo -e "${CYAN}使用するPython: $(which python3)${NC}"
    
    # アプリを起動
    python3 "$script_name" || {
        echo -e "${RED}⚠️  $app_name の実行中にエラーが発生しました${NC}"
        return 1
    }
}

# メイン処理
main() {
    # コマンドライン引数処理
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -i|--info)
                activate_venv
                show_info
                exit 0
                ;;
            -s|--scan)
                echo -e "${GREEN}🔍 デバイススキャンを実行します...${NC}"
                activate_venv
                python3 ble_scanner.py
                exit $?
                ;;
            -c|--cli)
                activate_venv
                launch_app "cli" "CLI版" "ble_monitor.py"
                exit $?
                ;;
            -g|--gui)
                activate_venv
                launch_app "gui" "GUI版" "ble_gui.py"
                exit $?
                ;;
            --check-deps)
                check_dependencies
                exit $?
                ;;
            *)
                echo -e "${RED}不明なオプション: $1${NC}"
                show_help
                exit 1
                ;;
        esac
        shift
    done
    
    # 対話モード
    while true; do
        show_main_menu
        read -r choice
        echo ""
        
        case $choice in
            1)
                if check_dependencies; then
                    launch_app "cli" "CLI版" "ble_monitor.py"
                else
                    echo -e "${RED}依存関係の問題を解決してから再度お試しください${NC}"
                fi
                ;;
            2)
                if check_dependencies; then
                    launch_app "gui" "GUI版" "ble_gui.py"
                else
                    echo -e "${RED}依存関係の問題を解決してから再度お試しください${NC}"
                fi
                ;;
            3)
                echo -e "${GREEN}🔍 デバイススキャンを実行します...${NC}"
                if [[ "$VENV_ACTIVATED" == "false" ]]; then
                    activate_venv
                fi
                python3 ble_scanner.py || echo -e "${RED}スキャンに失敗しました${NC}"
                ;;
            4)
                show_info
                ;;
            5)
                check_dependencies || echo -e "${YELLOW}問題を解決してから再度お試しください${NC}"
                ;;
            q|Q)
                echo -e "${GREEN}👋 ありがとうございました!${NC}"
                exit 0
                ;;
            *)
                echo -e "${RED}無効な選択です。1-5 または q を入力してください。${NC}"
                ;;
        esac
        
        echo ""
        echo -e "${YELLOW}Enterキーでメニューに戻ります...${NC}"
        read -r
        clear
    done
}

# スクリプト実行
main "$@"
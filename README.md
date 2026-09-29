# Nuvoton USB HID ISP Programmer (Cross-Platform)

新唐科技 (Nuvoton) 微控制器 USB HID 介面韌體更新工具（In-System Programming, ISP）。

本專案支援 **Windows** 與 **Linux** 跨平台以 **CMake** 統一建置，全面升級至現代化 **`libusb-1.0`** API，具備傳輸穩定、速度快、隨插即用（免安裝專屬驅動）等優勢。

---

## 📌 目錄 (Table of Contents)

- [主要特色 (Features)](#-主要特色-features)
- [支援硬體與預設 ID (Hardware Support)](#-支援硬體與預設-id-hardware-support)
- [環境需求與相依性 (Prerequisites)](#-環境需求與相依性-prerequisites)
  - [Linux 環境](#1-linux-環境-ubuntudebianraspberry-pi)
  - [Windows 環境](#2-windows-環境)
- [編譯與建置 (Build Instructions)](#-編譯與建置-build-instructions)
  - [Linux 建置步驟](#1-linux-建置)
  - [Windows 建置步驟](#2-windows-建置)
- [使用說明 (Usage)](#-使用說明-usage)
  - [命令列語法](#命令列語法)
  - [常用範例](#常用範例)
- [測試工具 (Test Utility)](#-測試工具-test-utility)
- [常見問題排查 (Troubleshooting / FAQ)](#-常見問題排查-troubleshooting--faq)

---

## 🚀 主要特色 (Features)

1. **跨平台原生支援**：使用標準 CMake 建置系統，支援 Linux (GCC / Clang) 與 Windows (MSVC / MinGW)。
2. **升級至 libusb-1.0**：淘汰過時的 libusb-0.1，全面改用現代化 `libusb-1.0`，具備完善的逾時與會話管理機制。
3. **Windows 隨插即用 (Zero-Driver Setup)**：在 Windows 上利用 `libusb-1.0` 內建的 Windows HID 後端直接通訊，**完全不需要透過 Zadig 安裝 WinUSB 或 libusbK 驅動程式**，也不會破壞原有驅動。
4. **自動化靜態庫整合**：Windows 環境下透過 CMake `FetchContent` 自動下載並靜態編譯 `libusb-1.0`，無需手動尋找或配置外部 DLL。
5. **支援 `--jumpAP` 參數**：可彈性選擇在燒錄成功後保持在 LDROM 模式，或主動發送指令重啟並跳轉至 APROM 開機執行。
6. **優化傳輸速度**：移除了歷史版本的人工延遲，256KB 韌體約 12 秒內即可燒錄並校驗完畢（速率約 20 KB/s）。
7. **整合 Cppcheck 與 MISRA 規範檢驗**：Windows 環境下自動偵測 Cppcheck 與 `misra.json`，在每次 CMake 編譯前自動執行靜態分析。

---

## 🔌 支援硬體與預設 ID (Hardware Support)

- **目標晶片**：支援 Nuvoton 全系列具備 USB HID ISP 模式之 MCU（如 M3331、M2351、M480 系列等）。
- **預設 USB 連線參數**：
  - **Vendor ID (VID)**: `0x0416` (Nuvoton Technology Corp.)
  - **Product ID (PID)**: `0x3F00`（或 `0xA317`）
  - **端點 (Endpoints)**: OUT `0x02`, IN `0x81` (64 bytes/packet)
- **進入 ISP 模式**：
  晶片重置（Reset）或上電時，將硬體 ISP 偵測腳（例如 `PB12`）拉至低電位（按住 ISP 按鈕），即可進入 LDROM 的 USB ISP 模式。

---

## 🛠 環境需求與相依性 (Prerequisites)

### 1. Linux 環境 (Ubuntu/Debian/Raspberry Pi)

#### 安裝編譯工具與相依套件：
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libusb-1.0-0-dev
```

#### 設定 Linux USB 裝置權限 (udev rules)：
Linux 預設限制非 root 帳號存取 USB 原始裝置。為了讓一般使用者可以直接執行燒錄（免每次輸入 `sudo`），請建立 udev 規則：

```bash
sudo bash -c 'cat << "EOF" > /etc/udev/rules.d/99-nuvoton-isp.rules
# Nuvoton USB HID ISP Device
SUBSYSTEM=="usb", ATTRS{idVendor}=="0416", ATTRS{idProduct}=="3f00", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0416", ATTRS{idProduct}=="3f00", MODE="0666", GROUP="plugdev"
EOF'

# 重新載入 udev 規則
sudo udevadm control --reload-rules
sudo udevadm trigger
```

---

### 2. Windows 環境

1. **編譯器**：
   - [Visual Studio 2019 或 2022](https://visualstudio.microsoft.com/)（安裝時勾選 **「使用 C++ 的桌面開發」**），或安裝 MinGW-w64。
2. **CMake**：
   - 版本 $\ge$ 3.16（若安裝 Visual Studio 已內建 CMake）。
3. **驅動程式**：
   - **完全免安裝任何驅動**。插上 USB 後 Windows 會自動以內建 HID 裝置識別。
4. **外部庫**：
   - **免手動配置**。CMake 會自動透過網路拉取 `libusb-cmake` 並靜態打包至執行檔中。

---

## 🔨 編譯與建置 (Build Instructions)

### 1. Linux 建置

在專案根目錄下開啟終端機：

```bash
# 1. 建立建置目錄並產生 Makefile
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 2. 開始編譯
cmake --build build

# 編譯產物路徑：./build/linux_usb_isp_nuvoton/linux_usb_isp_nuvoton
```

---

### 2. Windows 建置

在專案根目錄下開啟 **PowerShell** 或 **Command Prompt**：

```powershell
# 1. 產生 Visual Studio 專案建置檔
cmake -B build

# 2. 開始編譯為 Release 版本 (編譯前會自動執行 Cppcheck + MISRA 靜態分析)
cmake --build build --config Release

# 編譯產物路徑：.\build\linux_usb_isp_nuvoton\Release\linux_usb_isp_nuvoton.exe
```

---

### 3. Cppcheck 與 MISRA 規範檢驗 (靜態代碼分析)

若系統安裝了 [Cppcheck](https://cppcheck.sourceforge.io/)（預設安裝路徑 `C:\Program Files\Cppcheck`）且專案目錄中存在 `misra.json`，CMake 會自動啟用預編譯檢查：

- **自動檢查**：每次執行 `cmake --build build` 編譯主程式前，MSBuild 會自動觸發 `run_cppcheck` 目標，並依照 `misra.json` 的規則定義檢驗程式碼。
- **單獨執行靜態分析**：
  ```powershell
  cmake --build build --target run_cppcheck
  ```
- **關閉 Cppcheck 分析**：若不想在編譯時執行 Cppcheck，可在 CMake 配置時關閉：
  ```powershell
  cmake -B build -DENABLE_CPPCHECK=OFF
  ```

---

## 💻 使用說明 (Usage)

### 命令列語法

```bash
linux_usb_isp_nuvoton <firmware_file.bin> [--jumpAP] [PID_HEX] [VID_HEX]
```

### 參數說明

| 參數 | 說明 | 必填 / 選填 |
| :--- | :--- | :--- |
| `<firmware_file.bin>` | 欲燒錄進 APROM 的韌體二進位檔案路徑 | **必填** |
| `--jumpAP` | 燒錄成功後發送重啟指令，讓 MCU 重開機並由 **APROM** 啟動 | 選填（預設維持在 LDROM） |
| `[PID_HEX]` | 目標裝置 USB Product ID（十六進位，預設為 `3F00`） | 選填 |
| `[VID_HEX]` | 目標裝置 USB Vendor ID（十六進位，預設為 `0416`） | 選填 |

---

### 常用範例

#### 範例 1：標準燒錄（燒錄完維持在 LDROM ISP 模式）
```bash
# Linux
./build/linux_usb_isp_nuvoton/linux_usb_isp_nuvoton my_firmware.bin

# Windows
.\build\linux_usb_isp_nuvoton\Release\linux_usb_isp_nuvoton.exe my_firmware.bin
```

#### 範例 2：燒錄完成後立即跳轉至 APROM 開機執行
```bash
# Linux
./build/linux_usb_isp_nuvoton/linux_usb_isp_nuvoton my_firmware.bin --jumpAP

# Windows
.\build\linux_usb_isp_nuvoton\Release\linux_usb_isp_nuvoton.exe my_firmware.bin --jumpAP
```

#### 範例 3：指定自訂 PID / VID
```bash
# 指定 PID 為 3F00，VID 為 0416，並於燒錄完成後跳轉 APROM
linux_usb_isp_nuvoton my_firmware.bin --jumpAP 3F00 0416
```

---

## 🧪 測試工具 (Test Utility)

專案內附 Python 輔助工具 [`generate_test_bin.py`](generate_test_bin.py)，方便測試不同容量大小的燒錄情境：

```bash
# 產生 256KB 的測試檔案 (預設檔名為 test_256k.bin)
python generate_test_bin.py 256

# 產生指定大小與檔名 (例如 128KB)
python generate_test_bin.py 128 test_128k.bin
```

---

## ❓ 常見問題排查 (Troubleshooting / FAQ)

### Q1: 出現 `USB IO Card not found.` 錯誤？
- **檢查 MCU 狀態**：請確認 MCU 是否已拉低 ISP 偵測腳（如 `PB12`）並按重置鍵進入 LDROM。
- **檢查 Linux 權限**：請確認是否已設定上述的 [udev rules](#設定-linux-usb-裝置權限-udev-rules)；若尚未設定，可暫時先以 `sudo` 執行測試。
- **檢查 VID / PID**：在 Linux 下可使用 `lsusb`，Windows 可使用裝置管理員，確認是否有出現 `0416:3f00`。若硬體使用了不同的 PID/VID，請於命令列指定。

### Q2: 燒錄完成後 MCU 沒有自動開機執行？
- 這是正常預設行為。若希望燒錄完成後立即讓晶片開機執行新韌體，請在命令列加入 **`--jumpAP`** 參數。

### Q3: 出現 `APROM FILE OPEN FALSE`？
- 請確認輸入的 `.bin` 韌體路徑是否正確，以及檔案是否具備讀取權限。

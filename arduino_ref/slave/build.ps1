#!/usr/bin/env powershell
# Build and upload ZPlate WiFi firmware

Write-Host "╔═══════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  ZPlate - Build & Upload              ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════╝" -ForegroundColor Cyan

Write-Host "`n[1/3] Cleaning previous build..." -ForegroundColor Yellow
arduino-cli compile --clean --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc . | Out-Null

Write-Host "[2/3] Compiling WiFi streaming firmware..." -ForegroundColor Yellow
$compileOutput = arduino-cli compile --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc . 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ Compilation failed!" -ForegroundColor Red
    Write-Host $compileOutput
    exit 1
}
Write-Host "✓ Compilation successful" -ForegroundColor Green

Write-Host "[3/3] Uploading to ESP32-C6..." -ForegroundColor Yellow
$uploadOutput = arduino-cli upload -p COM10 --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc . 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ Upload failed!" -ForegroundColor Red
    Write-Host $uploadOutput
    exit 1
}
Write-Host "✓ Upload successful`n" -ForegroundColor Green

Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host "📱 Connect to WiFi: ZPlate" -ForegroundColor White
Write-Host "   Password: zplate2026" -ForegroundColor White
Write-Host "" -ForegroundColor White
Write-Host "📡 ESP32 IP: 192.168.4.1" -ForegroundColor White
Write-Host "" -ForegroundColor White
Write-Host "▶  Run: python python\wifi_receiver_gui.py" -ForegroundColor White
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

Write-Host "Starting serial monitor in 3 seconds..." -ForegroundColor Yellow
Start-Sleep -Seconds 3
arduino-cli monitor -p COM10 -c baudrate=921600

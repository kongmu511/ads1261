#!/usr/bin/env powershell
# ZPlate Automated System Test
# Tests firmware, communication, and ADC readings

Write-Host "`n=== ZPlate Automated System Test ===" -ForegroundColor Cyan
Write-Host "Testing ESP32 communication and ADC readings`n"

# Configuration
$ESP32_IP = "192.168.4.1"
$LOCAL_IP = "192.168.4.100"
$UDP_PORT = 5555
$CALIB_FACTOR = 2231.19

# Test 1: Send START command
Write-Host "[1/4] Sending START command to ESP32..." -ForegroundColor Yellow
try {
    $cmdSocket = New-Object System.Net.Sockets.UdpClient
    $startCmd = [byte]0x53  # 'S'
    $cmdSocket.Send(@($startCmd), 1, $ESP32_IP, $UDP_PORT) | Out-Null
    $cmdSocket.Close()
    Write-Host "  OK: START command sent" -ForegroundColor Green
    Start-Sleep -Seconds 1
} catch {
    Write-Host "  ✗ Failed to send command: $_" -ForegroundColor Red
    exit 1
}

# Test 2: Receive data packets
Write-Host "[2/4] Listening for data packets on ${LOCAL_IP}:${UDP_PORT}..." -ForegroundColor Yellow
try {
    $udpClient = New-Object System.Net.Sockets.UdpClient
    $udpClient.Client.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket, 
                                      [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true)
    $endpoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Parse($LOCAL_IP), $UDP_PORT)
    $udpClient.Client.Bind($endpoint)
    $udpClient.Client.ReceiveTimeout = 3000
    
    Write-Host "  Listening for 3 seconds..."
    
    $packets = @()
    $startTime = Get-Date
    
    while (((Get-Date) - $startTime).TotalSeconds -lt 3) {
        try {
            $remoteEP = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
            [byte[]]$data = New-Object byte[] 64
            $count = $udpClient.Client.ReceiveFrom($data, [ref]$remoteEP)
            
            if ($data.Length -eq 20) {
                # Parse packet: uint32 timestamp + 4x int32 channels
                $timestamp = [BitConverter]::ToUInt32($data, 0)
                $ch1 = [BitConverter]::ToInt32($data, 4)
                $ch2 = [BitConverter]::ToInt32($data, 8)
                $ch3 = [BitConverter]::ToInt32($data, 12)
                $ch4 = [BitConverter]::ToInt32($data, 16)
                
                $packets += @{
                    Timestamp = $timestamp
                    CH1 = $ch1
                    CH2 = $ch2
                    CH3 = $ch3
                    CH4 = $ch4
                }
            }
        } catch [System.Net.Sockets.SocketException] {
            # Timeout - normal
        }
    }
    
    $udpClient.Close()
    
    if ($packets.Count -eq 0) {
        Write-Host "  ✗ No packets received!" -ForegroundColor Red
        Write-Host "    Possible causes:" -ForegroundColor Yellow
        Write-Host "    - ESP32 not streaming (check serial monitor)" -ForegroundColor Yellow
        Write-Host "    - Firewall blocking UDP port 5555" -ForegroundColor Yellow
        Write-Host "    - IP mismatch (ESP32 sending to wrong IP)" -ForegroundColor Yellow
        exit 1
    }
    
    Write-Host "  OK: Received $($packets.Count) packets" -ForegroundColor Green
    
} catch {
    Write-Host "  ✗ Failed to receive data: $_" -ForegroundColor Red
    exit 1
}

# Test 3: Analyze ADC data
Write-Host "[3/4] Analyzing ADC readings..." -ForegroundColor Yellow

$avgCH1 = ($packets | Measure-Object -Property CH1 -Average).Average
$avgCH2 = ($packets | Measure-Object -Property CH2 -Average).Average
$avgCH3 = ($packets | Measure-Object -Property CH3 -Average).Average
$avgCH4 = ($packets | Measure-Object -Property CH4 -Average).Average

# Convert to Newtons
$ch1_N = $avgCH1 / $CALIB_FACTOR
$ch2_N = $avgCH2 / $CALIB_FACTOR
$ch3_N = $avgCH3 / $CALIB_FACTOR
$ch4_N = $avgCH4 / $CALIB_FACTOR

Write-Host "`n  Average ADC Values (Raw):"
Write-Host "    CH1: $([math]::Round($avgCH1, 0))" -ForegroundColor Cyan
Write-Host "    CH2: $([math]::Round($avgCH2, 0))" -ForegroundColor Cyan
Write-Host "    CH3: $([math]::Round($avgCH3, 0))" -ForegroundColor Cyan
Write-Host "    CH4: $([math]::Round($avgCH4, 0))" -ForegroundColor Cyan

Write-Host "`n  Force Values (Newtons):"
Write-Host "    CH1: $([math]::Round($ch1_N, 2)) N" -ForegroundColor Green
Write-Host "    CH2: $([math]::Round($ch2_N, 2)) N" -ForegroundColor Green
Write-Host "    CH3: $([math]::Round($ch3_N, 2)) N" -ForegroundColor Green
Write-Host "    CH4: $([math]::Round($ch4_N, 2)) N" -ForegroundColor Green

# Check if one channel is different (loaded)
$allValues = @($avgCH1, $avgCH2, $avgCH3, $avgCH4)
$minVal = ($allValues | Measure-Object -Minimum).Minimum
$maxVal = ($allValues | Measure-Object -Maximum).Maximum
$range = $maxVal - $minVal

Write-Host "`n  Value Range: $([math]::Round($range, 0)) ADC counts" -ForegroundColor Cyan

if ($range -gt 1000) {
    Write-Host "  OK: Load cells showing different values (one is loaded)" -ForegroundColor Green
    
    # Find which channel has the different value
    $sorted = $allValues | Sort-Object
    if ([math]::Abs($avgCH1 - $sorted[0]) -lt 100 -or [math]::Abs($avgCH1 - $sorted[3]) -gt 100) {
        Write-Host "    → CH1 appears to be the loaded cell" -ForegroundColor Yellow
    }
    if ([math]::Abs($avgCH2 - $sorted[0]) -lt 100 -or [math]::Abs($avgCH2 - $sorted[3]) -gt 100) {
        Write-Host "    → CH2 appears to be the loaded cell" -ForegroundColor Yellow
    }
    if ([math]::Abs($avgCH3 - $sorted[0]) -lt 100 -or [math]::Abs($avgCH3 - $sorted[3]) -gt 100) {
        Write-Host "    → CH3 appears to be the loaded cell" -ForegroundColor Yellow
    }
    if ([math]::Abs($avgCH4 - $sorted[0]) -lt 100 -or [math]::Abs($avgCH4 - $sorted[3]) -gt 100) {
        Write-Host "    → CH4 appears to be the loaded cell" -ForegroundColor Yellow
    }
    
} elseif ($range -lt 10) {
    Write-Host "  WARNING: All channels showing identical values" -ForegroundColor Yellow
    Write-Host "    ADC may not be reading properly" -ForegroundColor Yellow
} else {
    Write-Host "  WARNING: Small variation detected (noise or light load)" -ForegroundColor Yellow
}

# Test 4: Calculate data rate
Write-Host "`n[4/4] Calculating sample rate..." -ForegroundColor Yellow
$duration = 3.0
$sampleRate = $packets.Count / $duration
Write-Host "  Sample rate: $([math]::Round($sampleRate, 1)) Hz" -ForegroundColor Cyan

if ($sampleRate -gt 900) {
    Write-Host "  OK: Sample rate OK (target: 1000 Hz)" -ForegroundColor Green
} elseif ($sampleRate -gt 500) {
    Write-Host "  WARNING: Sample rate lower than expected" -ForegroundColor Yellow
} else {
    Write-Host "  ✗ Sample rate too low" -ForegroundColor Red
}

# Send STOP command
Write-Host "`nSending STOP command..." -ForegroundColor Yellow
try {
    $cmdSocket = New-Object System.Net.Sockets.UdpClient
    $stopCmd = [byte]0x45  # 'E'
    $cmdSocket.Send(@($stopCmd), 1, $ESP32_IP, $UDP_PORT) | Out-Null
    $cmdSocket.Close()
    Write-Host "OK: STOP command sent" -ForegroundColor Green
} catch {
    Write-Host "WARNING: Failed to send STOP: $_" -ForegroundColor Yellow
}

# Summary
Write-Host "`n=== Test Summary ===" -ForegroundColor Cyan
Write-Host "Communication: Working" -ForegroundColor Green
Write-Host "Data Reception: $($packets.Count) packets in 3 seconds" -ForegroundColor Green

$rateColor = if ($sampleRate -gt 900) { "Green" } else { "Yellow" }
Write-Host "Sample Rate: $([math]::Round($sampleRate, 1)) Hz" -ForegroundColor $rateColor

$loadStatus = if ($range -gt 1000) { "Detecting load" } else { "Check ADC needed" }
$loadColor = if ($range -gt 1000) { "Green" } else { "Yellow" }
Write-Host "Load Cell Status: $loadStatus" -ForegroundColor $loadColor
Write-Host ""

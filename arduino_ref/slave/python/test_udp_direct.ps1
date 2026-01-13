# Direct UDP receiver in PowerShell - no Python needed
# Receives data from ESP32 and displays it

$udpClient = New-Object System.Net.Sockets.UdpClient
$udpClient.Client.ReceiveTimeout = 10000
$localEndPoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Parse("192.168.4.100"), 5555)
$udpClient.Client.Bind($localEndPoint)

Write-Host "Listening on 192.168.4.100:5555 for 10 seconds..." -ForegroundColor Green
Write-Host "Press Ctrl+C to stop"

# Send START command first
$cmdClient = New-Object System.Net.Sockets.UdpClient
$startCmd = [byte[]]@(0x53)  # 'S'
$cmdClient.Send($startCmd, $startCmd.Length, "192.168.4.1", 5555) | Out-Null
$cmdClient.Close()
Write-Host "START command sent to ESP32" -ForegroundColor Yellow

$packetCount = 0
$startTime = Get-Date

try {
    while (((Get-Date) - $startTime).TotalSeconds -lt 10) {
        try {
            $remoteEndPoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
            $receivedBytes = $udpClient.Receive([ref]$remoteEndPoint)
            $packetCount++
            
            Write-Host "Packet $packetCount`: $($receivedBytes.Length) bytes from $($remoteEndPoint.Address)" -ForegroundColor Cyan
            
            if ($receivedBytes.Length -eq 20) {
                # Parse data packet: uint32 timestamp + 4x int32 channels
                $timestamp = [BitConverter]::ToUInt32($receivedBytes, 0)
                $ch1 = [BitConverter]::ToInt32($receivedBytes, 4)
                $ch2 = [BitConverter]::ToInt32($receivedBytes, 8)
                $ch3 = [BitConverter]::ToInt32($receivedBytes, 12)
                $ch4 = [BitConverter]::ToInt32($receivedBytes, 16)
                Write-Host "  Time: $timestamp us, CH1: $ch1, CH2: $ch2, CH3: $ch3, CH4: $ch4"
            } else {
                Write-Host "  Raw: $([BitConverter]::ToString($receivedBytes))"
            }
        }
        catch [System.Net.Sockets.SocketException] {
            # Timeout - keep trying
        }
    }
}
finally {
    # Send STOP command
    $cmdClient = New-Object System.Net.Sockets.UdpClient
    $stopCmd = [byte[]]@(0x45)  # 'E'
    $cmdClient.Send($stopCmd, $stopCmd.Length, "192.168.4.1", 5555) | Out-Null
    $cmdClient.Close()
    
    $udpClient.Close()
    Write-Host "`nSTOP command sent" -ForegroundColor Yellow
    Write-Host "Total packets received: $packetCount" -ForegroundColor Green
    
    if ($packetCount -eq 0) {
        Write-Host "`nTROUBLESHOOTING:" -ForegroundColor Red
        Write-Host "1. Check Windows Firewall (allow UDP 5555)"
        Write-Host "2. Verify IP is 192.168.4.100 (run: ipconfig)"
        Write-Host "3. Check ESP32 serial monitor for '▶ Streaming STARTED'"
    }
}

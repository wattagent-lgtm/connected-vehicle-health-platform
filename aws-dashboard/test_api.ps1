[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ApiBaseUrl,
    [string]$DeviceId = "CAR-01-OBD"
)

$ErrorActionPreference = "Stop"
$Base = $ApiBaseUrl.TrimEnd("/")
$EncodedDevice = [uri]::EscapeDataString($DeviceId)

Write-Host "Health endpoint" -ForegroundColor Cyan
Invoke-RestMethod "$Base/api/cars/$EncodedDevice/health" -TimeoutSec 15 |
    Format-List

Write-Host "Latest endpoint" -ForegroundColor Cyan
Invoke-RestMethod "$Base/api/cars/$EncodedDevice/latest" -TimeoutSec 15 |
    Format-List

Write-Host "History endpoint (last 60 minutes)" -ForegroundColor Cyan
$History = Invoke-RestMethod "$Base/api/cars/$EncodedDevice/history?minutes=60" -TimeoutSec 15
Write-Host "History records: $($History.count)"

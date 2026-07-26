[CmdletBinding()]
param(
    [string]$AwsProfile = "",
    [switch]$NoConfirm
)

$ErrorActionPreference = "Stop"
$env:SAM_CLI_TELEMETRY = "0"
$ProjectDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ProjectDirectory

if (-not (Get-Command aws -ErrorAction SilentlyContinue)) {
    throw "AWS CLI is not installed or is not available in PATH."
}

if (-not (Get-Command sam -ErrorAction SilentlyContinue)) {
    throw "AWS SAM CLI is not installed or is not available in PATH."
}

$IdentityArguments = @("sts", "get-caller-identity")
if ($AwsProfile) {
    $IdentityArguments += @("--profile", $AwsProfile)
}

Write-Host "Checking AWS identity..." -ForegroundColor Cyan
& aws @IdentityArguments
if ($LASTEXITCODE -ne 0) {
    throw "AWS authentication failed. Configure AWS CLI credentials before deployment."
}

Write-Host "Validating SAM template..." -ForegroundColor Cyan
sam validate --lint
if ($LASTEXITCODE -ne 0) {
    throw "SAM template validation failed."
}

Write-Host "Building application..." -ForegroundColor Cyan
sam build
if ($LASTEXITCODE -ne 0) {
    throw "SAM build failed."
}

$DeployArguments = @("deploy")
if ($AwsProfile) {
    $DeployArguments += @("--profile", $AwsProfile)
}
if ($NoConfirm) {
    $DeployArguments += "--no-confirm-changeset"
}

Write-Host "Deploying car-health-dashboard..." -ForegroundColor Cyan
& sam @DeployArguments
if ($LASTEXITCODE -ne 0) {
    throw "SAM deployment failed."
}

Write-Host "Deployment complete. CloudFormation outputs:" -ForegroundColor Green
$OutputArguments = @(
    "cloudformation",
    "describe-stacks",
    "--stack-name",
    "car-health-dashboard",
    "--region",
    "ap-southeast-1",
    "--query",
    "Stacks[0].Outputs",
    "--output",
    "table"
)
if ($AwsProfile) {
    $OutputArguments += @("--profile", $AwsProfile)
}
& aws @OutputArguments

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$depsRoot = Join-Path $repoRoot '.deps'
$dxcRoot = Join-Path $depsRoot 'dxc'

New-Item -ItemType Directory -Force -Path $depsRoot | Out-Null

function Find-X64Dxc([string]$root) {
    if (-not (Test-Path $root)) { return $null }
    return Get-ChildItem -Path $root -Filter dxc.exe -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '[\\/]bin[\\/]x64[\\/]dxc\.exe$' } |
        Select-Object -First 1
}

$existing = Find-X64Dxc $dxcRoot
if ($existing) {
    Write-Host "DXC x64 already installed: $($existing.FullName)"
    exit 0
}

Write-Host 'Fetching the latest official DirectX Shader Compiler Windows package...'
$headers = @{
    'User-Agent' = 'RuneForge-Realms-build'
    'Accept' = 'application/vnd.github+json'
}
if ($env:GITHUB_TOKEN) {
    $headers['Authorization'] = "Bearer $env:GITHUB_TOKEN"
} elseif ($env:GH_TOKEN) {
    $headers['Authorization'] = "Bearer $env:GH_TOKEN"
}

$release = Invoke-RestMethod -Headers $headers -Uri 'https://api.github.com/repos/microsoft/DirectXShaderCompiler/releases/latest'
$asset = $release.assets |
    Where-Object { $_.name -match '^dxc_.*\.zip$' -and $_.name -notmatch '(pdb|symbols)' } |
    Select-Object -First 1

if (-not $asset) {
    throw 'Could not locate a Windows DXC zip in the latest official DirectXShaderCompiler release.'
}

$tempZip = Join-Path $depsRoot 'dxc.zip'
$tempExtract = Join-Path $depsRoot 'dxc-extract'
Remove-Item $tempZip -Force -ErrorAction SilentlyContinue
Remove-Item $tempExtract -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $dxcRoot -Recurse -Force -ErrorAction SilentlyContinue

Invoke-WebRequest -Headers $headers -Uri $asset.browser_download_url -OutFile $tempZip
Expand-Archive -LiteralPath $tempZip -DestinationPath $tempExtract -Force

New-Item -ItemType Directory -Force -Path $dxcRoot | Out-Null
Copy-Item -Path (Join-Path $tempExtract '*') -Destination $dxcRoot -Recurse -Force

Remove-Item $tempZip -Force
Remove-Item $tempExtract -Recurse -Force

$dxc = Find-X64Dxc $dxcRoot
if (-not $dxc) {
    throw 'DXC package downloaded, but bin/x64/dxc.exe was not found after extraction.'
}

Write-Host "DXC x64 installed: $($dxc.FullName)"

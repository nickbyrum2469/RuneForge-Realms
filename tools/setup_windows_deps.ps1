$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$depsRoot = Join-Path $repoRoot '.deps'
$dxcRoot = Join-Path $depsRoot 'dxc'

New-Item -ItemType Directory -Force -Path $depsRoot | Out-Null

$existing = Get-ChildItem -Path $dxcRoot -Filter dxc.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if ($existing) {
    Write-Host "DXC already installed: $($existing.FullName)"
    exit 0
}

Write-Host 'Fetching the latest official DirectX Shader Compiler Windows package...'
$headers = @{
    'User-Agent' = 'RuneForge-Realms-build'
    'Accept' = 'application/vnd.github+json'
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

$dxc = Get-ChildItem -Path $dxcRoot -Filter dxc.exe -Recurse | Select-Object -First 1
if (-not $dxc) {
    throw 'DXC package downloaded, but dxc.exe was not found after extraction.'
}

Write-Host "DXC installed: $($dxc.FullName)"

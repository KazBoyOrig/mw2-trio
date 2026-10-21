param([string]$Compiler, [string]$Python = 'python')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'tools/Compiler.ps1')
$cc = Get-TrioCompiler -Compiler $Compiler
$outputDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$systemWinmm = Join-Path $env:SystemRoot 'SysWOW64/winmm.dll'
if (-not (Test-Path -LiteralPath $systemWinmm -PathType Leaf)) {
    throw 'A 64-bit Windows installation with the x86 system winmm.dll is required.'
}
& $Python -I (Join-Path $PSScriptRoot 'tools/gen_winmm32_proxy.py') $systemWinmm
if ($LASTEXITCODE -ne 0) { throw 'Export generation failed' }
$sources = @('bootstrap.c','pure_sha256.c','startup184.c','storage184.c','scripts184.c',
             'cache184.c','control184.c','net184.c','runtime184.c','party184.c','connect184.c','netopen184.c','qa_identity184.c','reliable184.c','service184.c','backend184.c',
             'peer_scope184.c','peer_transport184.c','winmm32_stubs.S','winmm32.def') |
    ForEach-Object { Join-Path $PSScriptRoot ('src/' + $_) }
$flags = @('-std=c11','-Wall','-Wextra','-Werror','-O2','-ffreestanding','-fno-builtin','-static-libgcc')
& $cc @flags -shared -o (Join-Path $outputDir 'winmm.dll') @sources -lws2_32 -liphlpapi
if ($LASTEXITCODE -ne 0) { throw 'x86 proxy build failed' }
& $cc @flags -municode -o (Join-Path $outputDir 'bootstrap-helper.exe') (Join-Path $PSScriptRoot 'src/helper.c')
if ($LASTEXITCODE -ne 0) { throw 'Read-only bootstrap helper build failed' }
Get-FileHash -LiteralPath (Join-Path $outputDir 'winmm.dll'),(Join-Path $outputDir 'bootstrap-helper.exe') -Algorithm SHA256
Write-Output 'Built source-only prototype 0.0.1-dev. No files were installed into a game.'

param([string]$Compiler, [string]$Python = 'python', [string]$GameExe)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $projectRoot 'tools/Compiler.ps1')
$cc = Get-TrioCompiler -Compiler $Compiler
& (Join-Path $projectRoot 'build.ps1') -Compiler $cc -Python $Python
$arguments = @('-I', (Join-Path $PSScriptRoot 'run_tests.py'), '--compiler', $cc)
if ($GameExe) { $arguments += @('--game-exe', (Get-Item -LiteralPath $GameExe).FullName) }
& $Python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Own-process helper test suite failed. See build/tests/.' }

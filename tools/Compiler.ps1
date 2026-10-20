function Get-TrioCompiler {
    param([string]$Compiler)
    if (-not $Compiler) { $Compiler = $env:MW2TRIO_CC }
    if (-not $Compiler) {
        $found = Get-Command 'i686-w64-mingw32-clang.exe' -ErrorAction SilentlyContinue
        if ($found) { $Compiler = $found.Source }
    }
    if (-not $Compiler -or -not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
        throw 'Set -Compiler or MW2TRIO_CC to the absolute path of LLVM-MinGW i686-w64-mingw32-clang.exe. See README.md.'
    }
    return (Get-Item -LiteralPath $Compiler).FullName
}

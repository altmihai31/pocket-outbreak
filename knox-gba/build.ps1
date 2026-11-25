# Builds the ROM from Windows PowerShell through devkitPro's MSYS2 shell.
# Butano's makefiles cannot handle spaces in paths ("Project GBAzoid"), so the parent folder is mapped
# to a temporary drive letter with subst for the duration of the build.
#
#   .\build.ps1                 knox.gba
#   .\build.ps1 PROFILE=1       knox_profile.gba
#   .\build.ps1 BENCH=1         knox_bench.gba
#   .\build.ps1 clean

param([Parameter(ValueFromRemainingArguments = $true)] [string[]] $MakeArgs)

$ErrorActionPreference = 'Stop'
$devkitPro = 'C:\devkitPro'
$bash = Join-Path $devkitPro 'msys2\usr\bin\bash.exe'

if (-not (Test-Path $bash)) {
    throw "devkitPro not found at $devkitPro (install it with the GBA Development group)."
}

$python = (Get-Command python -ErrorAction Stop).Source
$projectName = Split-Path $PSScriptRoot -Leaf
$parent = Split-Path $PSScriptRoot -Parent

$used = (Get-PSDrive -PSProvider FileSystem).Name
$letter = 'KXYZWVUT'.ToCharArray() | Where-Object { $used -notcontains [string]$_ } | Select-Object -First 1

if (-not $letter) {
    throw 'No free drive letter for subst.'
}

subst "$($letter):" "$parent"

try {
    $msysPython = '/' + $python.Substring(0, 1).ToLower() + $python.Substring(2).Replace('\', '/')
    $makeArgsText = ($MakeArgs -join ' ')
    $script = "export APPDATA='$env:APPDATA' DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM; " +
              "export PATH=`$DEVKITARM/bin:`$DEVKITPRO/tools/bin:`$PATH; " +
              "cd /$($letter.ToString().ToLower())/$projectName && make -j8 PYTHON='$msysPython' $makeArgsText"
    & $bash -lc $script

    if ($LASTEXITCODE -ne 0) {
        throw "make failed with exit code $LASTEXITCODE"
    }
}
finally {
    subst "$($letter):" /D
}

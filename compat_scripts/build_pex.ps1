# compat_scripts のスクリプトをコンパイルして .pex を作る。
#
# 1) 新規スクリプト(SKSE / UI / Input / StringUtil)は、compat_scripts の .psc をそのままコンパイルする。
# 2) バニラのスクリプト(Form / Game / Utility / Math)は、バニラの .psc をコピーして、
#    additions\<名前>.txt(自作の最小限の宣言)を末尾に足してからコンパイルする。
#    バニラの .psc は、このリポジトリに含めない(Creation Kit 付属のものを使う)。
#
# 使い方:
#   .\build_pex.ps1 -Compiler "<...>\Papyrus Compiler\PapyrusCompiler.exe" `
#                   -VanillaSource "<...>\Data\Source\Scripts" -Out "<出力先>"

param(
    [Parameter(Mandatory = $true)] [string] $Compiler,
    [Parameter(Mandatory = $true)] [string] $VanillaSource,
    [Parameter(Mandatory = $true)] [string] $Out
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$startDir = Get-Location
$work = Join-Path ([IO.Path]::GetTempPath()) ("skyshim_psc_" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $work, $Out | Out-Null

try {
    # バニラ + 追加宣言(UTF-8、BOM なし)
    foreach ($name in 'Form', 'Game', 'Utility', 'Math') {
        $vanilla = Join-Path $VanillaSource "$name.psc"
        if (-not (Test-Path $vanilla)) { throw "バニラのソースが見つかりません: $vanilla" }
        $text = [IO.File]::ReadAllText($vanilla) + [IO.File]::ReadAllText((Join-Path $here "additions\$name.txt"))
        [IO.File]::WriteAllText((Join-Path $work "$name.psc"), $text, (New-Object Text.UTF8Encoding($false)))
    }
    # 新規スクリプト
    foreach ($name in 'SKSE', 'UI', 'Input', 'StringUtil') {
        Copy-Item (Join-Path $here "$name.psc") $work
    }

    # work を先頭に置く(追加済みの Form などを優先)。バニラの残りは、その後ろ。
    # コンパイラは、実行時の作業フォルダの影響を受けることがあるため、バニラのソースのフォルダを作業フォルダにする。
    Set-Location $VanillaSource
    $import = "$work;$VanillaSource"
    $failed = 0
    foreach ($name in 'SKSE', 'UI', 'Input', 'StringUtil', 'Form', 'Game', 'Utility', 'Math') {
        & $Compiler (Join-Path $work "$name.psc") "-f=TESV_Papyrus_Flags.flg" "-i=$import" "-o=$Out" | Out-Null
        $pex = Join-Path $Out "$name.pex"
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $pex)) { Write-Host "NG  $name" -ForegroundColor Red; $failed++ }
        else { Write-Host ("OK  {0}.pex ({1} bytes)" -f $name, (Get-Item $pex).Length) }
    }
    if ($failed -gt 0) { throw "$failed 個のスクリプトのコンパイルに失敗しました" }
}
finally {
    Set-Location $startDir
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}

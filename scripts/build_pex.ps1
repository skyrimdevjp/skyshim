# scripts のスクリプトをコンパイルして .pex を作る。
#
# 1) 新規スクリプト(SKSE / UI / Input / StringUtil / EquipSlot)は、scripts の .psc をそのままコンパイルする。
# 2) バニラのスクリプト(Form / Game / Utility / Math / Actor / Armor / Weapon / Spell)は、バニラの .psc をコピーして、
#    additions\<名前>.txt(自作の最小限の宣言)を末尾に足してからコンパイルする。
#    バニラの .psc は、このリポジトリに含めない(Creation Kit 付属のものを使う)。
#
# 使い方:
#   .\build_pex.ps1 -Compiler "<...>\Papyrus Compiler\PapyrusCompiler.exe" `
#                   -VanillaSource "<...>\Data\Source\Scripts" -Out "<出力先>"

param(
    [Parameter(Mandatory = $true)] [string] $Compiler,
    [Parameter(Mandatory = $true)] [string] $VanillaSource,
    [Parameter(Mandatory = $true)] [string] $Out,
    # 指定すると、SkyUI のスクリプト(sourcescripts)を、この追加宣言だけでコンパイルして、足りない関数が無いか確認する。
    [string] $SkyUISource = ""
)

$ErrorActionPreference = 'Stop'

# Papyrus のコンパイラは、日本語(UTF-8)のコメントを別の文字コードとして読み、直後の宣言を飲み込んでしまう。
# このリポジトリのソースのコメントは日本語なので、コンパイル用の一時コピーからは、コメントだけの行を取り除く。
function Remove-Comments([string] $path) {
    $lines = [IO.File]::ReadAllLines($path, [Text.Encoding]::UTF8) | Where-Object { $_ -notmatch '^\s*;' }
    return ($lines -join "`r`n") + "`r`n"
}
$here = $PSScriptRoot
$startDir = Get-Location
$work = Join-Path ([IO.Path]::GetTempPath()) ("skyshim_psc_" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $work, $Out | Out-Null

try {
    # バニラ + 追加宣言(UTF-8、BOM なし)
    foreach ($name in 'Form', 'Game', 'Utility', 'Math', 'Actor', 'Armor', 'Weapon', 'Spell') {
        $vanilla = Join-Path $VanillaSource "$name.psc"
        if (-not (Test-Path $vanilla)) { throw "バニラのソースが見つかりません: $vanilla" }
        $text = [IO.File]::ReadAllText($vanilla) + "`r`n" + (Remove-Comments (Join-Path $here "additions\$name.txt"))
        [IO.File]::WriteAllText((Join-Path $work "$name.psc"), $text, (New-Object Text.UTF8Encoding($false)))
    }
    # 新規スクリプト
    foreach ($name in 'SKSE', 'UI', 'Input', 'StringUtil', 'EquipSlot') {
        [IO.File]::WriteAllText((Join-Path $work "$name.psc"), (Remove-Comments (Join-Path $here "$name.psc")), (New-Object Text.UTF8Encoding($false)))
    }

    # work を先頭に置く(追加済みの Form などを優先)。バニラの残りは、その後ろ。
    # コンパイラは、作業フォルダにある同名の .psc を、指定したファイルより先に使う。
    # 追加宣言を足した Form などが優先されるように、work フォルダを作業フォルダにする。
    Set-Location $work
    $flags = Join-Path $VanillaSource "TESV_Papyrus_Flags.flg"
    $import = "$work;$VanillaSource"
    $failed = 0
    foreach ($name in 'SKSE', 'UI', 'Input', 'StringUtil', 'EquipSlot', 'Form', 'Game', 'Utility', 'Math', 'Actor', 'Armor', 'Weapon', 'Spell') {
        & $Compiler (Join-Path $work "$name.psc") "-f=$flags" "-i=$import" "-o=$Out" | Out-Null
        $pex = Join-Path $Out "$name.pex"
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $pex)) { Write-Host "NG  $name" -ForegroundColor Red; $failed++ }
        else { Write-Host ("OK  {0}.pex ({1} bytes)" -f $name, (Get-Item $pex).Length) }
    }
    if ($failed -gt 0) { throw "$failed 個のスクリプトのコンパイルに失敗しました" }

    # 追加した宣言が、できた .pex に入っているか確認する(バニラの版が使われると、入らない)。
    $missing = 0
    foreach ($name in 'Form', 'Game', 'Utility', 'Math', 'Actor', 'Armor', 'Weapon', 'Spell') {
        $bytes = [IO.File]::ReadAllBytes((Join-Path $Out "$name.pex"))
        $strings = [Text.Encoding]::GetEncoding(28591).GetString($bytes)
        $declared = [regex]::Matches((Remove-Comments (Join-Path $here "additions\$name.txt")), '(?im)^\s*(?:\w+(?:\[\])?\s+)?Function\s+(\w+)') | ForEach-Object { $_.Groups[1].Value }
        foreach ($fn in $declared) {
            if ($strings.IndexOf($fn, [StringComparison]::OrdinalIgnoreCase) -lt 0) { Write-Host "NG  $name.pex に $fn がありません" -ForegroundColor Red; $missing++ }
        }
    }
    if ($missing -gt 0) { throw "追加した宣言が $missing 個、.pex に入っていません" }
    Write-Host "追加した宣言は、すべて .pex に入っています。"

    # SkyUI のスクリプトで確認する。追加済みの Form などがある work フォルダを、作業フォルダにする
    # (作業フォルダにバニラの Form.psc があると、そちらが優先されてしまうため)。
    if ($SkyUISource -ne "") {
        Set-Location $work
        $check = Join-Path $work "skyui_check"
        New-Item -ItemType Directory -Force $check | Out-Null
        $bad = 0
        # コンパイラは、診断メッセージを標準エラー出力に書く。ここでは、それを実行時エラーとして扱わない。
        $ErrorActionPreference = 'Continue'
        foreach ($f in Get-ChildItem $SkyUISource -Filter *.psc) {
            $msg = & $Compiler $f.FullName "-f=$flags" "-i=$work;$SkyUISource;$VanillaSource" "-o=$check" 2>&1
            if (-not (Test-Path (Join-Path $check "$($f.BaseName).pex"))) {
                $bad++
                Write-Host "SkyUI NG  $($f.Name)" -ForegroundColor Red
                $msg | Where-Object { $_ -match "\(\d+,\d+\)" } | Select-Object -First 5 | ForEach-Object { Write-Host "    $_" }
            }
        }
        if ($bad -gt 0) { throw "SkyUI のスクリプト $bad 個がコンパイルできません(足りない SKSE の関数があります)" }
        Write-Host "SkyUI のスクリプトは、すべてコンパイルできました。"
    }
}
finally {
    Set-Location $startDir
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}

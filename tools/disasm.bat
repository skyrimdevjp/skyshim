@echo off
rem SkyrimSE.exe を逆アセンブルして、テキストに保存する(MSVC の dumpbin を使う)。
rem 288 MB ほどになり、数分かかる。クラッシュした関数の命令を読むときに使う。
rem
rem 使い方:  disasm.bat "<...>\SkyrimSE.exe" "<出力するテキスト>"
rem
rem 関数の先頭のアドレスが 0x289D30 なら、出力の中の「0000000140289D30:」の行から読む
rem (アドレス = 0x140000000 + RVA)。
if "%~2"=="" (
  echo usage: disasm.bat "SkyrimSE.exe" "output.txt"
  exit /b 1
)
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
dumpbin /disasm:nobytes "%~1" > "%~2"

Scriptname SKSE Hidden
; Skyshim: SkyUI が使う部分だけの宣言(署名の互換性のためだけに書いたもの)。
; 実装は Skyshim 側(src\papyrus\register.cpp)にある。

; SkyUI は、GetVersionRelease が 0 でないこと、必要な最小値(53)以上であることを確認する。
; Skyshim は、実際の SKSE を名乗らず、条件を満たす値を返す。
int Function GetVersion() global native
int Function GetVersionMinor() global native
int Function GetVersionBeta() global native
int Function GetVersionRelease() global native
int Function GetScriptVersionRelease() global native

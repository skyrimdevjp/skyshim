Scriptname SKSE Hidden
; SkyUICompat: SkyUI が使う部分だけの宣言(署名の互換性のためだけに書いたもの)。
; 実装は互換ランタイム側(src\papyrus\register.cpp)にある。

; SkyUI は、GetVersionRelease が 0 でないこと、必要な最小値(53)以上であることを確認する。
; 互換ランタイムは、実際の SKSE を名乗らず、条件を満たす値を返す。
int Function GetVersion() global native
int Function GetVersionMinor() global native
int Function GetVersionBeta() global native
int Function GetVersionRelease() global native
int Function GetScriptVersionRelease() global native

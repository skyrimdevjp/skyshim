Scriptname StringUtil Hidden
; Skyshim: SkyUI が使う部分だけの宣言(署名の互換性のためだけに書いたもの)。
; 実装は互換ランタイム側(src\papyrus\register.cpp)にある。長さと位置は、文字数ではなくバイト数で数える。

; 文字列の一部を返す。startIndex は開始位置、len は長さ(-1 なら末尾まで)。
string Function Substring(string str, int startIndex, int len = -1) global native
; 文字列の長さを返す。
int Function GetLength(string str) global native

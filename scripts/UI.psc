Scriptname UI Hidden
; Skyshim: SkyUI が使う部分だけの宣言(署名の互換性のためだけに書いたもの)。
; 実装は Skyshim 側(src\papyrus\ui_api.cpp)にある。
; menuName はメニュー名(例: "HUD Menu")、target は ActionScript の変数や関数のパス(例: "_root.xxx")。

; メニューが開いているか。
bool Function IsMenuOpen(string menuName) global native
; 文字入力中か(入力中はホットキーを無効にするための判定)。
bool Function IsTextInputEnabled() global native

; ActionScript の変数に値を書き込む(書き込みはゲームのメインスレッドで行われる)。
Function SetInt(string menuName, string target, int value) global native
Function SetFloat(string menuName, string target, float value) global native
Function SetString(string menuName, string target, string value) global native
; ActionScript の変数の値を読む。
int Function GetInt(string menuName, string target) global native
string Function GetString(string menuName, string target) global native

; ActionScript の関数を呼ぶ。引数は、なし、真偽値、整数、小数、文字列の 1 個。
Function Invoke(string menuName, string target) global native
Function InvokeBool(string menuName, string target, bool arg) global native
Function InvokeInt(string menuName, string target, int arg) global native
Function InvokeFloat(string menuName, string target, float arg) global native
Function InvokeString(string menuName, string target, string arg) global native
; 配列の各要素を、別々の引数として渡して、ActionScript の関数を呼ぶ。
Function InvokeIntA(string menuName, string target, int[] args) global native
Function InvokeFloatA(string menuName, string target, float[] args) global native
Function InvokeStringA(string menuName, string target, string[] args) global native

Scriptname UI Hidden
; SkyUICompat: signature-compatible declarations (SkyUI-used subset only).

bool Function IsMenuOpen(string menuName) global native
bool Function IsTextInputEnabled() global native

Function SetInt(string menuName, string target, int value) global native
Function SetFloat(string menuName, string target, float value) global native
Function SetString(string menuName, string target, string value) global native
int Function GetInt(string menuName, string target) global native
string Function GetString(string menuName, string target) global native

Function Invoke(string menuName, string target) global native
Function InvokeBool(string menuName, string target, bool arg) global native
Function InvokeInt(string menuName, string target, int arg) global native
Function InvokeFloat(string menuName, string target, float arg) global native
Function InvokeString(string menuName, string target, string arg) global native
Function InvokeIntA(string menuName, string target, int[] args) global native
Function InvokeFloatA(string menuName, string target, float[] args) global native
Function InvokeStringA(string menuName, string target, string[] args) global native

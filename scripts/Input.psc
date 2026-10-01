Scriptname Input Hidden
; Skyshim: SkyUI が使うのは GetMappedControl だけ(SKI_ConfigManager)。
; 実装: src\papyrus\equip_api.cpp(キー番号の変換表は src\events\events.cpp)。

; キー番号に割り当てられている操作の名前を返す。
string Function GetMappedControl(int keyCode) global native

Scriptname Input Hidden
; Skyshim: SkyUI が使うのは GetMappedControl だけ(SKI_ConfigManager)。
; 注意: 互換ランタイムには、この関数の実装がまだ無い。
; 実装するときは、SKSE の動作(省略できる引数の有無)を確認してから、署名を確定する。

; キー番号に割り当てられている操作の名前を返す。
string Function GetMappedControl(int keyCode) global native

# SkyUI が使う SKSE の機能の一覧(SKYUI_SKSE_SURFACE)

Phase 1 の集計。対象は SkyUI-Community(`b7a24a9`)。`source/scripts` と `source/actionscript` を grep して作った。

**注意: この集計は、不完全だった。** 末尾の「訂正(2026-10-01)」に、あとで分かった漏れと誤りをまとめた。
実装の状況は、`STATUS.md` を参照する。

## Papyrus(呼び出し箇所の数)

| API | 回数 | 備考 |
|---|---|---|
| Form.RegisterForModEvent | 28 | |
| Form.SendModEvent | 5 | |
| Form.UnregisterForModEvent | 1 | 必要 |
| Form.RegisterForMenu / UnregisterForMenu | 9 / 1 | |
| Form.RegisterForKey / UnregisterForKey | 2 / 2 | OnKeyDown を使う |
| Form.GetType | 6 | |
| SKSE.GetVersion / GetVersionMinor / GetVersionBeta / GetVersionRelease / GetScriptVersionRelease | 1 / 1 / 1 / 2 / 1 | |
| UI.Invoke / InvokeBool / InvokeInt / InvokeFloat / InvokeString | 7 / 13 / 3 / 4 / 11 | |
| UI.InvokeIntA / InvokeFloatA / InvokeStringA | 9 / 6 / 13 | |
| UI.SetInt / SetFloat / SetString | 3 / 2 / 2 | **SetFloat は、計画書(rev4)の一覧に無かった** |
| UI.GetInt / GetString | 6 / 1 | |
| UI.IsMenuOpen / IsTextInputEnabled | 3 / 1 | |
| Input.GetMappedControl | 1 | |
| StringUtil.Substring / GetLength | 1 / 1 | |
| Game.IsObjectFavorited | 1 | **SKSE が追加した Game の関数。計画書(rev4)の一覧に無かった** |

バニラにあって、作業が不要とした関数(**この判断の一部は誤り。訂正を参照**): `Utility.*INI*`、`Game.GetFormFromFile`、`Game.UsingGamepad`、`Utility.Wait*`。

SkyUI が使わないもの: `UI.SetBool` / `UI.GetBool`、上に挙げた以外の配列版の `Invoke*`。

## Scaleform の `skse.*`(呼び出し箇所の数)

- `SendModEvent` 34、`version` 12、`OpenMenu` 5、`ExtendData` 3、`AllowTextInput` 3、`Log` 2
- `plugins` 1(`skse.plugins.InventoryInjector` を読む。未定義でも落ちないこと)
- `StoreIndices` 1、`LoadIndices` 1、`StartRemapMode` 1、`GetLastKeycode` 1、`ShowOnMap` 1
- `RequestActivePlayerEffects` 1、`ForceContainerCategorization` 1、`ExtendAlchemyCategories` 1
- `EnableMapMenuMouseWheel` 1

計画書(rev4)には載っていたが、現行のソースでは使われていないもの: `GetMappedKey`、`GetLastControl`、`SetINISetting`、
`GetINISetting`、`CloseMenu`、`ExtendForm`。「必要になったら実装する」扱いに下げた。

## 訂正(2026-10-01)

SkyUI の全スクリプトを、バニラ + 自作の宣言だけでコンパイルして調べたところ(`scripts\build_pex.ps1 -SkyUISource ...`)、
上の集計に、次の誤りと漏れがあった。

### 誤り

| 項目 | 上の集計 | 実際 |
|---|---|---|
| `Utility.GetINIInt` / `GetINIFloat` | 「バニラにあるので作業不要」 | Skyrim 1.5.97 のバニラには無く、SKSE が追加している。未実装だと 0 を返し、SkyUI がエラーコード 4 を出す。**実装した** |

### 漏れ(grep で拾えなかったもの)

| 項目 | 内容 | 状況 |
|---|---|---|
| Math のビット演算 | `LeftShift`、`RightShift`、`LogicalAnd`、`LogicalOr`、`LogicalXor`、`LogicalNot`(`SKI_FavoritesManager` が使う) | 実装した |
| MOD の翻訳ファイルの読み込み | SKSE が、`Interface\Translations\<プラグイン名>_<言語>.txt` を、ゲームの翻訳テーブルに追加している。API の呼び出しではないため、grep で拾えなかった | 実装した(`src\translation.cpp`) |
| `EquipSlot`(型) | `SKI_FavoritesManager` が、`as EquipSlot` の型変換と、`Spell.GetEquipType()` の戻り値で使う | 型の宣言を追加した。ゲームが型として認識するかは、未確認 |
| `Spell.GetEquipType` | 呪文の装備スロットを返す | 実装した(ゲーム内では未確認) |
| `Weapon.GetWeaponType`、`Armor.GetSlotMask` | 武器の種類、防具の部位マスク | 実装した(ゲーム内では未確認) |
| `Form.GetName` | 名前を返す | 実装した(ゲーム内では未確認) |
| `Actor` の装備まわり | `GetWornForm`、`GetEquippedObject`、`EquipItemEx`、`UnequipItemEx`、`EquipItemById`、`GetEquippedItemId`、`GetWornItemId` | 実装した(ゲーム内では未確認)。アイテム ID は常に 0 |

### 漏れが見つかった理由と対策

- 漏れの多くは、SKSE が**バニラのクラス(`Actor`、`Weapon` など)に関数を足している**もので、呼び出しの書き方が、バニラの関数と見分けにくかった。
- 今後は、grep ではなく、**SkyUI のスクリプトを実際にコンパイルして**、足りない関数を調べる(`scripts\build_pex.ps1 -SkyUISource <SkyUI の source\scripts>`)。
  足りない関数があれば、コンパイルがエラーになる。

### 引き続き未実装のもの

- 拡張データ: `ExtendData`、`ForceContainerCategorization`、`ExtendAlchemyCategories`(ゲーム本体への手入れが必要)
- マップ: `ShowOnMap`、`EnableMapMenuMouseWheel`(ホイールのズームは、実装なしでも動いている)
- `GetLastControl`、`GetMappedKey`、`ExtendForm`(現行の SkyUI は使っていない)

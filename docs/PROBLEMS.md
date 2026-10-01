# SkyUI を SKSE なしで動かす作業の問題と原因

- 対象: `E:\work\work_skyrim\独自SKSE\skyshim`
- 目的: SKSE64 を入れずに SkyUI を動かす。計画書は `独自SKSE\SkyUI_SKSE_compat_runtime_plan_rev4.md`
- 検証環境: Skyrim Special Edition 1.5.97、Mod Organizer 2(以下 MO2)2.5.2、MO2 のプロファイル `Default` と `start`
- 進捗の記録: `独自SKSE\skyshim\STATUS.md`
- 記載日: 2026-09-30

## 0. 用語

| 用語 | 意味 |
|---|---|
| SKSE | Skyrim Script Extender。Skyrim の機能を拡張する外部ソフト。SkyUI は SKSE があることを前提に作られている |
| Papyrus | Skyrim のスクリプト言語 |
| ネイティブ関数 | Papyrus から呼べる、ゲーム本体側の C++ で実装された関数。SKSE はこれを追加している |
| Address Library | ゲーム本体の関数や変数の場所を、バージョンをまたいで調べるための対応表(`version-1-5-97-0.bin` など) |
| 仮想ファイルシステム | MO2 が、複数の MOD のファイルを 1 つの `Data` フォルダに重ねて見せる仕組み。実際のフォルダには何も書き込まない |
| ローダー | `skyshim_loader.exe`。Skyrim を一時停止した状態で起動し、Skyshim の DLL を読み込ませてから再開する |
| CommonLib | Skyrim 本体の型や関数を C++ から使うためのライブラリ(`third_party\CommonLibSSE-NG-MIT`) |

## 1. 正しい起動と確認の手順(まずここを見る)

### 1.1 ビルド

Visual Studio の環境を読み込んでから `cmake --build` を実行する。読み込まないと `cl.exe` などが見つからない。PowerShell での例:

```
cd "E:\work\work_skyrim\独自SKSE\skyshim"
cmd /c 'call "D:\vs2026\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cmake --build build-se 2>&1'
```

- 成果物: `build-se\skyshim.dll` と `build-se\skyshim_loader.exe`
- 出力に `vswhere.exe is not recognized` と出るが、ビルドには影響しない
- エディタ(clang)が `RE/Skyrim.h file not found` などの赤い診断を出すが、インクルードパスが設定されていないだけで、ビルドの成否とは無関係

### 1.2 MO2 への登録

「実行ファイルを編集」で次のように登録する。

| 項目 | 値 |
|---|---|
| Binary | `E:\work\work_skyrim\独自SKSE\skyshim\build-se\skyshim_loader.exe` |
| Start in | `E:\work\work_skyrim\skyrimSECK`(MO2 が使うゲームフォルダ。`ModOrganizer.ini` の `gamePath` と同じ値) |
| Arguments | 空欄 |

- 「Apply」を押してから「OK」で閉じる。Apply を押さないと保存されない
- 使うプロファイルで、次の MOD を有効にする: `0000(system)Address Library All in One`、`0001(system)SkyUI 5 2 SE`
- 実行先は「SKSE」ではなく、上の登録名で起動する

### 1.3 ログの場所と見方

| ファイル | 内容 |
|---|---|
| `C:\Users\kaiser\Documents\My Games\Skyrim Special Edition\Skyshim\skyshim.log` | Skyshim のログ。起動のたびに上書きされる |
| `独自SKSE\skyshim\build-se\loader.log` | ローダーが受け取った引数、作業フォルダ、起動する `SkyrimSE.exe` のパス |
| `C:\Users\kaiser\Documents\My Games\Skyrim Special Edition\Logs\Script\Papyrus.0.log` | Papyrus のログ。`Unbound native function` や `error:` が出る。ログを有効にした直近の起動が `.0`、その前が `.1` |

`skyshim.log` の正常時の行:

```
ADDRESS_DB_FILE=PASS ...\skyrimSECK\Data\SKSE\Plugins\version-1-5-97-0.bin
REL_MODULE=PASS
ADDRESS_LIBRARY=PASS
KNOWN_ID_RESOLVE=PASS
MENU_MANAGER=PASS (RE::UI)
INPUT_MANAGER=PASS
PAPYRUS_VM_POINTER=PASS
PAPYRUS_NATIVES_REGISTERED=PASS
TRANSLATIONS_IMPORTED=PASS files=N entries=M
PAPYRUS_CALL SKSE.GetVersionRelease
PAPYRUS_CALL Form.GetType
```

- `ADDRESS_DB_FILE` のパスが `skyrimSE` ではなく `skyrimSECK` になっていること
- 致命的な例外(アクセス違反など)が起きると `EXCEPTION code=... module=... +0x...` の行が出る

## 2. 問題一覧

各問題を「症状 → 原因 → 該当ソースまたは設定 → 修正 → 確認方法」の順に書く。

---

### 問題 1. MO2 から起動しても MOD が読み込まれない(Alternate Start が始まらない、SkyUI が出ない)

**症状**
- MO2 から `skyshim_loader` を起動すると、ゲームは起動するが、Alternate Start も SkyUI も動かない。
- `skyshim.log` の `ADDRESS_DB_FILE` が `E:\work\work_skyrim\skyrimSE\...` を指している。

**原因**
- MO2 が管理するゲームフォルダは `E:\work\work_skyrim\skyrimSECK`(`ModOrganizer.ini` の `gamePath`)。
- ローダーの起動設定(Start in と Arguments)が別のフォルダ `skyrimSE` を指していた。
- MO2 は `skyrimSECK\Data` にだけ MOD を重ねて見せる。`skyrimSE` 側の `SkyrimSE.exe` は、MOD を見ることができない。

**該当箇所**
- `ModOrganizer.ini` の `gamePath=@ByteArray(E:\\work\\work_skyrim\\skyrimSECK)`
- ローダー: `src\bootstrap\loader_main.cpp`。引数が無いときは現在の作業フォルダをゲームフォルダとして使う。

```cpp
std::wstring gameDir = argc > 1 ? argv[1] : L".";
```

**修正**
- MO2 の設定で Start in を `E:\work\work_skyrim\skyrimSECK` にし、Arguments を空欄にする。
- 診断用に、ローダーが受け取った内容を `loader.log` に書く処理を追加した(同じファイルの `Diagnostic` ブロック)。

**確認方法**
- `loader.log` の `cwd=` と `exe=` が `skyrimSECK` になっている。
- `skyshim.log` の `ADDRESS_DB_FILE` が `skyrimSECK` のパスになっている。

---

### 問題 2. ローダーがすぐ終了し、MO2 の仮想ファイルシステムが閉じるおそれがある

**症状**
- ローダーはゲームを再開した直後に終了していた。MO2 は、自分が起動したプログラムが終了すると、ファイルの重ね合わせを終える。

**原因**
- ローダーがゲームの終了を待たずに `return 0` していた。

**該当ソース**(修正後): `src\bootstrap\loader_main.cpp`

```cpp
ResumeThread(pi.hThread);
CloseHandle(pi.hThread);
// Stay alive until the game exits: a mod manager (MO2) tears down its VFS when the launched process ends.
WaitForSingleObject(pi.hProcess, INFINITE);
CloseHandle(pi.hProcess);
return 0;
```

**確認方法**
- ゲームを終了するまで、MO2 の実行ボタンが「実行中」のままになる。

---

### 問題 3. エラーコード 1「SKSE64 is not running」が出る(Address Library が見えない)

**症状**
- SkyUI が `SKYUI ERROR CODE 1` を出す。
- `skyshim.log` が `ADDRESS_DB_FILE=FAIL ...\skyrimSECK\Data\SKSE\Plugins\version-1-5-97-0.bin` で止まる。

**原因**
- MO2 の MOD の有効/無効は、プロファイルごとに別々に保存される(`mod_se\profiles\<名前>\modlist.txt`)。使ったプロファイルで `0000(system)Address Library All in One` が無効(行頭が `-`)だった。
- Address Library の `bin` が見つからないため、Skyshim が起動途中で止まり、Papyrus のネイティブ関数が登録されなかった。その結果、SkyUI から見ると SKSE が動いていない状態になった。
- `skyrimSECK\Data\SKSE\Plugins` は実際には空。ファイルは MO2 の MOD からだけ供給される。

**確認方法**
- `mod_se\profiles\<プロファイル名>\modlist.txt` の `0000(system)Address Library All in One` の行が `+` で始まっている。

**修正**
- 使うプロファイルで、この MOD にチェックを入れる。プロファイルを切り替えたら、毎回確認する。

---

### 問題 4. エラーコード 4「Papyrus INI settings are invalid」が出る

**症状**
- SkyUI が `SKYUI ERROR CODE 4` を出す。`Skyrim.ini` の `[Papyrus]` に正しい値を書いても消えなかった。

**原因(3 つが重なっていた)**

1. `Utility.GetINIInt` と `Utility.GetINIFloat` が、Skyrim 1.5.97 のバニラには存在せず、SKSE が追加していた。Skyshim が未実装で、ゲームは「未バインドのネイティブ関数」として 0 を返していた。
   - 証拠: `Papyrus.0.log` に `error: Unbound native function "GetINIInt" called` と出る。
   - SkyUI 側の判定: `SkyUI-Community\source\scripts\SKI_Main.psc` の 143〜144 行目

   ```papyrus
   if (Utility.GetINIInt("iMinMemoryPageSize:Papyrus") <= 0 || Utility.GetINIInt("iMaxMemoryPageSize:Papyrus") <= 0 || Utility.GetINIInt("iMaxAllocatedMemoryBytes:Papyrus") <= 0)
       Error(ERR_INI_PAPYRUS, "Your Papyrus INI settings are invalid. ...")
   ```

2. MO2 の設定 `LocalSettings=false` のため、ゲームが読む INI は MO2 のプロファイル内のファイルではなく、`C:\Users\kaiser\Documents\My Games\Skyrim Special Edition\Skyrim.ini` だった(`mod_se\profiles\<名前>\settings.ini` の `LocalSettings=false`)。
3. 同じファイルの `[Papyrus]` に `bEnableLogging` と `bEnableTrace` が 2 回書かれ、先に書かれた `0` が使われて Papyrus のログが出なかった(同じキーが重複したときは、先頭の値が使われる)。

**必要な設定**(`Documents\...\Skyrim.ini` の `[Papyrus]`)

```
iMinMemoryPageSize=128
iMaxMemoryPageSize=512
iMaxAllocatedMemoryBytes=1048576
```

**該当ソース**(修正後): `src\papyrus\register.cpp`

```cpp
RE::Setting* FindINI(const RE::BSFixedString& a_key)
{
    if (!a_key.data()) return nullptr;
    // Skyrim.ini first, then SkyrimPrefs.ini (iSize W/H:Display live there).
    if (auto* ini = RE::INISettingCollection::GetSingleton())
        if (auto* s = ini->GetSetting(a_key.c_str())) return s;
    if (auto* prefs = RE::INIPrefSettingCollection::GetSingleton())
        return prefs->GetSetting(a_key.c_str());
    return nullptr;
}
...
a_vm->RegisterFunction("GetINIInt", "Utility", GetINIInt);
a_vm->RegisterFunction("GetINIFloat", "Utility", GetINIFloat);
```

**確認方法**
- `Papyrus.0.log` に `Unbound native function "GetINIInt"` が出ない。
- エラーコード 4 の画面が出ない。

**注意**
- Papyrus のログは、`Documents\My Games\Skyrim Special Edition\Logs\Script` フォルダが存在しないと出力されなかった。フォルダを手で作成した。

---

### 問題 5. `Cannot divide by zero`(`SKI_ConfigMenu.ApplySettings`)

**症状**
- `Papyrus.0.log` に次のエラーが出る。

```
error: Cannot divide by zero
stack:
    [SKI_ConfigMenuInstance (05000820)].SKI_ConfigMenu.ApplySettings() ...
```

**原因**
- SkyUI は `Utility.GetINIInt("iSize W:Display")` と `iSize H:Display` を読んで割り算をする(`SkyUI-Community\source\scripts\SKI_ConfigMenu.psc` 1563〜1564 行目付近)。
- この 2 つの設定は `SkyrimPrefs.ini` に入っているが、最初の実装は `Skyrim.ini` 側の設定だけを検索していたため、0 を返した。

**修正**
- 問題 4 の `FindINI` のとおり、`Skyrim.ini` で見つからなければ `SkyrimPrefs.ini`(`RE::INIPrefSettingCollection`)を検索する。

**確認方法**
- `Papyrus.0.log` に `Cannot divide by zero` が出ない。

---

### 問題 6. システムメニューの項目が「$MOD CONFIGURATION」と表示される(翻訳が効かない)

**症状**
- SKSE で起動したときは「MOD設定」と表示される。Skyshim では `$MOD CONFIGURATION` とキーがそのまま表示される。
- MO2 から素の `SkyrimSE.exe`(SKSE なし、Skyshim なし)で起動しても、同じ表示になる。

**原因**
- バニラのゲーム本体は、公式ファイル(`Skyrim.esm` など)の翻訳ファイルしか読まない。MOD の翻訳ファイル `Data\Interface\Translations\<プラグイン名>_<言語>.txt` を読み込む処理は、SKSE が行っていた。
- 翻訳ファイル自体は正しく存在し、ゲームプロセスから見えていた(ファイルの存在確認と一覧取得の両方で確認済み)。ファイルの有無や言語設定(`sLanguage=ENGLISH`)は原因ではなかった。

**SKSE 側の動作の参照先**(挙動の把握にだけ使い、コードはコピーしていない)
- `E:\work\work_skyrim\src\skse\skse64\skse64\Translation.cpp`
- `E:\work\work_skyrim\src\skse\skse64\skse64\Hooks_Scaleform.cpp`(1512〜1532 行目。ゲームの Scaleform 読み込み処理を作った直後に、翻訳ファイルを取り込んでいる)

**SKSE の動作の要点**
1. `%LOCALAPPDATA%\Skyrim Special Edition\plugins.txt` を読む(行頭が `*` のプラグインが有効)。
2. 有効なプラグインごとに `Data\Interface\Translations\<プラグイン名>_<sLanguage>.txt` を探す。
3. ファイルの先頭が UTF-16 リトルエンディアンの BOM(`FF FE`)であることを確認する。
4. `$キー<タブ>翻訳文` の各行を、ゲームの翻訳テーブルに追加する。

**修正**
- `src\translation.cpp` を追加した(`ImportModTranslations`)。ゲームの翻訳テーブルは次で取得する。

```cpp
auto* mgr = RE::BSScaleformManager::GetSingleton();
if (!mgr || !mgr->loader) return false;
auto tr = mgr->loader->GetState<RE::BSScaleformTranslator>(RE::GFxState::StateType::kTranslator);
```

- 呼び出しは `src\engine.cpp` の `WaitForSingletons`。メインメニューが開いてから 1 回だけ実行する。

```cpp
if (!tr && ui) {
    if (auto* u = RE::UI::GetSingleton(); u && u->IsMenuOpen(RE::MainMenu::MENU_NAME)) tr = ImportModTranslations(a_log);
}
```

**制約**
- 対象は `Data` にあるファイル(仮想ファイルシステム経由を含む)だけ。BSA の中にある MOD の翻訳ファイルは読まない。

**確認方法**
- `skyshim.log` に `TRANSLATIONS_IMPORTED=PASS files=N entries=M`(N と M が 0 でない)。
- システムメニューに「MOD設定」と表示される。

---

### 問題 7. 翻訳の読み込みを追加したところ、ゲームが起動しなくなった(クラッシュ)

**症状**
- 翻訳の読み込みを追加した直後の起動で、ゲームが落ちた。`skyshim.log` が `INPUT_MANAGER=PASS` の行で止まっていた。

**原因**
- 最初の実装は、UI と入力の管理オブジェクトが現れた直後(ゲームの起動途中)に、翻訳テーブルを書き換えていた。この時点では Scaleform の読み込み処理がまだ作成途中で、テーブルが未完成だったと考えられる。SKSE が「読み込み処理を作った直後」に処理を割り込ませているのは、この理由と考えられる。
- 断定はできていない。ログに例外の位置が残っていなかったため。

**修正**
- 翻訳の読み込みを、メインメニューが開いた後に移した(問題 6 のソース)。
- Papyrus のネイティブ関数の登録は、翻訳の読み込みとは切り離し、Papyrus の仮想マシンが現れた時点で行う。翻訳が終わるのを待つと、登録が遅れてスクリプトが先に動くおそれがあるため。

```cpp
// Natives are registered as soon as the VM exists (scripts must not run before this).
if (!natives && ui && input && vm) {
    natives = true;
    const bool ok = skyshim::papyrus::RegisterAll(RE::SkyrimVM::GetSingleton()->impl.get());
    a_log("PAPYRUS_NATIVES_REGISTERED=%s", ok ? "PASS" : "FAIL");
}
```

- 次回のクラッシュ調査用に、致命的な例外の位置を出力する処理を追加した(`src\runtime.cpp` の `CrashLogger`)。ログに `EXCEPTION code=... module=... +0x...` が出る。

---

### 問題 8. 翻訳ファイルが 1 件も取り込まれない(`files=0 entries=0`)

**症状**
- `TRANSLATIONS_IMPORTED=PASS files=0 entries=0`。

**原因**
- パスを組み立てる文字列から、区切りの `\` が失われていた。コマンドで文字列を一括置換したときに、`\\`(C++ で `\` を表す書き方)が `\` 1 つに変わり、`L"Data\Interface\Translations"` になった。C++ では `\I` `\T` が不正なエスケープとなり、実際には `DataInterfaceTranslations` という存在しないパスを探していた。

**修正**(`src\translation.cpp`): 区切りを含む文字列リテラルを使わず、パスの部品を `/` 演算子でつなぐ。

```cpp
const auto p = std::filesystem::path(L"Data") / L"Interface" / L"Translations" / (name + L"_" + lang + L".txt");
```

**再発防止**
- パスの区切りを含む文字列を、コマンドの一括置換で編集しない。編集後は、対象行を読み直して `\` が残っているか確認する。

---

### 問題 9. 計画書の前提が誤っていた点

計画書 `SkyUI_SKSE_compat_runtime_plan_rev4.md` と、`docs\SKYUI_SKSE_SURFACE.md` の集計に、次の誤りと漏れがあった。

| 内容 | 誤り | 実際 |
|---|---|---|
| `Utility.GetINIInt` / `GetINIFloat` | 「バニラにあるので作業不要」と記載 | 1.5.97 のバニラには無く、SKSE が追加している。実装が必要(問題 4) |
| MOD の翻訳ファイルの読み込み | 依存として記載なし | SKSE が行っていた処理。実装が必要(問題 6) |
| `StringUtil.Substring` の第 3 引数の既定値 | `scripts\StringUtil.psc` で `len = 0` | SKSE 本来の既定値は `-1`(末尾まで)。`-1` に修正した |
| 検証したゲームのバージョン | 計画書の対象は 1.7.104 | 現在の検証は 1.5.97 のみ。1.7.104(`versionlib-1-7-104-0.bin` を使う)は、実機で未確認 |

---

### 問題 10. ビルド環境まわりの補足

- `cmake --build` だけを実行すると、コンパイラが見つからずに失敗する。必ず `vcvars64.bat` を先に呼ぶ(1.1 節)。
- 環境に Python が入っていないため、Python での一括置換はできない。ファイルの編集は Edit ツールで行う。
- ビルドの出力に、日本語で `メモ: インクルード ファイル:` の行が大量に出る場合がある。エラーではない。`error C` と `FAILED` を検索して確認する。

## 3. 現在の状態と残っている問題

**解決済み**
- SKSE なしで、SkyUI のエラーポップアップが出なくなった(SKSE の存在確認とバージョン確認を通過)。
- システムメニューに「MOD設定」が表示される。

**未解決(次の作業)**
- `Papyrus.0.log` に、次の未バインドのネイティブ関数が残っている。このため MCM(MOD 設定画面)を開いても中身が表示されない。
  - `Form.RegisterForModEvent`、`Form.RegisterForMenu`、`Form.RegisterForKey`、`Form.SendModEvent`
  - `UI.IsMenuOpen`、`UI.Invoke*` など `UI` 系の関数
- Scaleform 側の `_global.skse` オブジェクトの提供(SWF から SKSE の関数を呼ぶための土台)が未着手。
- 計画書の Phase 5〜12(UI の橋渡し、イベント、`_global.skse`、拡張データなど)が未着手。

**同じ問題が起きたときの最短の確認順**
1. `loader.log` の `cwd` と `exe` が `skyrimSECK` か(問題 1)
2. 使うプロファイルで Address Library が有効か(問題 3)
3. `skyshim.log` に `PAPYRUS_NATIVES_REGISTERED=PASS` があるか
4. `Papyrus.0.log` の `Unbound native function` の一覧(未実装のネイティブ関数がここに出る)
5. `skyshim.log` の `EXCEPTION` の行(クラッシュの位置)


---

### 問題 11. MCM(MOD 設定)を開いても、SkyUI の項目が何も出ない

**症状**
- システムメニューに「MOD設定」は出るが、開いても SkyUI の設定項目が 1 つも表示されない。
- `Papyrus.0.log` に未バインドのネイティブ関数は出ていない(問題 1〜8 を解消した後の状態)。

**原因**
- MCM の画面は Scaleform(Flash)の SWF でできており、ActionScript が `_global.skse` というオブジェクトを通じて SKSE の関数を呼ぶ作りになっている。Skyshim がこのオブジェクトを提供していなかったため、次のような呼び出しが失敗していた。
  - `skse.SendModEvent(...)`(SWF から Papyrus へ通知する経路。SkyUI 全体で 34 か所)
  - `skse.version.releaseIdx`(70 以上で SkyUI の拡張機能を有効にする判定)
  - `skse.plugins.InventoryInjector`(未定義でも落ちないこと)
- SkyUI 側の該当箇所の例: `SkyUI-Community\source\actionscript\` 以下で `skse.` を検索する。

**修正**
- `src\scaleform\inject.cpp` を追加した。ゲームに登録されている全メニューの生成関数を、ラッパーに置き換える。メニューが作られた直後に `_global.skse` を作る。ゲーム本体のアドレスを推測して書き換える方法は使っていない。

```cpp
// 生成関数の置き換え(UI::menuMap の各項目の create を差し替える)
for (auto& entry : ui->menuMap) {
    g_original[n] = entry.second.create;
    entry.second.create = g_wrappers[n];
    ++n;
}
```

- 呼び出しは `src\engine.cpp` の `WaitForSingletons`。メインメニューが開いた後に 1 回だけ実行する。

**確認方法**
- `skyshim.log` に `SKSE_JS_INJECT=PASS wrapped N menu creators`(N は 35 前後)。
- 「MOD設定」を開くと、SkyUI の設定項目が表示される。

**残っている制限**
- `_global.skse` のうち、次は何もしない仮の実装で、呼ばれるとログに `SKSE_JS_STUB_CALLED <関数名>` を 1 回出す: `StartRemapMode`、`GetLastKeycode`、`GetLastControl`、`GetMappedKey`、`ShowOnMap`、`RequestActivePlayerEffects`、`ForceContainerCategorization`、`ExtendData`、`ExtendAlchemyCategories`、`EnableMapMenuMouseWheel`、`ExtendForm`。
- そのため、インベントリなどの拡張表示(並べ替えや絞り込み用の追加情報)、キー割り当て、地図やアクティブ効果の表示は、まだ SKSE 相当には動かない。

---

### 問題 12. UI 関連のネイティブ関数を実装するときの設計上の注意(今後の参照用)

- Scaleform(画面)への書き込みや関数呼び出しは、ゲームのメインスレッドで行う必要がある。Papyrus のネイティブ関数は別のスレッドから呼ばれることがある。
- SKSE のタスク機能(`SKSE::GetTaskInterface`)は、SKSE 本体を経由するため、Skyshim では使えない。
- そのため `src\mainthread.cpp` で、ゲームのメインループ内の `Main::Update` の呼び出し箇所(1.5.97 では ID 35565 の +0x748)を書き換え、1 フレームに 1 回タスクを実行する仕組みを作った。書き換える前に、その位置の先頭バイトが `call` 命令(`E8`)であることを確認し、違えば何もしない。
- この位置は、1.5.97 用の既知の値を使っている。AE(1.6 以降)や 1.7.104 では位置が異なるため、フックは付けない(ログに `MAIN_THREAD_HOOK=SKIPPED`)。1.7.104 対応の際は、別途位置の特定が必要。
- 読み取り系(`UI.GetInt` など)は、呼び出したスレッドで直接実行している。SKSE も同様の方針だが、まれに競合する可能性がある。


---

### 問題 13. MCM のページ名だけが「$General」のまま表示される(翻訳キーの大文字小文字)

**症状**
- SKSE 起動では、MCM の左側のページ名が「一般的」と表示される。
- Skyshim では、`$Controls`(操作方法)と `$Advanced`(高度な設定)は訳されるが、`$General` だけが `$General` のまま表示される。
- 翻訳テーブルの中身を確認すると、`$General` = 一般的 が入っているように見える。

**原因**
- ゲームの公式の翻訳ファイルは、`$GENERAL`、`$ARMOR`、`$MAP`、`$MAGIC`、`$OFF`、`$GROUP` のように**大文字のキー**で持っている。
- ゲームの文字列プール(`RE::BSFixedStringW`)は、大文字小文字を区別せず「同じ文字列」とみなす。最初に作られた綴りだけを正式な綴りとして保存する。
- 画面側の翻訳処理は、正式な綴りと同じ綴りでしか見つけられない。
  - `$GENERAL` で検索 → 見つかる
  - `$General` や `$general` で検索 → 見つからない
- SKSE は、公式の翻訳が読み込まれる前に MOD の翻訳を追加する。そのため MOD の綴り(`$General`)が正式な綴りになる。Skyshim は、公式の読み込みが終わった後に追加するため、公式の綴りが残り、SkyUI の綴りでは見つからなかった。
- 追加処理が「すでに同じキーがある」と判断していたため、確認用のログや `find` では見つかったように見えていた。

**該当ソース**(修正後): `src\translation.cpp` の `ParseFile`

```cpp
{
    RE::BSFixedStringW probe(key.c_str());
    if (auto it = map.find(probe); it != map.end()) {
        const std::wstring existingKey = it->first.c_str() ? it->first.c_str() : L"";
        ...
        if (existingKey != key) map.erase(it);  // probe releases its reference at the end of this block
    }
}
const auto result = map.insert({ RE::BSFixedStringW(key.c_str()), RE::BSFixedStringW(value) });
if (!result.second) result.first->second = RE::BSFixedStringW(value);
```

- 綴りが違う既存のエントリを、いったん削除する。その後、MOD の綴りで追加し直す。

**調べた方法**(同じ症状が別のキーで起きたときの手順)
1. SkyUI の SWF を取り出し、`FFDec`(`SkyUI-Community\tools\FFDec\ffdec-cli.jar`)で ActionScript を書き出す。
   `java -jar ffdec-cli.jar -export script <出力先> <SWF のパス>`
2. 翻訳処理 `skyui.util.Translator.translate` が、隠しテキストフィールドに `$キー` を入れて読み戻す方式だと確認する。
3. 同じ手順をゲーム内で再現する診断を書き、キーの綴りを変えて結果を比べた。`$GENERAL` だけが訳され、`$General` は訳されなかった。
4. 翻訳テーブルの全エントリを列挙し、実際に保存されている綴りが大文字だと確認した(`TRANSLATION_DUMP_ENTRY`)。

**確認方法**
- `skyshim.log` に `TRANSLATION_KEY_REPLACED existing=$GENERAL=一般 new=$General=一般的` が出る。
- MCM のページ名が「一般的」と表示される。

**残っている制限**
- 訳文(値)にも同じ性質がある。公式の文字列と大文字小文字だけが違う訳文は、先に作られた綴りで表示される(例: `Group` が `GROUP` と表示される)。SKSE でも、読み込み順が違うだけで同種の差が出る。見た目の違いだけで、機能には影響しない。

**診断用の道具(普段はオフ)**
- `src\scaleform\inject.cpp` の `TranslateProbe`: 隠しテキストフィールドで翻訳を再現する。`Wrapper` 内のコメントに、有効にする方法を書いてある。
- `src\translation.cpp` の `DumpTranslations`: 翻訳テーブルの一部を列挙する。`src\engine.cpp` のコメントに、有効にする方法を書いてある。

**問題 13 の修正の更新(最終版)**

最初の修正(綴りが違う公式のエントリを削除して入れ直す方法)には、副作用があった。ゲーム下部のメニュー(魔法、マップなど)は、公式の綴り(`$MAGIC`、`$MAP`)で検索するため、SkyUI の綴りに入れ替えると、こちらが訳されなくなった(`$MAGIC`、`$MAP` と表示された)。

- 原因: 画面側の検索は、保存されている綴りと完全に一致するキーでしか見つからない。公式(大文字)と SkyUI(大文字小文字が混在)の両方の綴りを、別々のエントリとして持つ必要がある。
- 修正: キーと訳文を、SKSE と同じ `RE::BSScaleformTranslator::GetCachedString` で作る。綴りを保持するキャッシュのため、綴りが違えば別のエントリになる(`RE::BSFixedStringW` の通常のコンストラクタは大文字小文字を区別しないため、使わない)。公式のエントリは削除しない。
- 該当ソース: `src\translation.cpp` の `ParseFile` 内の `cached` ラムダと、`map.insert({ cached(key), cached(value) })`。
- 確認: ゲーム下部のメニューが「魔法」「マップ」と表示され、MCM のページ名も「一般的」と表示される。
- ログ: `TRANSLATION_KEY_COLLAPSED` は、キャッシュが既存のエントリに吸収した場合にだけ出る。その場合は、既存のエントリの値を MOD の値に更新する。

**この問題から得られる一般的な注意**
- ゲームの文字列に関する処理では、「大文字小文字を区別するか」が、文字列の作り方(プールを通すか、別のキャッシュを通すか)で変わる。
- 画面に `$` 付きのキーがそのまま出るときは、キーが存在するかだけでなく、綴りが完全に一致しているか(大文字小文字を含む)を確認する。


---

### 問題 14. MCM でキー割り当てを始めると、何も操作できなくなる(画面が固まる)

**症状**
- MCM のキー割り当て項目を選ぶと、キー割り当ての小さな画面が開き、その後何を押しても反応せず、抜けられなくなる。

**原因**
- SkyUI は、キー割り当てを始めるときに `skse.StartRemapMode(this)` を呼ぶ。SKSE は、次に押されたキーを `this.EndRemapMode(キー番号)` に渡す。
- Skyshim では、この関数が何もしない仮の実装だったため、`EndRemapMode` が呼ばれなかった。SkyUI は、`EndRemapMode` が呼ばれるまで入力を受け付けない(`_bRemapMode` が真の間、`handleInput` が入力を捨てる)ため、固まった。
- SkyUI 側の該当箇所: `SkyUI-Community\source\actionscript\ModConfigPanel\ConfigPanel.as` の `initRemapMode`(635〜642 行目)、`EndRemapMode`(643〜651 行目)、`handleInput` の 372〜375 行目。

**修正**
- `src\events\events.cpp` に、キー入力の受け取り処理を追加した。割り当て待ちの間、次に押されたキーを `EndRemapMode(キー番号)` としてゲームのメインスレッドで呼ぶ。
- `src\scaleform\inject.cpp` の `_global.skse` に、`StartRemapMode` と `GetLastKeycode` を実装した。

```cpp
AddFn(a_view, skse, "StartRemapMode", [](FnHandler::Params& p) {
    if (p.argCount > 0 && p.args[0].IsObject()) events::StartRemap(p.args[0]);
});
```

- 割り当てを開いたキー(Enter やクリック)を、割り当て先と誤って受け取らないよう、割り当てを始めてから 0.25 秒以内の入力は無視する。
- キー番号は SKSE と同じ規則: キーボードはスキャンコード、マウスは 256 を足した値。ゲームパッドは未対応。

**確認方法**
- MCM のキー割り当て項目を選び、キーを押すと、画面が固まらず、押したキーが表示される。

---

### 問題 15. Papyrus のログに `Unbound native function "LogicalAnd" called` が出る

**症状**
- `Papyrus.0.log` に `error: Unbound native function "LogicalAnd" called` が出る。

**原因**
- `Math.LogicalAnd` などのビット演算は、Skyrim 1.5.97 のバニラには無く、SKSE が追加している(`skyrimSECK\Data\Scripts\Source\Math.psc` の 42 行目以降に `SKSE 64 additions` と書かれている)。
- `SkyUI-Community\source\scripts\SKI_FavoritesManager.psc` が使っている。

**修正**
- `src\papyrus\register.cpp` に、`Math` の 6 関数(`LeftShift`、`RightShift`、`LogicalAnd`、`LogicalOr`、`LogicalXor`、`LogicalNot`)を追加した。

**同じ種類の問題が起きたときの確認**
- `Papyrus.0.log` の `Unbound native function "<関数名>"` を、`skyrimSECK\Data\Scripts\Source` 以下の `.psc` で検索する。`SKSE additions` の記述があれば、SKSE 由来。Skyshim に追加する。


---

### 問題 16. 別の PC でビルドすると、`spdlog.lib` のリンクで未解決のシンボルが出る

**症状**
- `cmake --build` の最後に、次のエラーが出る。

```
spdlog.lib(spdlog.cpp.obj) : error LNK2019: 未解決の外部シンボル __std_find_first_of_trivial_pos_1 ...
skyshim.dll : fatal error LNK1120: 2 件の未解決の外部参照
```

**原因**
- 同じ Visual Studio 2022 の中に、複数のツールセット(コンパイラの版)が入っていた(`14.38.33130` と `14.44.35207`)。
- vcpkg は、新しい版(19.44)で `spdlog` をビルドした。一方、開発者コマンドプロンプトの既定は古い版(19.38)で、リンカーが古かった。新しい版の標準ライブラリで作ったライブラリを、古い版のリンカーでは解決できなかった。

**確認方法**
- 入っているツールセット: `dir "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"`
- vcpkg が使った版: `findstr /s /c:"compiler identification" <vcpkg のフォルダ>\buildtrees\spdlog\*.log`(`MSVC 19.44...` のように出る)

**修正**
- プロンプトの側を、vcpkg が使った版に合わせる。ビルドのたびに、次を実行する。

```
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
cl
```

- `cl` が `Version 19.44.xxxxx` と表示されることを確認してから、`rmdir /s /q build-se` のあとで `cmake` を実行する。

**補足**
- ビルドの途中で `Performing Test CMAKE_HAVE_LIBC_PTHREAD - Failed` と出るのは、Windows に pthread が無いことの確認で、エラーではない(直後に `Found Threads: TRUE`)。

---

### 問題 17. メインメニューに入るときにゲームがクラッシュする(`EngineFixes.dll` の中)

**症状**
- Skyshim のログが、すべて `PASS` のまま、最後に次の行が出てクラッシュする。

```
EXCEPTION code=C0000005 addr=... module=...\Data\SKSE\Plugins\EngineFixes.dll +0x2EAFD
```

**原因(断定していない)**
- `EngineFixes.dll`(SSE Engine Fixes)は、SKSE が読み込む SKSE プラグインである。ゲームフォルダに入っているプリローダー(`d3dx9_42.dll` など)によって、SKSE なしでもプロセスの中で読み込まれていた。
- SKSE が提供する機能が無いことで失敗したのか、ゲームのメインループの呼び出し箇所を書き換える Skyshim のフックと衝突したのかは、特定できていない。

**修正**
- 次のファイルを、別のフォルダへ移動する(削除ではなく退避)。
  - ゲームフォルダの `d3dx9_42.dll`(あれば)
  - `Data\SKSE\Plugins\EngineFixes.dll`
- 結果: クラッシュしなくなった。

**判断の手がかり**
- クラッシュのログの `module=` に、Skyshim 以外の DLL が出ていれば、その DLL(多くは SKSE プラグイン)が原因の候補になる。
- 計画書のとおり、任意の SKSE プラグインの動作は対象外としている。必要になった場合は、衝突するかどうかを別途調べる。

---

### 問題 18. 日本語のコメントがあると、Papyrus のコンパイルで宣言が欠ける(エラーは出ない)

**症状**
- `scripts` のコメントを日本語にしたあと、`build_pex.ps1` は成功と表示する。
- ところが、できた `.pex` が小さくなり(例: `UI.pex` が 2080 → 1087 バイト、`Form.pex` が 5067 → 2338 バイト)、宣言した関数の一部が入っていない。エラーも警告も出ない。

**原因**
- Papyrus のコンパイラ(`PapyrusCompiler.exe`)は、UTF-8 の日本語を、別の文字コードとして読む。コメントの直後の宣言が、文字の途中として飲み込まれて、欠ける。

**確認方法**
- `.pex` が、期待より小さくないか確認する。
- 確実な確認: SkyUI のスクリプトを、この追加宣言だけでコンパイルして、足りない関数が無いか調べる(`build_pex.ps1 -SkyUISource ...`)。

**修正**
- ソースのコメントは日本語のまま残す。`build_pex.ps1` の `Remove-Comments` が、コンパイル用の一時コピーから、`;` で始まるコメント行を取り除く。

**再発防止**
- 「コンパイルが通る」ことは、宣言が欠けていないことの証明にならない。`build_pex.ps1` は、できた `.pex` に、`additions` の宣言が入っているかを、自分で検証する。

---

### 問題 19. 追加宣言を足したはずの `.pex` に、宣言が入らない(バニラの版が使われる)

**症状**
- `build_pex.ps1` で作った `Form.pex` などに、`RegisterForModEvent` などの追加宣言が入っていない。

**原因**
- Papyrus のコンパイラは、作業フォルダにある同名の `.psc`(例: `Form.psc`)を、指定したファイルより先に使う。
- `build_pex.ps1` が、作業フォルダを、バニラの `.psc` があるフォルダにしていたため、追加宣言の無い、バニラの `Form.psc` が使われた。
- 以前、`.pex` に宣言が入っていると確認したのは、SKSE の完全版の `.psc` があるフォルダで実行したときの結果で、誤りだった(SKSE の `GetName`、`SetName` などの宣言が入っていた)。

**修正**
- `build_pex.ps1` で、作業フォルダを、追加宣言を足した `.psc` を置く一時フォルダにする。フラグファイルは、絶対パスで指定する。

**確認方法**
- `build_pex.ps1` の最後に、「追加した宣言は、すべて .pex に入っています。」と出る。
- 手で確認するなら、`.pex` の文字列に、追加した関数名が含まれているか調べる。

---

### 問題 20. PowerShell のスクリプトで、日本語の文字列が壊れて構文エラーになる

**症状**
- `build_pex.ps1` に、日本語を含む文字列(`Write-Host "…"` など)を足すと、実行時に「文字列に終端記号がありません」などの構文エラーが出る。

**原因**
- Windows PowerShell 5.1 は、BOM の無い UTF-8 のファイルを、日本語の文字コード(CP932)として読む。日本語が壊れて、引用符が閉じなくなる(コメントだけなら、問題が出にくい)。

**修正**
- `build_pex.ps1` を、BOM 付き UTF-8 で保存する。編集したあとも、BOM 付きで保存する。

**補足**
- コンパイラの診断メッセージは、標準エラー出力に出る。`$ErrorActionPreference = 'Stop'` のままだと、正常な診断も実行時エラーになるため、その部分だけ `'Continue'` にしている。

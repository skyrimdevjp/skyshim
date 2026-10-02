# ビルドと動作確認の手順

SkyUI を SKSE なしで動かすための Skyshim(`skyshim.dll` と `skyshim_loader.exe`)を、
Skyrim Special Edition 1.5.97 向けにビルドして動かす手順。
問題が出たときは、`PROBLEMS.md` を参照する。

## 1. 必要なもの

| 項目 | 内容 |
|---|---|
| Visual Studio | 2022(17.8 以降)または 2026。ワークロード「C++ によるデスクトップ開発」(MSVC、Windows SDK、CMake、Ninja を含む) |
| CMake | 3.24 以降(Visual Studio 付属で足りる) |
| Git | リポジトリの取得に使用 |
| vcpkg | 依存ライブラリ(`rsm-binary-io`、`spdlog`)の取得に使用。インターネット接続が必要 |

vcpkg の準備(例):

```
cd /d D:\work
git clone https://github.com/microsoft/vcpkg.git
D:\work\vcpkg\bootstrap-vcpkg.bat
```

`bootstrap-vcpkg.bat` が無いときは、clone に失敗している。Visual Studio 付属の vcpkg には、このファイルが含まれないので、別に clone する。

## 2. ビルド(Skyrim 1.5.97 用)

**「x64 Native Tools Command Prompt for VS」** で行う。通常のコマンドプロンプトでは、コンパイラと Ninja が見つからない。

### 2.1 コンパイラの版を揃える(重要)

同じ Visual Studio の中に複数のツールセットが入っていると、vcpkg が使う版と、プロンプトの既定の版が違うことがある。
違うと、`spdlog.lib` のリンクで未解決のシンボル(`__std_find_first_of_trivial_pos_1`)が出る。

```
dir "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
findstr /s /c:"compiler identification" <vcpkg のフォルダ>\buildtrees\spdlog\*.log
```

2 行目に出た版(例: `MSVC 19.44`)に合わせて、プロンプトを初期化する。

```
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
cl
```

`cl` が `Version 19.44.xxxxx` と表示されることを確認する。ビルドのたびに、この手順を行う。

> **毎回、`-vcvars_ver=14.44` を付けた `vcvars64.bat` を、実行してからビルドする。** 付けずに(既定のツールセットで)ビルドすると、`__std_find_last_trivial_1` や `__std_remove_8` などの未解決のシンボルで、リンクに失敗する(`PROBLEMS.md` の問題 16)。
>
> 毎回、同じ手順になるように、次のような `build.bat` を作っておくと、間違いが減る。
>
> ```
> @echo off
> call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
> cd /d <skyshim のフォルダ>
> cmake --build build-se
> ```

### 2.2 設定とビルド

```
cd /d <skyshim のフォルダ>
cmake -S . -B build-se -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg のフォルダ>/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md ^
  -DSKYRIM_SUPPORT_AE=OFF
cmake --build build-se
```

- 成果物: `build-se\skyshim.dll` と `build-se\skyshim_loader.exe`(同じフォルダに置いたまま使う)
- 初回は、vcpkg が依存ライブラリをビルドするため、時間がかかる。
- `cmake --preset se-1-5-97` は、`CMakePresets.json` の `toolchainFile` がこのリポジトリの作者の環境のパスを指しているため、そのままでは使えない。プリセットを使うなら、そのパスを書き換える。
- ビルドの出力に `Performing Test CMAKE_HAVE_LIBC_PTHREAD - Failed` と出るのは、Windows に pthread が無いことの確認で、エラーではない。
- 日本語環境では、`メモ: インクルード ファイル:` の行が大量に出る場合がある。エラーではない。失敗の判断は、`error` と `FAILED` で行う。
- 設定をやり直すときは、`build-se` を削除してから `cmake` を実行する。

## 3. 動作確認の前提

1. Skyrim Special Edition **1.5.97**(1.6 以降は未対応)。
2. **Address Library**(`version-1-5-97-0.bin`)を、ゲームの `Data\SKSE\Plugins` に置く。MO2 なら、Address Library の MOD を、使うプロファイルで有効にする(プロファイルごとに、有効かどうかが別)。
3. SkyUI 5.2 SE を有効にする(`SkyUI_SE.esp` と `SkyUI_SE.bsa`)。
4. `Documents\My Games\Skyrim Special Edition\Skyrim.ini` の `[Papyrus]` に、次の 3 行を入れる。

```
iMinMemoryPageSize=128
iMaxMemoryPageSize=512
iMaxAllocatedMemoryBytes=1048576
```

5. **SKSE では起動しない**(SKSE が入っていると、検証にならない)。
6. SKSE プラグイン(`Data\SKSE\Plugins` の DLL)が、プリローダー経由でプロセスに読み込まれる環境では、そのプラグインがクラッシュする場合がある。例: SSE Engine Fixes(`EngineFixes.dll` と `d3dx9_42.dll`)。切り分けのため、一時的に外して試す。

## 4. 起動方法

### 4.1 MO2 を使う場合

「実行ファイルを編集」で、次のように登録する。

| 項目 | 値 |
|---|---|
| Binary | `build-se\skyshim_loader.exe` のパス |
| Start in | MO2 が使うゲームフォルダ(`ModOrganizer.ini` の `gamePath` と同じ値) |
| Arguments | 空欄 |

「Apply」を押してから「OK」で閉じる。Apply を押さないと、保存されない。

### 4.2 MO2 を使わない場合

```
build-se\skyshim_loader.exe "<ゲームフォルダ>"
```

## 5. 確認するログ

| ファイル | 内容 |
|---|---|
| `Documents\My Games\Skyrim Special Edition\Skyshim\skyshim.log` | Skyshim のログ。起動のたびに上書きされる |
| `build-se\loader.log` | ローダーが受け取った引数、作業フォルダ、起動する `SkyrimSE.exe` のパス |
| `Documents\My Games\Skyrim Special Edition\Logs\Script\Papyrus.0.log` | Papyrus のログ(`Skyrim.ini` の `[Papyrus]` の `bEnableLogging=1` と `bEnableTrace=1` が必要。`Logs\Script` フォルダが無いと出ないので、手で作る) |

`skyshim.log` の正常時の主な行:

```
ADDRESS_DB_FILE=PASS ...
REL_MODULE=PASS
ADDRESS_LIBRARY=PASS
MAIN_THREAD_HOOK=PASS
PAPYRUS_NATIVES_REGISTERED=PASS
TRANSLATIONS_IMPORTED=PASS files=N entries=M
SKSE_JS_INJECT=PASS wrapped N menu creators
EVENT_SINKS_INSTALLED=PASS
```

致命的な例外(アクセス違反など)が起きると、`EXCEPTION code=... module=... +0x...` の行が出る。`module=` が Skyshim 以外の DLL なら、その DLL が原因の候補になる。

## 6. 動作確認の項目

- システムメニューに「MOD設定」が出て、SkyUI の設定項目が表示される(日本語のページ名)
- MCM のキー割り当てが動く
- インベントリ、魔法、お気に入り(グループ含む)、コンテナ、売買が SkyUI の見た目で動く
- アクティブ効果ウィジェットが、画面に出る
- 錬金メニューが SkyUI の見た目になる
- マップが動く(ホイールのズームは動く。場所検索は、アイコンの選択のみで、地図の移動はしない)
- セーブしてからロードしても、お気に入りのグループや MCM の設定が保たれる

## 7. 未実装・既知の制限

- 仮の実装(呼ばれると、ログに `SKSE_JS_STUB_CALLED <名前>` を 1 回出す): `ExtendData`、`ForceContainerCategorization`、`ExtendAlchemyCategories`、`ShowOnMap`、`EnableMapMenuMouseWheel`、`GetLastControl`、`GetMappedKey`、`ExtendForm`
- Skyrim 1.7.104(`-DSKYRIM_SUPPORT_AE=ON`)は、実機で未確認。AE 版では、メインスレッドのフックを付けない。
- 任意の SKSE プラグインの動作は、対象外。
- ゲームパッドのボタンによるキー割り当ては、未対応。
- 進捗の詳細は、`STATUS.md` を参照する。

## 8. スクリプト(.pex)のビルド

Skyshim が登録したネイティブ関数は、スクリプト(`.pex`)の側に宣言が無いと、Papyrus に結び付かない。
SKSE を入れていない環境では、SKSE が提供していた次の `.pex` が無いので、ここで作る。

| 種類 | スクリプト | 作り方 |
|---|---|---|
| 新規 | `SKSE`、`UI`、`Input`、`StringUtil`、`EquipSlot` | `scripts\*.psc` を、そのままコンパイルする |
| バニラの拡張 | `Form`、`Game`、`Utility`、`Math`、`Actor`、`Armor`、`Weapon`、`Spell` | バニラの `.psc` に、`scripts\additions\<名前>.txt`(自作の最小限の宣言)を足して、コンパイルする |

- バニラの `.psc` は、このリポジトリに含めない。Creation Kit 付属の `Data\Source\Scripts` のものを使う。
- SKSE が入れた完全版(`Data\Scripts\Source` の `Form.psc` など)は、SkyUI が使わない関数と SKSE の説明を含むため、使わない。

実行(PowerShell):

```
.\scripts\build_pex.ps1 `
  -Compiler "<Creation Kit>\Papyrus Compiler\PapyrusCompiler.exe" `
  -VanillaSource "<Creation Kit のあるゲームフォルダ>\Data\Source\Scripts" `
  -Out "<出力先>" `
  -SkyUISource "<SkyUI-Community>\source\scripts"
```

- `-SkyUISource` は省略できる。指定すると、SkyUI の全スクリプトを、この追加宣言だけでコンパイルして、足りない SKSE の関数が無いかを確認する。
- 成功すると、`OK  Form.pex (...)` など 13 行と、「追加した宣言は、すべて .pex に入っています。」が出る。出力先の `.pex` を、MOD の `Scripts` フォルダに置いて使う。
- スクリプトは、できた `.pex` に、`additions` の宣言が入っているかを、自分で検証する。入っていなければ、エラーで止まる。
- 追加する宣言を増やしたとき(新しいネイティブ関数を実装したとき)は、`additions\<名前>.txt` に宣言を足して、ビルドし直す。

注意(詳しくは `PROBLEMS.md` の問題 18〜20):
- **ソースのコメントは日本語だが、Papyrus のコンパイラは、日本語のコメントを正しく読めない。** `build_pex.ps1` は、コンパイル用の一時コピーから、コメントだけの行を取り除く。`scripts` のファイルを、直接 `PapyrusCompiler.exe` に渡さない。
- コンパイラは、作業フォルダにある同名の `.psc` を優先する。`build_pex.ps1` は、作業フォルダを一時フォルダにして、追加宣言付きの版が使われるようにしている。
- `build_pex.ps1` は、日本語を含むため、BOM 付き UTF-8 で保存してある(Windows PowerShell 5.1 は、BOM が無いと、日本語を正しく読めない)。編集したあとも、BOM 付きで保存する。
- 生成した `.pex` を、SKSE のスクリプトを一度も入れていない環境で動かす確認は、`build_pex.ps1` の修正(問題 19)のあとに、やり直す必要がある。

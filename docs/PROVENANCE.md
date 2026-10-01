# 出所の記録(PROVENANCE)

Phase 0(基準の固定)の記録。何を基準にしたか、コードの出所は何か、を残す。

## 基準(固定した版)

| 項目 | 固定した値 | 確認状況 |
|---|---|---|
| SkyUI-Community | `b7a24a936647e815269e34f4899e4b09768eff29` | 手元の `SkyUI-Community` の HEAD と一致 |
| Skyrim(計画書の対象) | SkyrimSE.exe 1.7.104(Steam 版) | 手元に実行ファイルあり(下の「1.7.104 の所在」を参照)。ハッシュは記録済み。動作確認は未実施 |
| Skyrim(現在の開発と確認の対象) | SkyrimSE.exe 1.5.97 | 動作確認ずみ(`STATUS.md` を参照) |
| CommonLib(MIT 版) | `e34246283a75fdd7009108476f0c646038f71f3c` | 手元の `CommonLibSSE-NG` に、このコミットが存在する |
| GPL の境界 | `cf24ab81ccff66788303295e0167132f3c9a472f` | この境界より後のコードは、取り込まない |

## 警告(停止条件: GPL の境界より後のコードの取り込み)

手元の `E:\work\work_skyrim\CommonLibSSE-NG` の HEAD(`d61bca4d...`)は、GPL の境界**より後**にある。
この作業ツリーからは、絶対にコピーしない。MIT 版のコミットを指す、切り離した worktree を使う。

    git -C CommonLibSSE-NG worktree add ../skyshim/third_party/CommonLibSSE-NG-MIT e34246283a75fdd7009108476f0c646038f71f3c

- `third_party/CommonLibSSE-NG-MIT` は、上のコミット(MIT 版)のスナップショットを、このリポジトリに含めたもの(別リポジトリへの参照は削除した)。
- SKSE64 のソースは、動作を調べるための参照だけに使う。コードはコピーしていない。
- `scripts\*.psc` と `scripts\additions\*.txt` は、自作の宣言(関数の署名だけ)。
  バニラのスクリプト(`Form.psc` など)は、このリポジトリに含めない。ビルドのときに、Creation Kit 付属のものに、自作の宣言を足して使う(`docs\BUILD.md` の「8. スクリプト(.pex)のビルド」を参照)。
- SKSE が入れる完全版の `.psc`(説明コメントを含む)は、使わない。

## 手元の環境の調査(2026-09-30)

- `E:\work\work_skyrim\skyrimSE\SkyrimSE.exe` は **1.5.97.0**(SHA256 `5666E1BD...5D12`)。`SkyrimAE\SkyrimSE.exe` も、1.5.97.0。
- 調査の時点では、1.7.104 の `SkyrimSE.exe` は、手元に無かった。Address Library は、`version-1-5-*.bin`(最大 1-5-97-0)だけで、
  `mod_se\mods\0000(system)Address Library All in One\` にあった。1.6 と 1.7 用の `versionlib`(形式 5)は無かった。
- 参照用のソース:
  - `E:\work\work_skyrim\CommonLibSSE-NG`(GPL の境界より後の HEAD。MIT 版は、worktree でだけ使う)
  - `E:\work\work_skyrim\src\skse\skse64`(動作の参照だけ。コピーしない)
- この時点では、1.7.104 の動作確認(`RUNTIME_VERSION=1.7.104`)は、できなかった。判断は `STATUS.md` に記録した。

## 1.7.104 の所在(2026-09-30)

- `H:\game\steam\steamapps\common\Skyrim Special Edition\SkyrimSE.exe` = 1.7.104.0
  - SHA256 `846EFCCF0C1374D71F892907F46549560F2FCB0A75CB87A3EED438BAA0F1402F`
- その環境には、1.7.104 用の Address Library(`versionlib-1-7-104-0.bin`)は無かった(`Data\SKSE\Plugins` が空)。
  別の場所(`mod_se` の Address Library の MOD)に、`versionlib-1-7-104-0.bin` がある。
- 1.5.97 の開発用の環境: `E:\work\work_skyrim\skyrimSE`(`version-1-5-97-0.bin` あり)。

## 記録の注意

- `SkyrimSE.exe`(1.5.97)の SHA256 は、途中までしか記録していない(`5666E1BD...5D12`)。完全な値を記録するのは、未実施。

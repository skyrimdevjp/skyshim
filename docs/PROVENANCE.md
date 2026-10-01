# PROVENANCE (Phase 0 - Authority freeze)

| Item | Pin | Verified |
|---|---|---|
| SkyUI-Community | b7a24a936647e815269e34f4899e4b09768eff29 | local `SkyUI-Community` HEAD matches |
| Skyrim | SkyrimSE.exe 1.7.104 Steam | NOT yet verified (no exe hash recorded) |
| CommonLib MIT | e34246283a75fdd7009108476f0c646038f71f3c | object exists in local `CommonLibSSE-NG` |
| GPL boundary | cf24ab81ccff66788303295e0167132f3c9a472f | |

## WARNING (STOP: POST_GPL_CODE_IMPORTED)
Local `E:\work\work_skyrim\CommonLibSSE-NG` HEAD (d61bca4d...) is AFTER the GPL boundary.
Never copy from that working tree. Use a detached worktree at the MIT SHA:

    git -C CommonLibSSE-NG worktree add ../skyui_compat/third_party/CommonLibSSE-NG-MIT e34246283a75fdd7009108476f0c646038f71f3c

SKSE64 source is behavior reference only; no code is copied.
compat_scripts/*.psc are original declarations (signatures only).

## Local environment audit (2026-09-30)
- E:\work\work_skyrim\skyrimSE\SkyrimSE.exe = **1.5.97.0** (SHA256 5666E1BD...5D12); SkyrimAE\SkyrimSE.exe also 1.5.97.0.
- No SkyrimSE.exe 1.7.104 found locally. Address Library: only `version-1-5-*.bin` (up to 1-5-97-0) under
  mod_se\mods\0000(system)Address Library All in One\; no 1.6/1.7 versionlib (Format 5) present.
- Reference sources: E:\work\work_skyrim\CommonLibSSE-NG (post-GPL HEAD; MIT SHA via worktree only),
  E:\work\work_skyrim\src\skse\skse64 (behavior reference only, not copied).
=> Gate RUNTIME_VERSION=1.7.104 cannot pass here. Blocked pending decision (see STATUS.md).

## 1.7.104 located (2026-09-30)
- H:\game\steam\steamapps\common\Skyrim Special Edition\SkyrimSE.exe = 1.7.104.0
  SHA256 846EFCCF0C1374D71F892907F46549560F2FCB0A75CB87A3EED438BAA0F1402F
- Address Library for 1.7.104 (versionlib-1-7-104-0.bin) NOT present in that install (Data\SKSE\Plugins empty).
- 1.5.97 dev target: E:\work\work_skyrim\skyrimSE (version-1-5-97-0.bin available).

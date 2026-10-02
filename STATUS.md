# Implementation status vs rev4
- Phase 0: DONE (docs/PROVENANCE.md; SkyrimSE.exe hash still to record)
- Phase 1: DONE (docs/SKYUI_SKSE_SURFACE.md; corrections to rev4 listed there)
- Phase 4/12 partial: scripts/*.psc declarations written; Form/Game additions pending merge gate
- Phase 6/7 partial: ModEventRegistry + IndexStore (engine-independent, smoke-tested)
- Phases 2,3,5,7(inject),8-11: NOT STARTED - need SkyrimSE 1.7.104 + Address Library + RE work.
  Per STOP rules, no guessed RVAs/hook sites were written.

## Blocker (2026-09-30)
Only Skyrim 1.5.97 is available locally; rev4 targets 1.7.104 (needs the exe + Format-5 Address Library).
Options: (a) supply 1.7.104 exe + Address Library; (b) develop/verify on 1.5.97 first (skse64 source and
version-1-5-97-0.bin available), forward-port to 1.7.104 later.

## Phase 2 result (2026-09-30)
- skyshim_loader.exe (CREATE_SUSPENDED + LoadLibraryW injection) + skyshim.dll (IAT hook on
  `_get_narrow_winmain_command_line`, fires on the game main thread after unpack, before game init).
- Verified by launching: 1.5.97 (E:\work\work_skyrim\skyrimSE) and 1.7.104 (H:\game\steam\...\Skyrim Special Edition)
  both log RUNTIME_LOADED_BEFORE_PAPYRUS=PASS / RUNTIME_LOADED_BEFORE_SCALEFORM_MENU=PASS.
  (1.7.104 takes ~10-30 s before the hook fires; log: Documents\My Games\Skyrim Special Edition\Skyshim\skyshim.log)
- Caveat: the PASS lines are asserted from hook position (before game main), not from observing the VM.
- Phase 3 next: Address Library missing at game Data\SKSE\Plugins for both. 1.5.97 copy is in
  mod_se\mods\0000(system)Address Library All in One\SKSE\Plugins\; set SKYSHIM_ADDRLIB_DIR to use it.
  1.7.104 versionlib-1-7-104-0.bin still needed.

## Phase 3 result (2026-09-30, Skyrim 1.5.97 only)
- Built build-se (preset se-1-5-97, CommonLibSSE MIT snapshot) via vcvars64 + `cmake --build build-se`.
- Launched with skyshim_loader.exe; log shows PASS for: REL_MODULE, ADDRESS_LIBRARY, KNOWN_ID_RESOLVE,
  MENU_MANAGER (RE::UI), INPUT_MANAGER, PAPYRUS_VM_POINTER. Address Library bin is now in skyrimSE\Data\SKSE\Plugins.
- Caveat: RUNTIME_LOADED_* remain asserted from hook position. 1.7.104 (build-ae, versionlib-1-7-104-0.bin) not yet verified.
- Next: Phase 4 — register SKSE.GetVersion* / Form.GetType natives (SKI_MAIN_SKSE_CHECK), verifying VM registration on 1.5.97 first.

## Phase 4 partial (2026-09-30, 1.5.97)
- src/papyrus/register.cpp: SKSE.GetVersion/Minor/Beta/Release/GetScriptVersionRelease (2.2.0, release 75) and Form.GetType
  registered via IVirtualMachine::RegisterFunction once RE::SkyrimVM impl exists (worker thread, after VM creation).
- Log: PAPYRUS_NATIVES_REGISTERED=PASS (registration call completed; NOT yet proof scripts can call them).
- SKI_MAIN_SKSE_CHECK NOT verified: needs SkyUI_SE.esp/bsa + compat .pex (compile scripts) and a save load to see
  SKI_Main not raising ERR_SKSE_*. Open risk: registration timing vs. script binding (thread-based, not a hook site).

## Phase 4 in-game check procedure (pending user action)
- natives now log `PAPYRUS_CALL SKSE.GetVersionRelease` / `Form.GetType` (first 20 calls) to skyshim.log.
- Existing Data\Scripts\{SKSE,Form,UI,...}.pex (from the old SKSE install) already declare these natives, so the compat .pex is
  not needed for this gate; the test only needs SkyUI enabled: mod_se\mods\0001(system)SkyUI 5 2 SE\SkyUI_SE.{esp,bsa}.
- Launch WITHOUT SKSE: build-se\skyshim_loader.exe "E:\work\work_skyrim\skyrimSE", enable SkyUI_SE.esp, load a save,
  wait ~30 s. PASS = PAPYRUS_CALL lines in the log and no "SKSE64 is not running" popup from SKI_Main.

## Phase 4 DONE + MO2 integration (2026-09-30, 1.5.97, MO2 profile Default)
- SKI_MAIN_SKSE_CHECK=PASS: no SkyUI error popup (was ERR 1/4 before; see fixes below).
- MO2: custom executable skyshim_loader.exe, Start in = <MO2 game dir> (skyrimSECK), Arguments empty; Address Library mod must be
  enabled in the active profile (per-profile!). Loader now waits for the game (MO2 tears down VFS otherwise); loader.log records cwd/exe.
- Corrections to earlier assumptions (Phase 1 census):
  * Utility.GetINIInt/GetINIFloat are NOT vanilla on 1.5.97 (unbound) -> implemented (Skyrim.ini then SkyrimPrefs.ini lookup).
  * SKSE also loads per-plugin translation files (Interface\Translations\<plugin>_<lang>.txt) into the GFx translator;
    without it SkyUI shows $MOD CONFIGURATION. Implemented in src/translation.cpp (reads %LOCALAPPDATA%\Skyrim Special Edition\plugins.txt,
    only after the Main Menu is open; importing earlier crashed). Loose files only (not BSA-hosted). Result: "MOD設定" shown.
- Crash diagnostics: vectored handler logs EXCEPTION module+offset to skyshim.log.
- Still unbound in Papyrus log: Form.RegisterForModEvent/RegisterForMenu/RegisterForKey/SendModEvent, UI.* (Phase 5/6). MCM page empty until then.
- Papyrus logging enabled by the user in Documents\...\Skyrim.ini ([Papyrus] bEnableLogging/bEnableTrace=1); backup Skyrim.ini.bak_skyuicompat.

## Phase 5/6/7 first cut (2026-09-30, 1.5.97, MO2 Default)
- Phase 6 events: Form.RegisterForModEvent/UnregisterForModEvent/SendModEvent/RegisterForMenu/UnregisterForMenu/RegisterForKey/UnregisterForKey
  (src/events/events.cpp; sinks installed once the Main Menu is open). Log: EVENT_SINKS_INSTALLED=PASS.
- Main-thread pump: src/mainthread.cpp hooks the call to Main::Update (SE 1.5.97: ID 35565 + 0x748, byte E8 verified before patching).
  Log: MAIN_THREAD_HOOK=PASS. AE flavour: not installed (site unverified).
- Phase 5 UI natives: src/papyrus/ui_api.cpp (IsMenuOpen, IsTextInputEnabled, Set*/Get*/Invoke* incl. arrays). Writes/invokes run on the
  main thread; Get* run on the calling thread (same as SKSE, small race risk).
- Phase 7 _global.skse: src/scaleform/inject.cpp wraps every UI::menuMap creator and injects skse into the new movie.
  Implemented: version{major,minor,beta,releaseIdx}, plugins{}, Log, SendModEvent, AllowTextInput, OpenMenu, StoreIndices, LoadIndices.
  Stubs (log SKSE_JS_STUB_CALLED once): StartRemapMode, GetLastKeycode, GetLastControl, GetMappedKey, ShowOnMap, RequestActivePlayerEffects,
  ForceContainerCategorization, ExtendData, ExtendAlchemyCategories, EnableMapMenuMouseWheel, ExtendForm.
- Result (user, in game): System menu shows "MOD設定" and the SkyUI page lists its settings. Papyrus log: 0 Unbound natives, 0 errors.
- Not yet verified: MCM interaction (sliders, menus, colour, text, keymap), Favorites hotkeys, HUD widgets, save->load.
- Next: Phase 8 (Input: StartRemapMode/GetLastKeycode/GetMappedKey, Input.GetMappedControl), Phase 9 (ExtendData for Inventory/Container/
  Barter/Gift/Favorites/Magic), Phase 10 (Crafting), Phase 11 (Active Effects, Map).

## Translation keys with differing case (2026-09-30)
- Symptom: MCM page name "$General" stayed untranslated although the translation table held "$General"=一般的.
- Cause: the wide string pool (BSFixedStringW) keeps the FIRST created spelling and treats other spellings as equal; the GFx text lookup needs
  the exact spelling. Vanilla stores "$GENERAL"/"$ARMOR"/"$MAP"/"$MAGIC"/"$OFF"/"$GROUP"; importing after vanilla left the vanilla spelling.
  SKSE imports before vanilla, so the mod's spelling wins there.
- Fix (src/translation.cpp): if an existing entry has a different spelling, erase it, then insert the mod's spelling (log TRANSLATION_KEY_REPLACED).
- Known limit: translated VALUES that differ only in case from a string already in the pool show the pool's spelling (e.g. "GROUP" for "Group").
- Debug aids kept but switched off: scaleform TranslateProbe (hidden TextField round trip), engine DumpTranslations.

- UPDATE (final translation approach): the erase-and-reinsert fix broke vanilla UI ("$MAGIC"/"$MAP" showed untranslated on the tween menu).
  Now keys/values are built with BSScaleformTranslator::GetCachedString (case-preserving, as SKSE does) and inserted as separate entries,
  so vanilla "$MAGIC" and SkyUI "$Magic" coexist. Log: TRANSLATION_KEY_COLLAPSED appears only when the cache folded a key into an existing entry
  (its value is then taken from the mod). Verified in game: tween menu and MCM page names both translated.

## Phase 8 first cut + Math (2026-09-30)
- skse.StartRemapMode(target) / skse.GetLastKeycode(reset) implemented (src/events/events.cpp StartRemap/LastKeycode; wired in src/scaleform/inject.cpp).
  The next key/mouse press (after 250 ms) calls target.EndRemapMode(keyCode) on the main thread. Keyboard = scancode, mouse = 256 + id. Gamepad: not yet.
  Verified in game by the user: MCM key mapping works (before: the MCM froze in remap mode because the stub never called EndRemapMode).
- Math.LeftShift/RightShift/LogicalAnd/LogicalOr/LogicalXor/LogicalNot implemented (SKSE additions; Papyrus log showed Unbound "LogicalAnd",
  used by SKI_FavoritesManager). Not yet verified in game.
- Still stubbed (logged once when called): GetLastControl, GetMappedKey, ShowOnMap, RequestActivePlayerEffects, ForceContainerCategorization,
  ExtendData, ExtendAlchemyCategories, EnableMapMenuMouseWheel, ExtendForm. Inventory already shows the SkyUI layout (user).

## User verification (2026-09-30, 1.5.97, MO2 Default)
- Working: MCM (pages, options, key mapping), Favorites menu (SkyUI layout, groups), Inventory (SkyUI layout, all columns/filters the user tried),
  Magic menu, Container menu. Papyrus log: no Unbound natives, no script errors. skyshim.log: no EXCEPTION.
- Observation: skse.ExtendData / ForceContainerCategorization (called from ItemMenu.as lines 48-49 during SWF init) never reached the stub, which
  suggests the SWF runs them before our injection (injection happens right after the menu creator returns). ExtendData data (formType, subType,
  weaponType, armorType, material, ...) is therefore not provided; verify columns like Type/Material in Inventory/Container.
- Not yet verified: HUD widgets, active effects widget (RequestActivePlayerEffects is a stub), Map (mouse wheel / ShowOnMap stubs), Crafting/Alchemy
  categories, Barter/Gift menus, save -> load with events re-registering, 1.7.104.

## Phase 11 (part): Active Effects (2026-09-30)
- skse.RequestActivePlayerEffects implemented in src/scaleform/inject.cpp (player->GetActiveEffectList(); fields id=usUniqueID, duration, elapsed,
  magnitude, effectFlags, archetype, actorValue, resistType; skips kInactive/kDispelled/kHideInUI). Verified by the user: HUD active effects widget works.
- Remaining stubs: GetLastControl, GetMappedKey (unused by current SkyUI), ShowOnMap, EnableMapMenuMouseWheel, ForceContainerCategorization,
  ExtendData, ExtendAlchemyCategories, ExtendForm.

## Second PC verification (2026-10-01, Skyrim 1.5.97, game dir D:\work\skyrimSEFix)
- Build on a second PC works with VS 2022 (toolset 14.44), vcpkg (spdlog, rsm-binary-io), Ninja. IMPORTANT: use the SAME MSVC toolset as the one
  vcpkg used for the libraries: `vcvars64.bat -vcvars_ver=14.44`. With the older 14.38 toolset the link fails
  (unresolved __std_find_first_of_trivial_pos_1 in spdlog.lib).
- Crash at the main menu was inside EngineFixes.dll (SSE Engine Fixes, a SKSE plugin loaded by its preloader without SKSE):
  skyshim.log showed EXCEPTION code=C0000005 module=...\EngineFixes.dll. Removing EngineFixes.dll (and its preloader d3dx9_42.dll) fixed it.
  Generic SKSE plugins are out of scope; whether the cause is a missing SKSE interface or a conflict with our Main::Update hook is NOT determined.
- Alchemy (crafting) menu shows the SkyUI layout (user), although ExtendAlchemyCategories is still a stub.
- Map menu: user confirms the SkyUI map menu works (second PC). EnableMapMenuMouseWheel / ShowOnMap are still stubs, so mouse-wheel zoom and
  "show on map" from the location finder were not separately verified.
- User check (second PC): Barter works; map mouse-wheel zoom works; location finder marks the selected icon but the map does NOT move to it
  (ShowOnMap is still a stub: skse.ShowOnMap(index) should centre the map camera on mapMarkers[index]; no verified engine call known yet).
  EnableMapMenuMouseWheel is also a stub, yet wheel zoom works in this setup (so the stub is not the cause of any visible problem).

## Finding: SKSE API surface used by SkyUI is larger than the Phase 1 census (2026-10-01)
Method: compile all SkyUI-Community/source/scripts/*.psc against vanilla CK sources + scripts/additions only
(work dir must contain the patched Form/Game/Utility/Math/Spell psc and must be the CURRENT directory; a vanilla Form.psc in the cwd wins otherwise).
Result: 10 of 12 scripts compile. SKI_FavoritesManager (and SKI_ConfigMenu, which depends on it) need more SKSE additions:
- New type: EquipSlot (script class + engine-side VM type registration; used for `as EquipSlot` casts and Spell.GetEquipType()).
- Spell.GetEquipType(); Weapon.GetWeaponType(); Armor.GetSlotMask(); Form.GetName().
- Actor.GetWornForm(int), GetEquippedObject(int), EquipItemEx(...), UnequipItemEx(...), EquipItemById(...), GetEquippedItemId(int), GetWornItemId(int).
None of these natives is implemented by the runtime yet. They are used when a favorites GROUP is equipped (hotkey / menu "use group"), so group
equipping is expected to fail with "Unbound native function" (not yet tested by the user; earlier tests covered only the favorites menu/groups UI).
The *ById / *ItemId functions rely on per-item IDs that SKSE's ExtendData provides (ExtendData is a stub), so Phase 9 and these natives are linked.

## Rename: skyui_compat -> Skyshim (2026-10-01)
- Project name is now **Skyshim** (a thin compatibility shim that lets SkyUI run without SKSE).
- Renamed: CMake project/targets (skyshim.dll, skyshim_loader.exe), C++ namespace (skyshim), log folder
  `Documents\My Games\Skyrim Special Edition\Skyshim\skyshim.log` (was SkyUICompat\skyui_compat.log), env var SKYSHIM_ADDRLIB_DIR,
  vcpkg.json name, comments and docs. Older entries in this file that mention skyui_compat were rewritten to the new names.
- MO2: re-register the executable (Binary = build-se\skyshim_loader.exe). The loader looks for skyshim.dll next to itself.

## Favorites equip natives + script build fixes (2026-10-01)
- Implemented in src/papyrus/equip_api.cpp (UNTESTED in game): Form.GetName, Weapon.GetWeaponType, Armor.GetSlotMask, Spell.GetEquipType,
  Actor.GetWornForm/GetEquippedObject/GetEquippedItemId/GetWornItemId/EquipItemEx/EquipItemById/UnequipItemEx, Game.IsObjectFavorited,
  Input.GetMappedControl. Item IDs are always 0 (ExtendData is a stub), EquipItemById ignores the id.
- Open risk: the engine may not know "EquipSlot" as a VM object type (Spell.GetEquipType returns a BGSEquipSlot form); check `as EquipSlot` in game.
- scripts: added additions/{Actor,Armor,Weapon,Spell}.txt, EquipSlot.psc, Form.GetName. build_pex.ps1 now verifies that the additions are present
  in the generated .pex and can compile all SkyUI scripts (-SkyUISource); all 12 SkyUI scripts compile with vanilla + additions.
- BUG FIXED in the build script: (1) Japanese comments made the Papyrus compiler drop following declarations -> comment lines are stripped from the
  compile copies; (2) a vanilla Form.psc in the current directory beat the patched one -> work dir is now the temp dir (PROBLEMS.md 18-20).
  .pex files built with the previous build_pex.ps1 (commits 4e2467c .. 9b99750) may lack the additions: rebuild them and redo the clean-environment test.

## Rename: compat_scripts -> scripts (2026-10-01)
- The folder `compat_scripts` is now `scripts` (Skyrim's own folder is also called Scripts). The word "compat" came from the old project name and
  no longer fits; documentation and comments now say "Skyshim" instead of "互換ランタイム". C++ constants kCompat* became kReported*.
- Build the scripts with `scripts\build_pex.ps1` (see docs\BUILD.md section 8).

## Clean environment verification (2026-10-01, second PC)
- Game folder D:\work\skyrimSEFix with no SKSE scripts; Skyshim Scripts MOD built by scripts\build_pex.ps1 (13 .pex, additions verified) in MO2
  (D:\work\mod_se\mods\Skyshim Scripts\Scripts). User reports it works: SkyUI starts without SKSE .pex files (A1 achieved for the features the
  runtime of that run implemented). The log of that run shows the OLD runtime ("skyui_compat runtime 0.1.0"), i.e. a DLL built before the rename and
  before the favorites equip natives, so those natives are still untested. Map location finder calls ShowOnMap/GetMappedKey/GetLastControl stubs.

## Input completed (2026-10-01)
- skse.GetLastKeycode(isKeyDown): the argument means "last key pressed (true) / released (false)", NOT "reset" (my earlier reading was wrong).
  Tracks the last down and up key codes and control names; skse.GetLastControl(isKeyDown) returns the control name; skse.GetMappedKey(control, device, context)
  returns the key code or -1. Gamepad buttons now map to SKSE codes 266-281 (table in src/events/events.cpp), so MCM key remapping accepts the gamepad
  (only while the gamepad is the active device, like SKSE). Input.GetMappedControl uses the same table. EnableMapMenuMouseWheel is a no-op on purpose.
- Still stubs: ShowOnMap, ExtendData, ForceContainerCategorization, ExtendAlchemyCategories, ExtendForm.

## ExtendData first cut (2026-10-01) - UNTESTED in game
- User report: in the Q (favorites) menu the A/D keys did not move between groups, and items could not be added to groups.
- Cause 1: SkyUI's InputDelegate builds details.skseKeycode from skse.GetLastKeycode(true) / GetLastControl(true); our input sink must run BEFORE the
  menu's input handling -> the sink is now prepended to the BSInputDeviceManager event source (PrependEventSink). The earlier GetLastKeycode also
  cleared the value on read and GetLastControl was a stub.
- Cause 2: favorites group assignment reads assignedEntry.formId / itemId, which come from SKSE's ExtendData. Implemented in src/scaleform/extend_data.cpp:
  vtable hook of IMenu::ProcessMessage (index 4) for Inventory/Container/Barter/Gift/Favorites menus; after the original runs, each list item's AS object gets
  formType, formId, itemId(=0), keywords, armor partMask/weightClass, weapon fields, ammo/potion/book flags, soul gem sizes, then
  skyui_itemDataProcessed is reset and the list's InvalidateData() is called so SkyUI reprocesses it.
- Finding: menus call skse.ExtendData / ExtendAlchemyCategories / EnableMapMenuMouseWheel inside InitExtensions(), which the engine runs while the menu is
  created, i.e. BEFORE our _global.skse injection, so those calls never reached us. InitExtensions must not be re-invoked (it registers callbacks), so
  extend data is treated as always enabled for the hooked menus. ForceContainerCategorization, ExtendAlchemyCategories, MagicMenu data: still TODO.

## Audit of what SkyUI still expects from SKSE (2026-10-01)
Method: decompiled all 48 SkyUI SWFs (FFDec) and grepped every `skse.` call; then read the data setters to see which entry fields SKSE used to provide.
- skse.* calls in the SWFs = the ones already known (no new API). Implemented: SendModEvent, GetMappedKey, GetLastControl/GetLastKeycode, AllowTextInput,
  OpenMenu, Log, StoreIndices/LoadIndices, StartRemapMode, RequestActivePlayerEffects, EnableMapMenuMouseWheel (no-op). Not implemented: ShowOnMap,
  ExtendAlchemyCategories, ForceContainerCategorization (see below); ExtendData is implemented through menu hooks, not through the call.
- ExtendData now also covers: MagicMenu (spells/shouts/powers/effects: formType, formId, keywords, school, skillLevel, archetype, delivery, actorValue,
  castType, magicType, magnitude, duration, area, spellType, equipSlot) and, for potions/scrolls/ingredients in item lists, the same magic fields plus
  useSound (SkyUI uses it to tell food from drinks). SkyUI's fixSKSEExtendedObject() maps subType->school/weaponType and magicType->resistance, so both names are set.
- Crafting menus (alchemy, smithing, enchanting, cooking ...): NOT extended. Each sub-menu keeps its own engine list and CommonLib offers no safe way to map
  an AS entry to its form; guessing by name is unreliable. Alchemy still shows the SkyUI layout.
- ForceContainerCategorization: undecided. A diagnostic logs DIAG_FILTERFLAG (menu, item, form type, filterFlag) for the first container items;
  if every item has the same/zero filterFlag, categories in the container menu will not filter and the flag must be computed natively.
- ShowOnMap: MapCamera internals are unknown in CommonLib (unk fields only), so no verified way to move the map camera. Left as a stub.

## Container categorization decided (2026-10-02)
- User confirmed in game: container categories filter the items correctly, the magic menu columns (school / level) and the potion details show.
  => ForceContainerCategorization is NOT needed (the engine already provides filterFlag); the diagnostic was removed. The call stays a harmless stub.
- Remaining stubs/limits: ShowOnMap (map camera internals unknown), ExtendAlchemyCategories, crafting-menu extended data, ExtendForm (unused by SkyUI).

## ShowOnMap, alchemy categories, crafting extended data (2026-10-02) - UNTESTED in game
- Found: SKSE's own source (src\skse\...\Hooks_Scaleform.cpp, kept as a behaviour reference) shows how each was done, and CommonLib has the needed structures.
- ShowOnMap(index): same as SKSE - marker index -> mapMarkers[index].ref -> RefHandleUIData message (UI_MESSAGE_TYPE::kUpdate) to "MapMenu".
- Crafting extended data (src/scaleform/extend_data.cpp ExtendCrafting): hook of CraftingMenu::ProcessMessage; sub-menu type found by its vtable
  (ConstructibleObjectMenu: recipes[i].constructibleObject->createdItem; SmithingMenu: recipes[i].item; AlchemyMenu: ingredientEntries[i].ingredient).
  The AS entryList and the native array are assumed to be in the same order; if the counts differ nothing is added (log CRAFTING_COUNT_MISMATCH).
  EnchantConstructMenu is not extended (SKSE did not extend it either).
- ExtendAlchemyCategories: instead of rewriting the arguments of SetCategoriesList (SKSE), the category entries (named after the effects) are fixed after the
  fact: the icon (beneficial / harmful / other) is set from the effect's archetype and detrimental flag, found by matching the effect name against the
  ingredients' effects (FixAlchemyCategories). Assumes craftingMenu.InventoryLists.CategoriesList.entryList exists.
- ForceContainerCategorization / ExtendData / ExtendAlchemyCategories / ExtendForm as skse.* calls are intentional no-ops now (the stub logging was removed).

## Crash after the second game load (2026-10-02, second PC)
- ShowOnMap moves the map, alchemy categories and crafting data work (user). Then the game crashed: EXCEPTION C0000005 at SkyrimSE.exe+0x289D3C, right after a
  second WidgetLoader.loadWidget (i.e. a second save load).
- That address is inside the 32-byte function at +0x289D30 (Address Library id 19154): `mov rcx,[rcx+40h]; test; je; mov rax,[rcx]; jmp [rax+0E8h]`.
  The jmp faulted: the object at this+0x40 had a dangling vtable pointer (use after free).
- Suspected cause (not yet confirmed): our ProcessMessage hooks touch a menu's list/sub-menu after the original ProcessMessage returned, also while the menu is
  being hidden/destroyed (save load). Mitigation: the hooks now run only while UI::IsMenuOpen(menu) is true. The crash logger now also prints return addresses
  found on the stack (STACK[n] SkyrimSE.exe+0x... / skyshim.dll+0x...) so the next crash shows the call path.
- Ask the user: which operation crashed (second load? closing a menu? exiting?), and whether it repeats.
- Result (user, second PC): the same operation no longer crashes after the IsMenuOpen guard. Tools added: tools\addrlib_lookup.ps1 (RVA -> Address Library id) and
  tools\disasm.bat (dumpbin disassembly of SkyrimSE.exe). See docs\PROBLEMS.md problem 21.

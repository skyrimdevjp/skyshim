#pragma once
#include <cstdint>
#include <string>

namespace RE { class TESForm; class GFxValue; }

// Phase 6: Papyrus event registrations (ModEvent / Menu / Key) and their dispatch to the Papyrus VM.
namespace skyshim::events
{
	void RegisterModEvent(RE::TESForm* a_form, const std::string& a_event, const std::string& a_callback);
	void UnregisterModEvent(RE::TESForm* a_form, const std::string& a_event);
	void SendModEvent(RE::TESForm* a_sender, const std::string& a_event, const std::string& a_str, float a_num);

	void RegisterMenu(RE::TESForm* a_form, const std::string& a_menu);
	void UnregisterMenu(RE::TESForm* a_form, const std::string& a_menu);

	void RegisterKey(RE::TESForm* a_form, std::int32_t a_key);
	void UnregisterKey(RE::TESForm* a_form, std::int32_t a_key);

	// Remap mode (MCM key mapping): the next key press is delivered to a_target.EndRemapMode(keyCode) on the main thread.
	void StartRemap(const RE::GFxValue& a_target);

	// 最後に押した(a_isDown が真)、または離したキーの番号(SKSE のキー番号。無ければ 0)。
	std::int32_t LastKeycode(bool a_isDown);
	// 最後に押した、または離したキーの操作名。
	std::string LastControl(bool a_isDown);
	// 操作名に割り当てられたキーの番号(割り当てが無ければ -1)。a_device: 0 = キーボード、1 = マウス、2 = ゲームパッド。
	std::int32_t MappedKey(const std::string& a_control, std::int32_t a_device, std::int32_t a_context);
	// キー番号に割り当てられた操作名(ゲームプレイ時)。割り当てが無ければ空。
	std::string MappedControl(std::int32_t a_keycode);

	// Adds the menu-open/close and input event sinks. Call once, when the main menu is open.
	void InstallSinks();
}

#pragma once
#include <cstdint>
#include <string>

namespace RE { class TESForm; class GFxValue; }

// Phase 6: Papyrus event registrations (ModEvent / Menu / Key) and their dispatch to the Papyrus VM.
namespace skyui_compat::events
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

	// Key code of the last key/mouse button pressed (0 if none); a_reset clears it.
	std::int32_t LastKeycode(bool a_reset);

	// Adds the menu-open/close and input event sinks. Call once, when the main menu is open.
	void InstallSinks();
}

#pragma once

// Phase 7: provides the ActionScript global `_global.skse` in every Scaleform menu movie.
namespace skyui_compat::scaleform
{
	// Wraps every registered menu's creation function so `_global.skse` is injected right after the menu (and its movie) is created.
	// Call once, when the main menu is open (all menus are registered by then).
	bool InstallMenuWrappers(void (*a_log)(const char*, ...));
}

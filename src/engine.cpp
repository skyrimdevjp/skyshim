// Phase 3: REL / Address Library / engine singletons via CommonLib (MIT snapshot).
#include "engine.h"
#include "events/events.h"
#include "mainthread.h"
#include "scaleform/inject.h"

#include "RE/Skyrim.h"
#include "REL/Relocation.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace skyui_compat::engine
{
	namespace
	{
		template <class T>
		bool Ready(T* (*a_get)()) { return a_get() != nullptr; }
	}

	namespace { LogFn g_logFn = nullptr; }

	void LogCall(const char* a_name) { if (g_logFn) g_logFn("PAPYRUS_CALL %s", a_name); }

	bool Phase3Init(LogFn a_log)
	{
		g_logFn = a_log;
		const auto v = REL::Module::get().version();
		a_log("REL_MODULE=PASS base=%p version=%s", reinterpret_cast<void*>(REL::Module::get().base()), v.string().c_str());

		// Loading the database happens on first ID use; report_and_fail aborts on a missing file.
		REL::Relocation<std::uintptr_t> probe{ RELOCATION_ID(514178, 400327) };  // UI singleton pointer
		a_log("ADDRESS_LIBRARY=PASS");
		a_log("KNOWN_ID_RESOLVE=PASS id(UI singleton)=%llu addr=%p", 514178ull, reinterpret_cast<void*>(probe.address()));
		skyui_compat::mainthread::Install(a_log);  // per-frame main-thread task pump (needed by UI.* natives)
		return true;
	}

	void WaitForSingletons(LogFn a_log)
	{
		bool vm = false, ui = false, input = false, natives = false, tr = false, dumped = false;
		std::chrono::steady_clock::time_point trTime{};
		const auto start = std::chrono::steady_clock::now();
		while (std::chrono::steady_clock::now() - start < std::chrono::seconds(300)) {
			if (!ui && RE::UI::GetSingleton()) { ui = true; a_log("MENU_MANAGER=PASS (RE::UI)"); }
			if (!input && RE::BSInputDeviceManager::GetSingleton()) { input = true; a_log("INPUT_MANAGER=PASS"); }
			if (!vm && RE::SkyrimVM::GetSingleton() && RE::SkyrimVM::GetSingleton()->impl.get()) { vm = true; a_log("PAPYRUS_VM_POINTER=PASS"); }

			// Natives are registered as soon as the VM exists (scripts must not run before this).
			if (!natives && ui && input && vm) {
				natives = true;
				const bool ok = skyui_compat::papyrus::RegisterAll(RE::SkyrimVM::GetSingleton()->impl.get());
				a_log("PAPYRUS_NATIVES_REGISTERED=%s", ok ? "PASS" : "FAIL");
			}

			// Translations only once the main menu is open: the GFx loader/translator is then fully constructed
			// (importing right after the singletons appeared crashed the game).
			if (!tr && ui) {
				if (auto* u = RE::UI::GetSingleton(); u && u->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
					tr = ImportModTranslations(a_log);
					skyui_compat::events::InstallSinks();  // menu-open/close and input sinks (Phase 6)
					skyui_compat::scaleform::InstallMenuWrappers(a_log);
					a_log("EVENT_SINKS_INSTALLED=PASS (menu open/close, input)");
				}
			}

			// Debug aid (off): DumpTranslations(a_log) logs selected translation table entries; post it to the main thread when needed.
			(void)dumped;
			(void)trTime;
			if (natives && tr) return;
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
		a_log("PHASE3_TIMEOUT ui=%d input=%d vm=%d natives=%d translations=%d", ui, input, vm, natives, tr);
	}
}

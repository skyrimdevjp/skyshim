#pragma once

namespace RE::BSScript { class IVirtualMachine; }
namespace skyshim::papyrus { bool RegisterAll(RE::BSScript::IVirtualMachine*); bool RegisterUI(RE::BSScript::IVirtualMachine*); bool RegisterEquip(RE::BSScript::IVirtualMachine*); }

namespace skyshim::engine
{
	using LogFn = void (*)(const char*, ...);

	void LogCall(const char* a_name);      // logs "PAPYRUS_CALL <name>" via the runtime logger
	bool ImportModTranslations(LogFn a_log);  // true once the GFx translator exists (call until true)
	void DumpTranslations(LogFn a_log);        // diagnostic: log selected translation table entries
	bool Phase3Init(LogFn a_log);        // REL + Address Library + known ID
	void WaitForSingletons(LogFn a_log);  // blocks (run on a worker thread)
}

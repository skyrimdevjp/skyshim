// Phase 4: Papyrus native registration (SKSE.GetVersion*, Form.GetType) via CommonLib MIT IVirtualMachine.
#include "../engine.h"
#include "../events/events.h"

#include "RE/Skyrim.h"

#include <atomic>
#include <cstring>
#include <string>
#include <string_view>

namespace skyshim::papyrus
{
	namespace
	{
		// Logs the first call of each native so a real script run leaves evidence (SKI_MAIN_SKSE_CHECK).
		void Trace(const char* a_name) { static std::atomic<int> n{ 0 }; if (n++ < 20) engine::LogCall(a_name); }

		// SkyUI only compares against MinSKSERelease (53); report a release number that satisfies it, not a real SKSE identity.
		constexpr std::int32_t kReportedVersion = 2, kReportedMinor = 2, kReportedBeta = 0, kReportedRelease = 75;

		std::int32_t GetVersion(RE::StaticFunctionTag*) { return kReportedVersion; }
		std::int32_t GetVersionMinor(RE::StaticFunctionTag*) { return kReportedMinor; }
		std::int32_t GetVersionBeta(RE::StaticFunctionTag*) { return kReportedBeta; }
		std::int32_t GetVersionRelease(RE::StaticFunctionTag*) { Trace("SKSE.GetVersionRelease"); return kReportedRelease; }
		std::int32_t GetScriptVersionRelease(RE::StaticFunctionTag*) { return kReportedRelease; }

		// SKI_Main only tests for != 0 (scripts properly loaded); return the real form type.
		std::int32_t FormGetType(RE::TESForm* a_self) { Trace("Form.GetType"); return a_self ? static_cast<std::int32_t>(a_self->GetFormType()) : 0; }

		// Form event registration (Phase 6). Text arguments arrive as BSFixedString.
		std::string S(const RE::BSFixedString& a_s) { return a_s.data() ? std::string(a_s.c_str()) : std::string(); }
		void FormRegisterForModEvent(RE::TESForm* a_self, RE::BSFixedString a_event, RE::BSFixedString a_callback) { events::RegisterModEvent(a_self, S(a_event), S(a_callback)); }
		void FormUnregisterForModEvent(RE::TESForm* a_self, RE::BSFixedString a_event) { events::UnregisterModEvent(a_self, S(a_event)); }
		void FormSendModEvent(RE::TESForm* a_self, RE::BSFixedString a_event, RE::BSFixedString a_str, float a_num) { events::SendModEvent(a_self, S(a_event), S(a_str), a_num); }
		void FormRegisterForMenu(RE::TESForm* a_self, RE::BSFixedString a_menu) { events::RegisterMenu(a_self, S(a_menu)); }
		void FormUnregisterForMenu(RE::TESForm* a_self, RE::BSFixedString a_menu) { events::UnregisterMenu(a_self, S(a_menu)); }
		void FormRegisterForKey(RE::TESForm* a_self, std::int32_t a_key) { events::RegisterKey(a_self, a_key); }
		void FormUnregisterForKey(RE::TESForm* a_self, std::int32_t a_key) { events::UnregisterKey(a_self, a_key); }

		// Utility.GetINI*: INISettingCollection lookup by "name:Section" (SkyUI: Papyrus/Interface/Display keys).
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
		std::int32_t GetINIInt(RE::StaticFunctionTag*, RE::BSFixedString a_key)
		{
			auto* s = FindINI(a_key);
			return s ? (s->GetType() == RE::Setting::Type::kFloat ? static_cast<std::int32_t>(s->GetFloat()) : s->GetSInt()) : 0;
		}
		float GetINIFloat(RE::StaticFunctionTag*, RE::BSFixedString a_key)
		{
			auto* s = FindINI(a_key);
			return s ? (s->GetType() == RE::Setting::Type::kFloat ? s->GetFloat() : static_cast<float>(s->GetSInt())) : 0.0f;
		}

		// Math additions (SKSE): bitwise helpers. Shifts outside 0-31 give 0 (or the sign for right shifts).
		std::int32_t MathLeftShift(RE::StaticFunctionTag*, std::int32_t a_value, std::int32_t a_by) { return (a_by < 0 || a_by > 31) ? 0 : static_cast<std::int32_t>(static_cast<std::uint32_t>(a_value) << a_by); }
		std::int32_t MathRightShift(RE::StaticFunctionTag*, std::int32_t a_value, std::int32_t a_by) { return (a_by < 0 || a_by > 31) ? (a_value < 0 ? -1 : 0) : (a_value >> a_by); }
		std::int32_t MathLogicalAnd(RE::StaticFunctionTag*, std::int32_t a, std::int32_t b) { return a & b; }
		std::int32_t MathLogicalOr(RE::StaticFunctionTag*, std::int32_t a, std::int32_t b) { return a | b; }
		std::int32_t MathLogicalXor(RE::StaticFunctionTag*, std::int32_t a, std::int32_t b) { return a ^ b; }
		std::int32_t MathLogicalNot(RE::StaticFunctionTag*, std::int32_t a) { return ~a; }

		// StringUtil (byte semantics, like SKSE).
		std::int32_t StrGetLength(RE::StaticFunctionTag*, RE::BSFixedString a_str) { return a_str.data() ? static_cast<std::int32_t>(std::strlen(a_str.c_str())) : 0; }
		std::string StrSubstring(RE::StaticFunctionTag*, RE::BSFixedString a_str, std::int32_t a_start, std::int32_t a_len)
		{
			const std::string_view s = a_str.data() ? std::string_view{ a_str.c_str() } : std::string_view{};
			if (a_start < 0 || static_cast<std::size_t>(a_start) > s.size()) return {};
			return std::string{ a_len < 0 ? s.substr(a_start) : s.substr(a_start, a_len) };
		}
	}

	bool RegisterAll(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) return false;
		a_vm->RegisterFunction("GetVersion", "SKSE", GetVersion);
		a_vm->RegisterFunction("GetVersionMinor", "SKSE", GetVersionMinor);
		a_vm->RegisterFunction("GetVersionBeta", "SKSE", GetVersionBeta);
		a_vm->RegisterFunction("GetVersionRelease", "SKSE", GetVersionRelease);
		a_vm->RegisterFunction("GetScriptVersionRelease", "SKSE", GetScriptVersionRelease);
		a_vm->RegisterFunction("GetType", "Form", FormGetType);
		a_vm->RegisterFunction("GetINIInt", "Utility", GetINIInt);
		a_vm->RegisterFunction("GetINIFloat", "Utility", GetINIFloat);
		a_vm->RegisterFunction("RegisterForModEvent", "Form", FormRegisterForModEvent);
		a_vm->RegisterFunction("UnregisterForModEvent", "Form", FormUnregisterForModEvent);
		a_vm->RegisterFunction("SendModEvent", "Form", FormSendModEvent);
		a_vm->RegisterFunction("RegisterForMenu", "Form", FormRegisterForMenu);
		a_vm->RegisterFunction("UnregisterForMenu", "Form", FormUnregisterForMenu);
		a_vm->RegisterFunction("RegisterForKey", "Form", FormRegisterForKey);
		a_vm->RegisterFunction("UnregisterForKey", "Form", FormUnregisterForKey);
		a_vm->RegisterFunction("LeftShift", "Math", MathLeftShift);
		a_vm->RegisterFunction("RightShift", "Math", MathRightShift);
		a_vm->RegisterFunction("LogicalAnd", "Math", MathLogicalAnd);
		a_vm->RegisterFunction("LogicalOr", "Math", MathLogicalOr);
		a_vm->RegisterFunction("LogicalXor", "Math", MathLogicalXor);
		a_vm->RegisterFunction("LogicalNot", "Math", MathLogicalNot);
		a_vm->RegisterFunction("GetLength", "StringUtil", StrGetLength);
		a_vm->RegisterFunction("Substring", "StringUtil", StrSubstring);
		return RegisterUI(a_vm) && RegisterEquip(a_vm);
	}
}

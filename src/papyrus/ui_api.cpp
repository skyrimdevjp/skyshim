// Phase 5: UI Papyrus natives. Writes/invocations run on the main thread (mainthread::Post); reads run on the calling thread.
#include "../engine.h"
#include "../mainthread.h"

#include "RE/Skyrim.h"

#include <string>
#include <vector>

namespace skyshim::papyrus
{
	namespace
	{
		std::string S(const RE::BSFixedString& a_s) { return a_s.data() ? std::string(a_s.c_str()) : std::string(); }

		RE::GPtr<RE::GFxMovieView> Movie(const std::string& a_menu)
		{
			auto* ui = RE::UI::GetSingleton();
			return ui ? ui->GetMovieView(a_menu) : nullptr;
		}

		bool IsMenuOpen(RE::StaticFunctionTag*, RE::BSFixedString a_menu)
		{
			auto* ui = RE::UI::GetSingleton();
			return ui && a_menu.data() && ui->IsMenuOpen(a_menu.c_str());
		}

		bool IsTextInputEnabled(RE::StaticFunctionTag*)
		{
			auto* map = RE::ControlMap::GetSingleton();
			return map && map->textEntryCount > 0;
		}

		void SetNumber(const std::string& a_menu, const std::string& a_target, double a_value)
		{
			mainthread::Post([a_menu, a_target, a_value] {
				if (auto mv = Movie(a_menu)) mv->SetVariable(a_target.c_str(), RE::GFxValue(a_value));
			});
		}

		void SetInt(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, std::int32_t a_value) { SetNumber(S(a_menu), S(a_target), a_value); }
		void SetFloat(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, float a_value) { SetNumber(S(a_menu), S(a_target), a_value); }
		void SetString(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, RE::BSFixedString a_value)
		{
			mainthread::Post([m = S(a_menu), t = S(a_target), v = S(a_value)] {
				if (auto mv = Movie(m)) mv->SetVariable(t.c_str(), RE::GFxValue(v.c_str()));
			});
		}

		std::int32_t GetInt(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target)
		{
			RE::GFxValue v;
			auto mv = Movie(S(a_menu));
			return (mv && mv->GetVariable(&v, S(a_target).c_str()) && v.IsNumber()) ? static_cast<std::int32_t>(v.GetNumber()) : 0;
		}

		std::string GetString(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target)
		{
			RE::GFxValue v;
			auto mv = Movie(S(a_menu));
			return (mv && mv->GetVariable(&v, S(a_target).c_str()) && v.IsString() && v.GetString()) ? std::string(v.GetString()) : std::string();
		}

		void InvokeArgs(std::string a_menu, std::string a_target, std::vector<RE::GFxValue> a_args)
		{
			mainthread::Post([m = std::move(a_menu), t = std::move(a_target), args = std::move(a_args)] {
				if (auto mv = Movie(m)) mv->Invoke(t.c_str(), nullptr, args.empty() ? nullptr : args.data(), static_cast<std::uint32_t>(args.size()));
			});
		}

		// A string GFxValue points at caller memory, so string arguments are kept alive by the task's captured std::string copies.
		void Invoke(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target) { InvokeArgs(S(a_menu), S(a_target), {}); }
		void InvokeBool(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, bool a_arg) { InvokeArgs(S(a_menu), S(a_target), { RE::GFxValue(a_arg) }); }
		void InvokeInt(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, std::int32_t a_arg) { InvokeArgs(S(a_menu), S(a_target), { RE::GFxValue(static_cast<double>(a_arg)) }); }
		void InvokeFloat(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, float a_arg) { InvokeArgs(S(a_menu), S(a_target), { RE::GFxValue(static_cast<double>(a_arg)) }); }

		// Strings: copy into stable storage that outlives the queued task.
		void InvokeStrings(std::string a_menu, std::string a_target, std::vector<std::string> a_strings)
		{
			mainthread::Post([m = std::move(a_menu), t = std::move(a_target), s = std::move(a_strings)] {
				auto mv = Movie(m);
				if (!mv) return;
				std::vector<RE::GFxValue> args;
				args.reserve(s.size());
				for (const auto& str : s) args.emplace_back(str.c_str());
				mv->Invoke(t.c_str(), nullptr, args.empty() ? nullptr : args.data(), static_cast<std::uint32_t>(args.size()));
			});
		}

		void InvokeString(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, RE::BSFixedString a_arg) { InvokeStrings(S(a_menu), S(a_target), { S(a_arg) }); }

		// Array variants pass each element as a separate ActionScript argument (SKSE behaviour).
		void InvokeIntA(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, std::vector<std::int32_t> a_args)
		{
			std::vector<RE::GFxValue> v;
			for (auto a : a_args) v.emplace_back(static_cast<double>(a));
			InvokeArgs(S(a_menu), S(a_target), std::move(v));
		}
		void InvokeFloatA(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, std::vector<float> a_args)
		{
			std::vector<RE::GFxValue> v;
			for (auto a : a_args) v.emplace_back(static_cast<double>(a));
			InvokeArgs(S(a_menu), S(a_target), std::move(v));
		}
		void InvokeStringA(RE::StaticFunctionTag*, RE::BSFixedString a_menu, RE::BSFixedString a_target, std::vector<RE::BSFixedString> a_args)
		{
			std::vector<std::string> v;
			for (auto& a : a_args) v.push_back(S(a));
			InvokeStrings(S(a_menu), S(a_target), std::move(v));
		}
	}

	bool RegisterUI(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) return false;
		a_vm->RegisterFunction("IsMenuOpen", "UI", IsMenuOpen);
		a_vm->RegisterFunction("IsTextInputEnabled", "UI", IsTextInputEnabled);
		a_vm->RegisterFunction("SetInt", "UI", SetInt);
		a_vm->RegisterFunction("SetFloat", "UI", SetFloat);
		a_vm->RegisterFunction("SetString", "UI", SetString);
		a_vm->RegisterFunction("GetInt", "UI", GetInt);
		a_vm->RegisterFunction("GetString", "UI", GetString);
		a_vm->RegisterFunction("Invoke", "UI", Invoke);
		a_vm->RegisterFunction("InvokeBool", "UI", InvokeBool);
		a_vm->RegisterFunction("InvokeInt", "UI", InvokeInt);
		a_vm->RegisterFunction("InvokeFloat", "UI", InvokeFloat);
		a_vm->RegisterFunction("InvokeString", "UI", InvokeString);
		a_vm->RegisterFunction("InvokeIntA", "UI", InvokeIntA);
		a_vm->RegisterFunction("InvokeFloatA", "UI", InvokeFloatA);
		a_vm->RegisterFunction("InvokeStringA", "UI", InvokeStringA);
		return true;
	}
}

#include "events.h"

#include "mod_event.h"
#include "../mainthread.h"

#include "RE/Skyrim.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

namespace skyshim::events
{
	namespace
	{
		ModEventRegistry g_modEvents;

		std::mutex g_mutex;
		std::unordered_map<std::string, std::set<std::uint64_t>> g_menus;  // menu name -> object handles
		std::unordered_map<std::int32_t, std::set<std::uint64_t>> g_keys;  // key code  -> object handles

		// Remap mode (MCM key mapping) and last pressed key. Guarded by g_mutex.
		std::shared_ptr<RE::GFxValue>         g_remapTarget;
		std::chrono::steady_clock::time_point g_remapStart;
		std::int32_t                          g_lastKey = 0;

		RE::BSScript::IVirtualMachine* VM()
		{
			auto* svm = RE::SkyrimVM::GetSingleton();
			return svm ? svm->impl.get() : nullptr;
		}

		// 0 means "no handle" (invalid).
		std::uint64_t HandleOf(RE::TESForm* a_form)
		{
			auto* vm = VM();
			if (!vm || !a_form) return 0;
			auto* policy = vm->GetObjectHandlePolicy();
			return policy ? policy->GetHandleForObject(a_form->GetFormType(), a_form) : 0;
		}

		template <class... Args>
		void Send(std::uint64_t a_handle, const std::string& a_function, Args&&... a_args)
		{
			if (auto* vm = VM()) vm->SendEvent(a_handle, RE::BSFixedString(a_function.c_str()), RE::MakeFunctionArguments(std::forward<Args>(a_args)...));
		}

		// Device offsets follow the SKSE key-code convention: keyboard 0-255, mouse 256+.
		std::int32_t KeyCode(const RE::ButtonEvent* a_button)
		{
			const auto id = static_cast<std::int32_t>(a_button->GetIDCode());
			switch (a_button->GetDevice()) {
			case RE::INPUT_DEVICE::kKeyboard: return id;
			case RE::INPUT_DEVICE::kMouse:    return 256 + id;
			default:                          return -1;  // gamepad: not needed by SkyUI yet
			}
		}

		// Records the key and, if remap mode is waiting, delivers it. Ignores presses right after StartRemapMode so the key that
		// opened the dialog (Enter / mouse click) is not taken as the new binding.
		void OnKeyDownForRemap(std::int32_t a_key)
		{
			std::shared_ptr<RE::GFxValue> target;
			{
				std::lock_guard l(g_mutex);
				g_lastKey = a_key;
				if (g_remapTarget && std::chrono::steady_clock::now() - g_remapStart > std::chrono::milliseconds(250)) target = std::move(g_remapTarget);
			}
			if (target) {
				mainthread::Post([target, a_key] {
					const RE::GFxValue arg(static_cast<double>(a_key));
					target->Invoke("EndRemapMode", nullptr, &arg, 1);
				});
			}
		}

		class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event) return RE::BSEventNotifyControl::kContinue;
				std::set<std::uint64_t> targets;
				{
					std::lock_guard l(g_mutex);
					if (auto it = g_menus.find(a_event->menuName.c_str()); it != g_menus.end()) targets = it->second;
				}
				for (auto h : targets) Send(h, a_event->opening ? "OnMenuOpen" : "OnMenuClose", std::string(a_event->menuName.c_str()));
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class InputSink : public RE::BSTEventSink<RE::InputEvent*>
		{
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!a_event) return RE::BSEventNotifyControl::kContinue;
				for (auto* e = *a_event; e; e = e->next) {
					auto* button = e->AsButtonEvent();
					if (!button || !(button->IsDown() || button->IsUp())) continue;
					const auto key = KeyCode(button);
					if (key < 0) continue;
					if (button->IsDown()) OnKeyDownForRemap(key);
					std::set<std::uint64_t> targets;
					{
						std::lock_guard l(g_mutex);
						if (auto it = g_keys.find(key); it != g_keys.end()) targets = it->second;
					}
					for (auto h : targets) {
						if (button->IsDown()) Send(h, "OnKeyDown", static_cast<std::int32_t>(key));
						else Send(h, "OnKeyUp", static_cast<std::int32_t>(key), static_cast<float>(button->HeldDuration()));
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		MenuSink  g_menuSink;
		InputSink g_inputSink;
	}

	void RegisterModEvent(RE::TESForm* a_form, const std::string& a_event, const std::string& a_callback)
	{
		if (const auto h = HandleOf(a_form)) g_modEvents.Register(a_event, h, a_callback);
	}

	void UnregisterModEvent(RE::TESForm* a_form, const std::string& a_event)
	{
		if (const auto h = HandleOf(a_form)) g_modEvents.Unregister(a_event, h);
	}

	// SKSE passes (eventName, strArg, numArg, sender) to the callback each object registered.
	void SendModEvent(RE::TESForm* a_sender, const std::string& a_event, const std::string& a_str, float a_num)
	{
		for (const auto& r : g_modEvents.Snapshot(a_event))
			Send(r.handle, r.callback, std::string(a_event), std::string(a_str), static_cast<float>(a_num), static_cast<RE::TESForm*>(a_sender));
	}

	void RegisterMenu(RE::TESForm* a_form, const std::string& a_menu)
	{
		if (const auto h = HandleOf(a_form)) { std::lock_guard l(g_mutex); g_menus[a_menu].insert(h); }
	}

	void UnregisterMenu(RE::TESForm* a_form, const std::string& a_menu)
	{
		if (const auto h = HandleOf(a_form)) { std::lock_guard l(g_mutex); g_menus[a_menu].erase(h); }
	}

	void RegisterKey(RE::TESForm* a_form, std::int32_t a_key)
	{
		if (const auto h = HandleOf(a_form)) { std::lock_guard l(g_mutex); g_keys[a_key].insert(h); }
	}

	void UnregisterKey(RE::TESForm* a_form, std::int32_t a_key)
	{
		if (const auto h = HandleOf(a_form)) { std::lock_guard l(g_mutex); g_keys[a_key].erase(h); }
	}

	void InstallSinks()
	{
		if (auto* ui = RE::UI::GetSingleton()) ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuSink);
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) input->AddEventSink(&g_inputSink);
	}
}

namespace skyshim::events
{
	void StartRemap(const RE::GFxValue& a_target)
	{
		std::lock_guard l(g_mutex);
		g_remapTarget = std::make_shared<RE::GFxValue>(a_target);
		g_remapStart = std::chrono::steady_clock::now();
	}

	std::int32_t LastKeycode(bool a_reset)
	{
		std::lock_guard l(g_mutex);
		const auto k = g_lastKey;
		if (a_reset) g_lastKey = 0;
		return k;
	}
}

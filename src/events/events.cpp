#include "events.h"

#include "mod_event.h"
#include "../mainthread.h"
#include "../vm_layout.h"

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
		// 最後に押した、または離したキーの番号と操作名(SkyUI は skse.GetLastKeycode / GetLastControl で読む)。
		std::int32_t g_lastKeyDown = 0, g_lastKeyUp = 0;
		std::string  g_lastControlDown, g_lastControlUp;

		RE::BSScript::IVirtualMachine* VM()
		{
			auto* svm = RE::SkyrimVM::GetSingleton();
			return skyshim::VMImpl(svm);
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

		// SKSE のキー番号: キーボード 0〜255、マウス 256〜265(ボタンとホイール)、ゲームパッド 266〜281。
		constexpr std::int32_t kMouseOffset = 256, kGamepadOffset = 266, kMaxKeycode = 282;
		// ゲームパッドのボタン(XInput のビットマスク)を、番号の順に並べた表(LT と RT は、0x9 と 0xA)。
		constexpr std::uint32_t kPadMasks[16] = { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
			0x0100, 0x0200, 0x1000, 0x2000, 0x4000, 0x8000, 0x0009, 0x000A };

		std::int32_t KeycodeOf(RE::INPUT_DEVICE a_device, std::uint32_t a_id)
		{
			switch (a_device) {
			case RE::INPUT_DEVICE::kKeyboard: return a_id < 256 ? static_cast<std::int32_t>(a_id) : -1;
			case RE::INPUT_DEVICE::kMouse:    return a_id < 10 ? kMouseOffset + static_cast<std::int32_t>(a_id) : -1;
			case RE::INPUT_DEVICE::kGamepad:
				for (std::int32_t i = 0; i < 16; ++i)
					if (kPadMasks[i] == a_id) return kGamepadOffset + i;
				return -1;
			default: return -1;
			}
		}

		// 逆変換(キー番号から、デバイスと ID を求める)。変換できなければ false。
		bool DeviceAndId(std::int32_t a_keycode, RE::INPUT_DEVICE& a_device, std::uint32_t& a_id)
		{
			if (a_keycode < 0 || a_keycode >= kMaxKeycode) return false;
			if (a_keycode < kMouseOffset) {
				a_device = RE::INPUT_DEVICE::kKeyboard;
				a_id = static_cast<std::uint32_t>(a_keycode);
			} else if (a_keycode < kGamepadOffset) {
				a_device = RE::INPUT_DEVICE::kMouse;
				a_id = static_cast<std::uint32_t>(a_keycode - kMouseOffset);
			} else {
				a_device = RE::INPUT_DEVICE::kGamepad;
				a_id = kPadMasks[a_keycode - kGamepadOffset];
			}
			return true;
		}

		std::int32_t KeyCode(const RE::ButtonEvent* a_button) { return KeycodeOf(a_button->GetDevice(), a_button->GetIDCode()); }

		// 押されたキーを記録し、割り当て待ちなら、そのキーを渡す。割り当てを始めた直後の入力は無視する
		// (割り当てを開いた Enter やクリックを、割り当て先と取り違えないため)。
		void OnKeyDownForRemap(std::int32_t a_key, bool a_isGamepad, const std::string& a_control)
		{
			std::shared_ptr<RE::GFxValue> target;
			{
				std::lock_guard l(g_mutex);
				g_lastKeyDown = a_key;
				g_lastControlDown = a_control;
				// SKSE と同じく、ゲームパッドを使っている間は、ゲームパッドの入力だけを、そうでなければ、キーボードとマウスの入力だけを受け取る。
				auto* devices = RE::BSInputDeviceManager::GetSingleton();
				const bool padMode = devices && devices->IsGamepadEnabled();
				if (g_remapTarget && padMode == a_isGamepad && std::chrono::steady_clock::now() - g_remapStart > std::chrono::milliseconds(250))
					target = std::move(g_remapTarget);
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
					const std::string control = button->QUserEvent().c_str() ? button->QUserEvent().c_str() : "";
					if (button->IsDown()) {
						OnKeyDownForRemap(key, button->GetDevice() == RE::INPUT_DEVICE::kGamepad, control);
					} else {
						std::lock_guard l(g_mutex);
						g_lastKeyUp = key;
						g_lastControlUp = control;
					}
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
		// 入力は、リストの先頭に追加する。SkyUI の入力処理は、メニューへのキー入力のたびに skse.GetLastKeycode / GetLastControl を読む。
		// そのため、「最後に押したキー」の記録が、メニューの入力処理より先に更新されている必要がある。
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) static_cast<RE::BSTEventSource<RE::InputEvent*>*>(input)->PrependEventSink(&g_inputSink);
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

	// 最後に押した(a_isDown が真)、または離したキーの番号。
	std::int32_t LastKeycode(bool a_isDown)
	{
		std::lock_guard l(g_mutex);
		return a_isDown ? g_lastKeyDown : g_lastKeyUp;
	}

	// 最後に押した、または離したキーの操作名(例: "Jump")。
	std::string LastControl(bool a_isDown)
	{
		std::lock_guard l(g_mutex);
		return a_isDown ? g_lastControlDown : g_lastControlUp;
	}

	// 操作名に割り当てられているキーの番号。割り当てが無ければ -1。
	// a_device: 0 = キーボード、1 = マウス、2 = ゲームパッド。a_context: 入力の場面(0 = ゲームプレイ)。
	std::int32_t MappedKey(const std::string& a_control, std::int32_t a_device, std::int32_t a_context)
	{
		auto* map = RE::ControlMap::GetSingleton();
		if (!map || a_device < 0 || a_device > 2 || a_context < 0 || a_context >= static_cast<std::int32_t>(RE::UserEvents::INPUT_CONTEXT_ID::kTotal)) return -1;
		const auto device = static_cast<RE::INPUT_DEVICE>(a_device);
		const auto id = map->GetMappedKey(a_control, device, static_cast<RE::UserEvents::INPUT_CONTEXT_ID>(a_context));
		return id == 0xFF ? -1 : KeycodeOf(device, id);
	}

	// キー番号に割り当てられている操作名(ゲームプレイ時)。割り当てが無ければ空。
	std::string MappedControl(std::int32_t a_keycode)
	{
		auto* map = RE::ControlMap::GetSingleton();
		RE::INPUT_DEVICE device{};
		std::uint32_t    id = 0;
		if (!map || !DeviceAndId(a_keycode, device, id)) return {};
		return std::string(map->GetUserEventName(id, device, RE::UserEvents::INPUT_CONTEXT_ID::kGameplay));
	}
}

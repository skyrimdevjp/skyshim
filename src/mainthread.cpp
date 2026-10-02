#include "mainthread.h"

#include "RE/Skyrim.h"
#include "SKSE/Trampoline.h"

#include <mutex>
#include <vector>

namespace skyshim::mainthread
{
	namespace
	{
		std::mutex                          g_mutex;
		std::vector<std::function<void()>> g_tasks;
		std::uintptr_t                      g_original = 0;
		std::vector<std::pair<int, std::function<void()>>> g_delayed;  // frames left, task

		// Same signature as the original call target: void().
		void Hook()
		{
			reinterpret_cast<void (*)()>(g_original)();

			std::vector<std::function<void()>> tasks;
			{
				std::lock_guard l(g_mutex);
				tasks.swap(g_tasks);
			}
			std::vector<std::function<void()>> due;
			{
				std::lock_guard l(g_mutex);
				for (auto it = g_delayed.begin(); it != g_delayed.end();) {
					if (--it->first <= 0) { due.push_back(std::move(it->second)); it = g_delayed.erase(it); } else ++it;
				}
			}
			for (auto& t : tasks) t();
			for (auto& t : due) t();
		}
	}

	bool Install(void (*a_log)(const char*, ...))
	{
		// ゲームのループの中で、Main::Update を呼ぶ call 命令(引数なし。直後は mov rcx,[グローバル変数])。
		// 1.5.97: ID 35565 + 0x748。1.7.104: ID 36564 + 0xC3D(逆アセンブルで、1.5.97 と同じ形のものを探して決めた)。
#if defined(SKYRIM_SUPPORT_AE)
		REL::Relocation<std::uintptr_t> site{ REL::ID(36564) };
		const auto addr = site.address() + 0xC3D;
#else
		REL::Relocation<std::uintptr_t> site{ REL::ID(35565) };
		const auto addr = site.address() + 0x748;
#endif
		if (*reinterpret_cast<const std::uint8_t*>(addr) != 0xE8) {
			a_log("MAIN_THREAD_HOOK=FAIL (byte at %p is 0x%02X, expected E8 call)", reinterpret_cast<void*>(addr), *reinterpret_cast<const std::uint8_t*>(addr));
			return false;
		}
		// 直後の命令が mov rcx,[rip+x](48 8B 0D)であることも確かめる(別の呼び出しを、取り違えないため)。
		{
			const auto* next = reinterpret_cast<const std::uint8_t*>(addr + 5);
			if (!(next[0] == 0x48 && next[1] == 0x8B && next[2] == 0x0D)) {
				a_log("MAIN_THREAD_HOOK=FAIL (instruction after the call is %02X %02X %02X, expected 48 8B 0D)", next[0], next[1], next[2]);
				return false;
			}
		}
		static SKSE::Trampoline trampoline;
		trampoline.create(64);
		g_original = trampoline.write_call<5>(addr, reinterpret_cast<std::uintptr_t>(&Hook));
		a_log("MAIN_THREAD_HOOK=PASS site=%p original=%p", reinterpret_cast<void*>(addr), reinterpret_cast<void*>(g_original));
		return true;
	}

	void Post(std::function<void()> a_task)
	{
		std::lock_guard l(g_mutex);
		g_tasks.push_back(std::move(a_task));
	}
}

namespace skyshim::mainthread
{
	void PostAfterFrames(int a_frames, std::function<void()> a_task)
	{
		std::lock_guard l(g_mutex);
		g_delayed.emplace_back(a_frames, std::move(a_task));
	}
}

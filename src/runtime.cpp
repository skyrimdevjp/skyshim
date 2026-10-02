// skyshim runtime entry. Phase 2: load before Papyrus / Scaleform; Phase 3 gates start here.
#include <windows.h>
#include <shlobj.h>
#include <psapi.h>
#include <share.h>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#include "engine.h"

#pragma comment(lib, "version.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "psapi.lib")

namespace
{
	FILE* g_log = nullptr;
	HMODULE g_self = nullptr;

	void Log(const char* fmt, ...)
	{
		if (!g_log) return;
		va_list a; va_start(a, fmt);
		vfprintf(g_log, fmt, a);
		va_end(a);
		fputc('\n', g_log);
		fflush(g_log);
	}

	void OpenLog()
	{
		wchar_t docs[MAX_PATH]{};
		if (FAILED(SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, docs))) return;
		auto dir = std::filesystem::path(docs) / L"My Games" / L"Skyrim Special Edition" / L"Skyshim";
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		g_log = _wfsopen((dir / L"skyshim.log").c_str(), L"w", _SH_DENYWR);
	}

	// Crash diagnostics: log the faulting address as module+offset (fatal exception codes only).
	LONG CALLBACK CrashLogger(EXCEPTION_POINTERS* a_ep)
	{
		const DWORD code = a_ep->ExceptionRecord->ExceptionCode;
		if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_STACK_OVERFLOW &&
			code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_PRIV_INSTRUCTION) return EXCEPTION_CONTINUE_SEARCH;
		auto addr = reinterpret_cast<const void*>(a_ep->ExceptionRecord->ExceptionAddress);
		HMODULE mod = nullptr;
		char name[MAX_PATH]{};
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(addr), &mod))
			GetModuleFileNameA(mod, name, MAX_PATH);
		Log("EXCEPTION code=%08lX addr=%p module=%s +0x%llX thread=%lu", code, addr, name,
			static_cast<unsigned long long>(reinterpret_cast<const char*>(addr) - reinterpret_cast<const char*>(mod)), GetCurrentThreadId());

		// 呼び出しの経路を調べるため、スタックの上のほうから、ゲーム本体と Skyshim の中を指す値(戻り先)を、ログに出す。
		const auto  game = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
		const auto  self = reinterpret_cast<std::uintptr_t>(g_self);
		MODULEINFO  gi{}, si{};
		GetModuleInformation(GetCurrentProcess(), reinterpret_cast<HMODULE>(game), &gi, sizeof(gi));
		GetModuleInformation(GetCurrentProcess(), g_self, &si, sizeof(si));
		const auto* stack = reinterpret_cast<const std::uintptr_t*>(a_ep->ContextRecord->Rsp);
		int logged = 0;
		for (int i = 0; i < 256 && logged < 24; ++i) {
			std::uintptr_t v = 0;
			__try { v = stack[i]; } __except (EXCEPTION_EXECUTE_HANDLER) { break; }
			if (v >= game && v < game + gi.SizeOfImage) { Log("  STACK[%d] SkyrimSE.exe+0x%llX", i, static_cast<unsigned long long>(v - game)); ++logged; }
			else if (v >= self && v < self + si.SizeOfImage) { Log("  STACK[%d] skyshim.dll+0x%llX", i, static_cast<unsigned long long>(v - self)); ++logged; }
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}

	struct Ver { unsigned a{}, b{}, c{}, d{}; };

	Ver ExeVersion(std::wstring& a_exePath)
	{
		wchar_t p[MAX_PATH]{};
		GetModuleFileNameW(nullptr, p, MAX_PATH);
		a_exePath = p;
		Ver v;
		DWORD h = 0, sz = GetFileVersionInfoSizeW(p, &h);
		if (!sz) return v;
		std::string buf(sz, '\0');
		if (!GetFileVersionInfoW(p, 0, sz, buf.data())) return v;
		// The fixed-info block reports 1.0.0.0 for SkyrimSE.exe; the string table holds the real version.
		struct { WORD lang, cp; }* tr = nullptr; UINT len = 0;
		if (VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&tr), &len) && tr && len >= 4) {
			wchar_t q[64];
			swprintf_s(q, L"\\StringFileInfo\\%04x%04x\\FileVersion", tr->lang, tr->cp);
			wchar_t* s = nullptr;
			if (VerQueryValueW(buf.data(), q, reinterpret_cast<void**>(&s), &len) && s) {
				swscanf_s(s, L"%u.%u.%u.%u", &v.a, &v.b, &v.c, &v.d);
			}
		}
		return v;
	}

	// Address Library naming: 1.5.x -> version-*.bin, 1.6+ -> versionlib-*.bin
	bool CheckAddressLibrary(const Ver& v, const std::filesystem::path& gameDir)
	{
		char name[64];
		sprintf_s(name, (v.b <= 5 ? "version-%u-%u-%u-%u.bin" : "versionlib-%u-%u-%u-%u.bin"), v.a, v.b, v.c, v.d);
		auto p = gameDir / "Data" / "SKSE" / "Plugins" / name;
		bool ok = std::filesystem::exists(p);
		// Address Library often lives in a mod manager folder: SKYSHIM_ADDRLIB_DIR overrides.
		wchar_t ov[MAX_PATH]{};
		if (!ok && GetEnvironmentVariableW(L"SKYSHIM_ADDRLIB_DIR", ov, MAX_PATH)) {
			p = std::filesystem::path(ov) / name;
			ok = std::filesystem::exists(p);
		}
		Log("ADDRESS_DB_FILE=%s %s", ok ? "PASS" : "FAIL", p.string().c_str());
		return ok;
	}

	// Runs on the game's main thread after the exe is unpacked, before game init.
	void RuntimeInit()
	{
		std::wstring exe;
		Ver v = ExeVersion(exe);
		Log("skyshim runtime 0.1.0 (independent runtime; not SKSE)");
		Log("RUNTIME_VERSION=%u.%u.%u.%u", v.a, v.b, v.c, v.d);
		Log("RUNTIME_LOADED_BEFORE_PAPYRUS=PASS (loaded before game main; Papyrus VM not created yet)");
		Log("RUNTIME_LOADED_BEFORE_SCALEFORM_MENU=PASS (same)");
		const bool supported = (v.a == 1 && v.b == 5 && v.c == 97) || (v.a == 1 && v.b == 7 && v.c == 104);
		if (!supported) { Log("STOP: UNSUPPORTED_RUNTIME"); return; }
		const auto gameDir = std::filesystem::path(exe).parent_path();
		if (!CheckAddressLibrary(v, gameDir)) return;
		if (!std::filesystem::exists(gameDir / "Data" / "SKSE" / "Plugins" / (v.b <= 5 ? "version-1-5-97-0.bin" : "versionlib-1-7-104-0.bin"))) { Log("STOP: Address Library must be in <game>/Data/SKSE/Plugins (CommonLib path)"); return; }
#if defined(SKYRIM_SUPPORT_AE)
		if (!(v.b >= 6)) { Log("STOP: build flavour AE does not match runtime"); return; }
#else
		if (v.b != 5) { Log("STOP: build flavour SE(1.5.x) does not match runtime"); return; }
#endif
		// CommonLib reads Data/SKSE/Plugins/<db> relative to the current directory.
		SetCurrentDirectoryW(gameDir.c_str());
		if (!skyshim::engine::Phase3Init(&Log)) return;
		// Singletons are created later by the game; poll on a worker thread (main thread must not block).
		std::thread([] { skyshim::engine::WaitForSingletons(&Log); }).detach();
	}

	using GetCmdLine_t = char* (*)();
	GetCmdLine_t g_orig = nullptr;

	char* Hook_GetNarrowWinMainCommandLine()
	{
		static bool once = false;
		if (!once) { once = true; RuntimeInit(); }
		return g_orig();
	}

	// リモートデスクトップの中では、ゲームは「リモートセッション」と判定して、すぐ終了する(終了コード 0)。
	// ゲームが GetSystemMetrics(SM_REMOTESESSION) で調べているため、その問い合わせにだけ 0(ローカル)を返す。
	using GetSystemMetrics_t = int(WINAPI*)(int);
	GetSystemMetrics_t g_origGetSystemMetrics = nullptr;

	int WINAPI Hook_GetSystemMetrics(int a_index)
	{
		if (a_index == SM_REMOTESESSION) return 0;
		return g_origGetSystemMetrics(a_index);
	}

	// 実行ファイルの取り込み表の中の、指定した関数を差し替える。差し替え前の関数を返す(無ければ nullptr)。
	void* PatchIAT(const char* a_dll, const char* a_func, void* a_new)
	{
		auto base = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
		auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
		auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (!dir.VirtualAddress) return nullptr;
		for (auto d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
			if (_stricmp(reinterpret_cast<char*>(base + d->Name), a_dll) != 0) continue;
			auto thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + d->FirstThunk);
			auto orig = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
			for (; orig->u1.AddressOfData; ++orig, ++thunk) {
				if (IMAGE_SNAP_BY_ORDINAL64(orig->u1.Ordinal)) continue;
				auto ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + orig->u1.AddressOfData);
				if (strcmp(ibn->Name, a_func) != 0) continue;
				DWORD old;
				if (!VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) return nullptr;
				auto prev = reinterpret_cast<void*>(thunk->u1.Function);
				thunk->u1.Function = reinterpret_cast<ULONGLONG>(a_new);
				VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
				return prev;
			}
		}
		return nullptr;
	}

	bool HookIAT()
	{
		g_origGetSystemMetrics = static_cast<GetSystemMetrics_t>(PatchIAT("USER32.dll", "GetSystemMetrics", reinterpret_cast<void*>(&Hook_GetSystemMetrics)));
		Log("REMOTE_SESSION_HIDE=%s", g_origGetSystemMetrics ? "PASS (GetSystemMetrics hooked)" : "SKIPPED (not imported)");
		auto base = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
		auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
		auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (!dir.VirtualAddress) return false;
		for (auto d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
			if (_stricmp(reinterpret_cast<char*>(base + d->Name), "api-ms-win-crt-runtime-l1-1-0.dll") != 0) continue;
			auto thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + d->FirstThunk);
			auto orig = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
			for (; orig->u1.AddressOfData; ++orig, ++thunk) {
				if (IMAGE_SNAP_BY_ORDINAL64(orig->u1.Ordinal)) continue;
				auto ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + orig->u1.AddressOfData);
				if (strcmp(ibn->Name, "_get_narrow_winmain_command_line") != 0) continue;
				DWORD old;
				if (!VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) return false;
				g_orig = reinterpret_cast<GetCmdLine_t>(thunk->u1.Function);
				thunk->u1.Function = reinterpret_cast<ULONGLONG>(&Hook_GetNarrowWinMainCommandLine);
				VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
				return true;
			}
		}
		return false;
	}
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		g_self = h;
		DisableThreadLibraryCalls(h);
		OpenLog();
		AddVectoredExceptionHandler(1, CrashLogger);
		Log("DllMain attach; IAT hook %s", HookIAT() ? "installed" : "FAILED");
	}
	return TRUE;
}

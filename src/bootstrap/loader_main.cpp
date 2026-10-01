// skyui_compat_loader: start SkyrimSE.exe suspended, inject skyui_compat.dll, resume.
// usage: skyui_compat_loader.exe [<game dir>]   (default: current directory)
#include <windows.h>
#include <cstdio>
#include <string>

static bool Inject(HANDLE a_proc, const std::wstring& a_dll)
{
	const SIZE_T bytes = (a_dll.size() + 1) * sizeof(wchar_t);
	void* remote = VirtualAllocEx(a_proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!remote || !WriteProcessMemory(a_proc, remote, a_dll.c_str(), bytes, nullptr)) return false;

	auto load = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
	HANDLE th = CreateRemoteThread(a_proc, nullptr, 0, load, remote, 0, nullptr);
	if (!th) return false;
	WaitForSingleObject(th, 10000);
	DWORD code = 0;
	GetExitCodeThread(th, &code);  // truncated HMODULE; 0 => LoadLibrary failed
	CloseHandle(th);
	VirtualFreeEx(a_proc, remote, 0, MEM_RELEASE);
	return code != 0;
}

int wmain(int argc, wchar_t** argv)
{
	wchar_t self[MAX_PATH]{};
	GetModuleFileNameW(nullptr, self, MAX_PATH);
	std::wstring dllPath = self;
	dllPath = dllPath.substr(0, dllPath.find_last_of(L"\\/") + 1) + L"skyui_compat.dll";

	std::wstring gameDir = argc > 1 ? argv[1] : L".";
	wchar_t full[MAX_PATH]{};
	GetFullPathNameW(gameDir.c_str(), MAX_PATH, full, nullptr);
	gameDir = full;
	const std::wstring exe = gameDir + L"\\SkyrimSE.exe";

	{  // Diagnostic: what the launcher (e.g. MO2) actually passed, next to the loader exe.
		wchar_t cwd[MAX_PATH]{};
		GetCurrentDirectoryW(MAX_PATH, cwd);
		std::wstring logPath = dllPath.substr(0, dllPath.find_last_of(L"\\/") + 1) + L"loader.log";
		if (FILE* f = _wfopen(logPath.c_str(), L"w, ccs=UTF-8")) {
			fwprintf(f, L"argc=%d\narg1=%s\ncwd=%s\nexe=%s\n", argc, argc > 1 ? argv[1] : L"(none)", cwd, exe.c_str());
			fclose(f);
		}
	}

	if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) { wprintf(L"missing %s\n", dllPath.c_str()); return 2; }

	// Steam requires the app id when not launched by Steam.
	if (GetFileAttributesW((gameDir + L"\\steam_appid.txt").c_str()) == INVALID_FILE_ATTRIBUTES)
		SetEnvironmentVariableW(L"SteamAppId", L"489830");

	STARTUPINFOW si{ sizeof(si) };
	PROCESS_INFORMATION pi{};
	std::wstring cmd = L"\"" + exe + L"\"";
	if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, gameDir.c_str(), &si, &pi)) {
		wprintf(L"CreateProcess failed: %lu\n", GetLastError());
		return 3;
	}
	if (!Inject(pi.hProcess, dllPath)) {
		wprintf(L"inject failed\n");
		TerminateProcess(pi.hProcess, 1);
		return 4;
	}
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	// Stay alive until the game exits: a mod manager (MO2) tears down its VFS when the launched process ends.
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	return 0;
}

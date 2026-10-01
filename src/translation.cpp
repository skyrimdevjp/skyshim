// Mod translation loading. The vanilla engine only reads Interface\Translations\<plugin>_<language>.txt for the
// official masters; SkyUI's strings ($MOD CONFIGURATION ...) need the per-plugin files loaded into the GFx translator.
// Behaviour: for every enabled plugin in plugins.txt, read <plugin>_<sLanguage>.txt (UTF-16LE, BOM) and add "$key<TAB>text".
#include "engine.h"

#include "RE/Skyrim.h"

#include <ShlObj.h>

#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace skyui_compat::engine
{
	namespace
	{
		std::wstring Language()
		{
			if (auto* ini = RE::INISettingCollection::GetSingleton())
				if (auto* s = ini->GetSetting("sLanguage:General"); s && s->GetType() == RE::Setting::Type::kString && s->GetString()) {
					const std::string a = s->GetString();
					return std::wstring(a.begin(), a.end());
				}
			return L"ENGLISH";
		}

		void (*g_log)(const char*, ...) = nullptr;
		int g_failed = 0;

		// Returns entries added.
		int ParseFile(RE::BSScaleformTranslator& a_tr, const std::filesystem::path& a_path)
		{
			std::ifstream f(a_path, std::ios::binary);
			if (!f) return 0;
			std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
			if (raw.size() < 2 || static_cast<unsigned char>(raw[0]) != 0xFF || static_cast<unsigned char>(raw[1]) != 0xFE) return 0;  // must be UTF-16LE + BOM
			const std::wstring text(reinterpret_cast<const wchar_t*>(raw.data() + 2), (raw.size() - 2) / 2);

			int added = 0;
			std::size_t pos = 0;
			while (pos < text.size()) {
				auto end = text.find(L'\n', pos);
				if (end == std::wstring::npos) end = text.size();
				std::wstring line = text.substr(pos, end - pos);
				pos = end + 1;
				if (!line.empty() && line.back() == L'\r') line.pop_back();
				if (line.size() < 4 || line[0] != L'$') continue;
				const auto tab = line.rfind(L'\t');
				if (tab == std::wstring::npos || tab < 2) continue;
				// The GFx lookup matches the exact spelling ("$MAGIC" from vanilla UI, "$Magic" from SkyUI), like SKSE, which builds
				// keys with GetCachedString (a case-preserving cache) instead of the case-folding BSFixedStringW pool.
				auto& map = a_tr.translator.translationMap;
				const std::wstring key = line.substr(0, tab);
				const std::wstring value = line.substr(tab + 1);
				auto cached = [](const std::wstring& a_text) {
					std::wstring buf = a_text;  // GetCachedString takes a writable buffer
					wchar_t*     out = nullptr;
					RE::BSScaleformTranslator::GetCachedString(&out, buf.data(), 0);
					// Wrap the pointer without going through the pool constructor. A copy takes its own reference; the wrapper
					// itself is deliberately never destroyed (worst case: one leaked reference).
					auto* wrapper = static_cast<RE::BSFixedStringW*>(::operator new(sizeof(RE::BSFixedStringW)));
					static_assert(sizeof(RE::BSFixedStringW) == sizeof(wchar_t*));
					std::memcpy(static_cast<void*>(wrapper), &out, sizeof(out));
					return RE::BSFixedStringW(*wrapper);
				};
				const auto result = map.insert({ cached(key), cached(value) });
				if (result.second) {
					++added;
				} else {
					// The cache folded the spelling into an existing entry: take the mod's value for it.
					if (g_log && g_failed++ < 40) g_log("TRANSLATION_KEY_COLLAPSED (same entry) key=%zu chars", key.size());
					result.first->second = cached(value);
					++added;
				}
			}
			return added;
		}
	}

	bool ImportModTranslations(LogFn a_log)
	{
		auto* mgr = RE::BSScaleformManager::GetSingleton();
		if (!mgr || !mgr->loader) return false;
		auto tr = mgr->loader->GetState<RE::BSScaleformTranslator>(RE::GFxState::StateType::kTranslator);
		if (!tr) return false;

		PWSTR local = nullptr;
		if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return false;
		const auto plugins = std::filesystem::path(local) / L"Skyrim Special Edition" / L"plugins.txt";
		CoTaskMemFree(local);

		std::ifstream f(plugins);
		if (!f) { a_log("TRANSLATIONS plugins.txt not found: %s", plugins.string().c_str()); return true; }
		g_log = a_log;
		const auto lang = Language();
		int files = 0, entries = 0, lines = 0, enabled = 0;
		a_log("TRANSLATIONS plugins.txt=%s lang=%ls", plugins.string().c_str(), lang.c_str());
		std::string line;
		while (std::getline(f, line)) {
			if (!line.empty() && line.back() == '\r') line.pop_back();
			++lines;
			if (line.empty() || line[0] != '*') continue;  // only enabled plugins
			++enabled;
			line.erase(0, 1);
			const auto dot = line.rfind('.');
			if (dot == std::string::npos) continue;
			const std::wstring name(line.begin(), line.begin() + dot);
			const auto p = std::filesystem::path(L"Data") / L"Interface" / L"Translations" / (name + L"_" + lang + L".txt");
			const int n = ParseFile(*tr, p);
			a_log("TRANSLATIONS try %s -> %d entries", p.string().c_str(), n);
			if (n) { ++files; entries += n; }
		}
		a_log("TRANSLATIONS_IMPORTED=PASS files=%d entries=%d (plugins.txt lines=%d enabled=%d)", files, entries, lines, enabled);
		return true;
	}
}

namespace skyui_compat::engine
{
	// Diagnostic: what the game's translation table holds for a few keys (call on the main thread, some time after import).
	void DumpTranslations(LogFn a_log)
	{
		auto* mgr = RE::BSScaleformManager::GetSingleton();
		if (!mgr || !mgr->loader) { a_log("TRANSLATION_DUMP no loader"); return; }
		auto tr = mgr->loader->GetState<RE::BSScaleformTranslator>(RE::GFxState::StateType::kTranslator);
		if (!tr) { a_log("TRANSLATION_DUMP no translator"); return; }
		auto& map = tr->translator.translationMap;
		a_log("TRANSLATION_DUMP size=%u", static_cast<unsigned>(map.size()));
		auto u8 = [](const wchar_t* a_w) { const auto s = std::filesystem::path(a_w ? a_w : L"").u8string(); return std::string(s.begin(), s.end()); };
		// Exact spellings actually stored: all keys equal to these words when compared case-insensitively.
		for (const auto& kv : map) {
			const std::wstring k = kv.first.c_str() ? kv.first.c_str() : L"";
			std::wstring lower = k;
			for (auto& c : lower) c = static_cast<wchar_t>(std::towlower(c));
			if (lower == L"$general" || lower == L"$armor" || lower == L"$map" || lower == L"$magic" || lower == L"$leather")
				a_log("TRANSLATION_DUMP_ENTRY key=%s value=%s keyptr=%p", u8(k.c_str()).c_str(), u8(kv.second.c_str()).c_str(), static_cast<const void*>(kv.first.c_str()));
		}
		for (const wchar_t* key : { L"$General",L"$Advanced", L"$Controls", L"$MOD CONFIGURATION", L"$Favorite Groups", L"$Armor" }) {
			auto it = map.find(RE::BSFixedStringW(key));
			a_log("TRANSLATION_DUMP %s = %s", u8(key).c_str(), it == map.end() ? "<missing>" : u8(it->second.c_str()).c_str());
		}
	}
}

#include "inject.h"

#include "../events/events.h"
#include "../mainthread.h"
#include "../events/mod_event.h"

#include "RE/Skyrim.h"

#include <array>
#include <functional>
#include <string>
#include <utility>

namespace skyshim::scaleform
{
	namespace
	{
		void (*g_log)(const char*, ...) = nullptr;
		IndexStore g_indices;

		// SkyUI reads skse.version.releaseIdx (>= 70 enables its extended features); values match SKSE.GetVersion*.
		constexpr double kMajor = 2, kMinor = 2, kBeta = 0, kReleaseIdx = 75;

		class FnHandler : public RE::GFxFunctionHandler
		{
		public:
			using Fn = std::function<void(Params&)>;
			explicit FnHandler(Fn a_fn) : _fn(std::move(a_fn)) {}
			void Call(Params& a_params) override { _fn(a_params); }

		private:
			Fn _fn;
		};

		std::string Str(const RE::GFxValue& a_v) { return a_v.IsString() && a_v.GetString() ? std::string(a_v.GetString()) : std::string(); }

		void AddFn(RE::GFxMovieView* a_view, RE::GFxValue& a_obj, const char* a_name, FnHandler::Fn a_fn)
		{
			RE::GFxValue fn;
			a_view->CreateFunction(&fn, new FnHandler(std::move(a_fn)));
			a_obj.SetMember(a_name, fn);
		}

		// No-op for API members SkyUI calls that are not implemented yet; logs the first call so gaps are visible.
		void AddStub(RE::GFxMovieView* a_view, RE::GFxValue& a_obj, const char* a_name)
		{
			AddFn(a_view, a_obj, a_name, [a_name](FnHandler::Params&) {
				static std::string seen;
				if (seen.find(a_name) == std::string::npos) { seen += a_name; if (g_log) g_log("SKSE_JS_STUB_CALLED %s", a_name); }
			});
		}

		void Inject(RE::GFxMovieView* a_view)
		{
			RE::GFxValue globals;
			if (!a_view->GetVariable(&globals, "_global")) return;

			RE::GFxValue skse;
			a_view->CreateObject(&skse);

			RE::GFxValue version;
			a_view->CreateObject(&version);
			version.SetMember("major", RE::GFxValue(kMajor));
			version.SetMember("minor", RE::GFxValue(kMinor));
			version.SetMember("beta", RE::GFxValue(kBeta));
			version.SetMember("releaseIdx", RE::GFxValue(kReleaseIdx));
			skse.SetMember("version", version);

			RE::GFxValue plugins;  // empty: skse.plugins.<Name> must read as undefined
			a_view->CreateObject(&plugins);
			skse.SetMember("plugins", plugins);

			AddFn(a_view, skse, "Log", [](FnHandler::Params& p) { if (g_log && p.argCount > 0) g_log("SKSE_JS %s", Str(p.args[0]).c_str()); });

			AddFn(a_view, skse, "SendModEvent", [](FnHandler::Params& p) {
				if (p.argCount < 1) return;
				events::SendModEvent(nullptr, Str(p.args[0]), p.argCount > 1 ? Str(p.args[1]) : std::string(), p.argCount > 2 && p.args[2].IsNumber() ? static_cast<float>(p.args[2].GetNumber()) : 0.0f);
			});

			AddFn(a_view, skse, "AllowTextInput", [](FnHandler::Params& p) {
				if (p.argCount < 1) return;
				if (auto* map = RE::ControlMap::GetSingleton()) map->AllowTextInput(p.args[0].GetBool());
			});

			AddFn(a_view, skse, "OpenMenu", [](FnHandler::Params& p) {
				if (p.argCount < 1) return;
				if (auto* q = RE::UIMessageQueue::GetSingleton()) q->AddMessage(RE::BSFixedString(Str(p.args[0]).c_str()), RE::UI_MESSAGE_TYPE::kShow, nullptr);
			});

			// skse.StoreIndices(key, array) / skse.LoadIndices(key, array): array is filled in place.
			AddFn(a_view, skse, "StoreIndices", [](FnHandler::Params& p) {
				if (p.argCount < 2 || !p.args[1].IsArray()) return;
				std::vector<std::int32_t> v;
				for (std::uint32_t i = 0, n = p.args[1].GetArraySize(); i < n; ++i) {
					RE::GFxValue e;
					if (p.args[1].GetElement(i, &e) && e.IsNumber()) v.push_back(static_cast<std::int32_t>(e.GetNumber()));
				}
				g_indices.Store(Str(p.args[0]), std::move(v));
			});
			AddFn(a_view, skse, "LoadIndices", [](FnHandler::Params& p) {
				if (p.argCount < 2 || !p.args[1].IsArray()) return;
				p.args[1].ClearElements();
				for (auto i : g_indices.Load(Str(p.args[0]))) p.args[1].PushBack(RE::GFxValue(static_cast<double>(i)));
			});

			// skse.StartRemapMode(target): the next key press is passed to target.EndRemapMode(keyCode).
			AddFn(a_view, skse, "StartRemapMode", [](FnHandler::Params& p) {
				if (p.argCount > 0 && p.args[0].IsObject()) events::StartRemap(p.args[0]);
			});
			// skse.GetLastKeycode(isKeyDown) / GetLastControl(isKeyDown): 最後に押した(真)、または離した(偽)キーの番号と操作名。
			AddFn(a_view, skse, "GetLastKeycode", [](FnHandler::Params& p) {
				const auto key = events::LastKeycode(p.argCount > 0 && p.args[0].IsBool() && p.args[0].GetBool());
				if (p.retVal) p.retVal->SetNumber(static_cast<double>(key));
			});
			AddFn(a_view, skse, "GetLastControl", [](FnHandler::Params& p) {
				const auto control = events::LastControl(p.argCount > 0 && p.args[0].IsBool() && p.args[0].GetBool());
				if (p.retVal) p.retVal->SetString(control.c_str());
			});
			// skse.GetMappedKey(control, deviceType, contextIdx): 操作名に割り当てられたキーの番号(割り当てが無ければ -1)。
			AddFn(a_view, skse, "GetMappedKey", [](FnHandler::Params& p) {
				if (p.argCount < 3 || !p.retVal) return;
				const auto key = events::MappedKey(Str(p.args[0]), static_cast<std::int32_t>(p.args[1].GetNumber()), static_cast<std::int32_t>(p.args[2].GetNumber()));
				p.retVal->SetNumber(static_cast<double>(key));
			});
			// skse.EnableMapMenuMouseWheel(enable): SKSE は、マップのマウスホイールを有効にする。
			// Skyshim では、ホイールのズームが、実装なしでも動くため、何もしない。
			AddFn(a_view, skse, "EnableMapMenuMouseWheel", [](FnHandler::Params&) {});

			// skse.RequestActivePlayerEffects(array): fills the array with the player's visible active effects.
			// Fields are the ones SkyUI's active effects widget reads: id, duration, elapsed (+ archetype, actorValue, effectFlags, resistType for the icon).
			AddFn(a_view, skse, "RequestActivePlayerEffects", [](FnHandler::Params& p) {
				if (p.argCount < 1 || !p.args[0].IsArray() || !p.movie) return;
				auto* player = RE::PlayerCharacter::GetSingleton();
				auto* list = player ? player->GetActiveEffectList() : nullptr;
				if (!list) return;
				for (auto* ae : *list) {
					if (!ae || !ae->effect || !ae->effect->baseEffect) continue;
					if (ae->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) continue;
					const auto* mgef = ae->effect->baseEffect;
					if (mgef->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) continue;
					RE::GFxValue obj;
					p.movie->CreateObject(&obj);
					obj.SetMember("id", RE::GFxValue(static_cast<double>(ae->usUniqueID)));
					obj.SetMember("duration", RE::GFxValue(static_cast<double>(ae->duration)));
					obj.SetMember("elapsed", RE::GFxValue(static_cast<double>(ae->elapsedSeconds)));
					obj.SetMember("magnitude", RE::GFxValue(static_cast<double>(ae->magnitude)));
					obj.SetMember("effectFlags", RE::GFxValue(static_cast<double>(mgef->data.flags.underlying())));
					obj.SetMember("archetype", RE::GFxValue(static_cast<double>(static_cast<std::int32_t>(mgef->data.archetype))));
					obj.SetMember("actorValue", RE::GFxValue(static_cast<double>(static_cast<std::int32_t>(mgef->data.primaryAV))));
					obj.SetMember("resistType", RE::GFxValue(static_cast<double>(static_cast<std::int32_t>(mgef->data.resistVariable))));
					p.args[0].PushBack(obj);
				}
			});

			for (const char* name : { "ShowOnMap",
					 "ForceContainerCategorization", "ExtendData", "ExtendAlchemyCategories", "ExtendForm" })
				AddStub(a_view, skse, name);

			globals.SetMember("skse", skse);
		}


		// Diagnostic: reproduces skyui.util.Translator.translate (hidden TextField: set .text = "$key", read .text back).
		void TranslateProbe(RE::GPtr<RE::GFxMovieView> a_view)
		{
			if (!a_view || !g_log) return;
			RE::GFxValue args[6] = { RE::GFxValue("__probe"), RE::GFxValue(9999.0), RE::GFxValue(0.0), RE::GFxValue(0.0), RE::GFxValue(1.0), RE::GFxValue(1.0) };
			const bool created = a_view->Invoke("_root.createTextField", nullptr, args, 6);
			g_log("TRANSLATE_PROBE createTextField=%d", created);
			for (const char* key : { "$General", "$Controls", "$Advanced", "$MOD CONFIGURATION", "$Armor", "$Map", "$Leather", "$Magic", "$Off", "$Group", "$Favorite Groups", "$ALL", "$FILTER", "$general", "$GENERAL" }) {
				a_view->SetVariable("_root.__probe.text", RE::GFxValue(key));
				RE::GFxValue out;
				const bool ok = a_view->GetVariable(&out, "_root.__probe.text");
				g_log("TRANSLATE_PROBE %s -> %s (get=%d)", key, ok && out.IsString() && out.GetString() ? out.GetString() : "<none>", ok);
			}
		}
		void InjectMenu(RE::IMenu* a_menu)
		{
			if (a_menu && a_menu->uiMovie) Inject(a_menu->uiMovie.get());
		}

		constexpr std::size_t kMaxMenus = 96;
		using Create_t = RE::UI::Create_t*;
		std::array<Create_t, kMaxMenus> g_original{};

		std::array<std::string, kMaxMenus> g_names{};

		template <std::size_t I>
		RE::IMenu* Wrapper()
		{
			auto* menu = g_original[I]();
			InjectMenu(menu);
			// Debug aid (off): to probe translations, run TranslateProbe on the Journal Menu a few frames after creation:
			//   if (menu && menu->uiMovie && g_names[I] == "Journal Menu") mainthread::PostAfterFrames(90, [v = menu->uiMovie] { TranslateProbe(v); });
			return menu;
		}

		template <std::size_t... I>
		constexpr std::array<Create_t, kMaxMenus> MakeWrappers(std::index_sequence<I...>) { return { &Wrapper<I>... }; }
		const std::array<Create_t, kMaxMenus> g_wrappers = MakeWrappers(std::make_index_sequence<kMaxMenus>{});
	}

	bool InstallMenuWrappers(void (*a_log)(const char*, ...))
	{
		g_log = a_log;
		auto* ui = RE::UI::GetSingleton();
		if (!ui) return false;
		std::size_t n = 0;
		for (auto& entry : ui->menuMap) {
			if (n >= kMaxMenus) { a_log("SKSE_JS_INJECT=WARN more than %zu menus", kMaxMenus); break; }
			if (!entry.second.create) continue;
			g_original[n] = entry.second.create;
			g_names[n] = entry.first.c_str() ? entry.first.c_str() : "";
			entry.second.create = g_wrappers[n];
			++n;
		}
		a_log("SKSE_JS_INJECT=PASS wrapped %zu menu creators", n);
		return n > 0;
	}
}

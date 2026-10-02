// 拡張データ(SKSE の skse.ExtendData に相当)。
//
// SkyUI の一覧(インベントリ、コンテナ、売買、贈り物、お気に入り)は、各アイテムの ActionScript のオブジェクトに、
// SKSE が足していた項目(formId、formType、keywords、武器や防具の細かい情報など)を読む。
// とくに、お気に入りのグループへの追加は、選んだアイテムの formId を使う。
//
// 方法: 各メニューの ProcessMessage(仮想関数)に割り込み、元の処理(一覧の作成)が終わったあとで、
// 一覧の各アイテムに項目を足し、SkyUI の処理済みの印(skyui_itemDataProcessed)を戻して、一覧を再処理させる。
// ゲーム本体の関数の先頭を書き換える方法は使わない(仮想関数の表の書き換えだけ)。
#include "extend_data.h"

#include "RE/Skyrim.h"

#include <array>
#include <cstring>
#include <string>
#include <unordered_map>

namespace skyshim::scaleform
{
	namespace
	{
		void (*g_log)(const char*, ...) = nullptr;

		void SetNumber(RE::GFxValue& a_obj, const char* a_name, double a_value) { a_obj.SetMember(a_name, RE::GFxValue(a_value)); }

		// キーワードを、「編集用の名前 = 真」の形のオブジェクトにして渡す(SKSE と同じ形)。
		void SetKeywords(RE::GFxMovieView* a_view, RE::GFxValue& a_obj, RE::TESForm* a_form)
		{
			RE::GFxValue keywords;
			a_view->CreateObject(&keywords);
			if (auto* kf = a_form->As<RE::BGSKeywordForm>()) {
				kf->ForEachKeyword([&](RE::BGSKeyword* a_keyword) {
					if (a_keyword) {
						const char* name = a_keyword->GetFormEditorID();
						if (name && *name) keywords.SetMember(name, RE::GFxValue(true));
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}
			a_obj.SetMember("keywords", keywords);
		}

		// 効果の設定(流派、必要な技能、種類など)を、項目に足す。
		void AddEffectSetting(RE::GFxValue& a_obj, const RE::EffectSetting* a_mgef)
		{
			if (!a_mgef) return;
			const auto& d = a_mgef->data;
			const auto school = static_cast<double>(static_cast<std::int32_t>(d.associatedSkill));
			SetNumber(a_obj, "subType", school);  // 旧名(SkyUI が school へ読み替える)
			SetNumber(a_obj, "school", school);
			SetNumber(a_obj, "skillLevel", static_cast<double>(d.minimumSkill));
			SetNumber(a_obj, "effectFlags", static_cast<double>(d.flags.underlying()));
			SetNumber(a_obj, "archetype", static_cast<double>(static_cast<std::int32_t>(d.archetype)));
			SetNumber(a_obj, "deliveryType", static_cast<double>(static_cast<std::int32_t>(d.delivery)));
			SetNumber(a_obj, "actorValue", static_cast<double>(static_cast<std::int32_t>(d.primaryAV)));
			SetNumber(a_obj, "castType", static_cast<double>(static_cast<std::int32_t>(d.castingType)));
			SetNumber(a_obj, "magicType", static_cast<double>(static_cast<std::int32_t>(d.resistVariable)));  // 旧名(SkyUI が resistance へ読み替える)
		}

		// 魔法に関するフォーム(呪文、巻物、ポーション、材料、効果)の項目を足す。SKSE の MagicItemData に相当する。
		void AddMagicFields(RE::GFxValue& a_obj, RE::TESForm* a_form)
		{
			if (auto* mgef = a_form->As<RE::EffectSetting>()) {
				AddEffectSetting(a_obj, mgef);
			} else if (auto* magic = a_form->As<RE::MagicItem>()) {
				// 最も影響の大きい効果から、効果量などを取る。
				if (const auto* effect = magic->GetCostliestEffectItem(static_cast<RE::MagicSystem::Delivery>(5), false)) {
					SetNumber(a_obj, "magnitude", effect->effectItem.magnitude);
					SetNumber(a_obj, "duration", static_cast<double>(effect->effectItem.duration));
					SetNumber(a_obj, "area", static_cast<double>(effect->effectItem.area));
					AddEffectSetting(a_obj, effect->baseEffect);
				}
				if (auto* spell = a_form->As<RE::SpellItem>()) {
					SetNumber(a_obj, "spellType", static_cast<double>(static_cast<std::int32_t>(spell->GetSpellType())));
					if (auto* slot = spell->GetEquipSlot()) SetNumber(a_obj, "equipSlot", static_cast<double>(slot->GetFormID()));
				}
			}
		}

		// アイテム 1 つ分の項目を足す。
		void ExtendEntry(RE::GFxMovieView* a_view, RE::GFxValue& a_obj, RE::TESForm* a_form, RE::InventoryEntryData* a_entry)
		{
			if (!a_form || !a_obj.IsObject()) return;

			SetNumber(a_obj, "formType", static_cast<double>(a_form->GetFormType()));
			SetNumber(a_obj, "formId", static_cast<double>(a_form->GetFormID()));
			// アイテムごとの固有 ID(SKSE の ExtraUniqueID)は、まだ扱わない。常に 0(同じアイテムのどれでもよい)。
			SetNumber(a_obj, "itemId", 0);
			SetKeywords(a_view, a_obj, a_form);

			if (auto* armor = a_form->As<RE::TESObjectARMO>()) {
				SetNumber(a_obj, "partMask", static_cast<double>(armor->GetSlotMask()));
				SetNumber(a_obj, "weightClass", static_cast<double>(armor->GetArmorType()));
			} else if (auto* weapon = a_form->As<RE::TESObjectWEAP>()) {
				const auto type = static_cast<double>(weapon->GetWeaponType());
				SetNumber(a_obj, "subType", type);
				SetNumber(a_obj, "weaponType", type);
				SetNumber(a_obj, "speed", weapon->GetSpeed());
				SetNumber(a_obj, "reach", weapon->GetReach());
				SetNumber(a_obj, "stagger", weapon->GetStagger());
				SetNumber(a_obj, "minRange", weapon->GetMinRange());
				SetNumber(a_obj, "maxRange", weapon->GetMaxRange());
				SetNumber(a_obj, "critDamage", static_cast<double>(weapon->criticalData.damage));
				SetNumber(a_obj, "baseDamage", static_cast<double>(weapon->attackDamage));
				if (auto* slot = weapon->GetEquipSlot()) SetNumber(a_obj, "equipSlot", static_cast<double>(slot->GetFormID()));
			} else if (auto* ammo = a_form->As<RE::TESAmmo>()) {
				SetNumber(a_obj, "flags", static_cast<double>(ammo->data.flags.underlying()));
			} else if (auto* gem = a_form->As<RE::TESSoulGem>()) {
				SetNumber(a_obj, "gemSize", static_cast<double>(*gem->soulCapacity));
				SetNumber(a_obj, "soulSize", static_cast<double>(a_entry ? a_entry->GetSoulLevel() : *gem->currentSoul));
			} else if (auto* potion = a_form->As<RE::AlchemyItem>()) {
				SetNumber(a_obj, "flags", static_cast<double>(potion->data.flags.underlying()));
				// 使用音(SkyUI は、飲み物と食べ物の区別に使う)。
				if (potion->data.consumptionSound) {
					RE::GFxValue sound;
					a_view->CreateObject(&sound);
					SetNumber(sound, "formType", static_cast<double>(potion->data.consumptionSound->GetFormType()));
					SetNumber(sound, "formId", static_cast<double>(potion->data.consumptionSound->GetFormID()));
					a_obj.SetMember("useSound", sound);
				}
			} else if (auto* book = a_form->As<RE::TESObjectBOOK>()) {
				SetNumber(a_obj, "flags", static_cast<double>(book->data.flags.underlying()));
				SetNumber(a_obj, "bookType", static_cast<double>(book->data.type.underlying()));
			}

			// ポーション、巻物、材料は、効果量や持続時間などの項目も足す。
			AddMagicFields(a_obj, a_form);
		}

		bool Has(RE::GFxValue& a_obj, const char* a_name)
		{
			RE::GFxValue v;
			return a_obj.IsObject() && a_obj.GetMember(a_name, &v) && !v.IsUndefined();
		}

		// 一覧(ItemList)の全アイテムに項目を足して、SkyUI に再処理させる。
		void ExtendItemList(RE::ItemList* a_list)
		{
			if (!a_list || !a_list->view || a_list->items.empty()) return;
			bool changed = false;
			for (auto* item : a_list->items) {
				if (!item || Has(item->obj, "formId")) continue;
				auto* form = item->data.objDesc ? item->data.objDesc->object : nullptr;

				ExtendEntry(a_list->view.get(), item->obj, form, item->data.objDesc);
				item->obj.SetMember("skyui_itemDataProcessed", RE::GFxValue(false));
				changed = true;
			}
			if (changed) a_list->root.Invoke("InvalidateData");
		}


		// 魔法メニュー(呪文、シャウト、パワー、効果)の 1 項目分。SKSE の MagicItemData に相当する。
		void ExtendMagicEntry(RE::GFxMovieView* a_view, RE::GFxValue& a_obj, RE::TESForm* a_form)
		{
			if (!a_form || !a_obj.IsObject()) return;

			SetNumber(a_obj, "formType", static_cast<double>(a_form->GetFormType()));
			SetNumber(a_obj, "formId", static_cast<double>(a_form->GetFormID()));
			SetKeywords(a_view, a_obj, a_form);

			AddMagicFields(a_obj, a_form);
		}

		// 魔法メニューの一覧に項目を足して、SkyUI に再処理させる。
		void ExtendMagicList(RE::MagicItemList* a_list)
		{
			if (!a_list || !a_list->view || a_list->items.empty()) return;
			bool changed = false;
			for (auto* item : a_list->items) {
				if (!item || Has(item->obj, "formId")) continue;
				ExtendMagicEntry(a_list->view.get(), item->obj, item->data.baseForm);
				item->obj.SetMember("skyui_itemDataProcessed", RE::GFxValue(false));
				changed = true;
			}
			if (changed) a_list->root.Invoke("InvalidateData");
		}
		// お気に入りメニューの一覧(FavoritesMenu::favorites と、ActionScript の entryList は、同じ順序)。
		void ExtendFavorites(RE::FavoritesMenu* a_menu)
		{
			if (!a_menu || a_menu->favorites.empty() || !a_menu->uiMovie) return;
			RE::GFxValue list, entries;
			if (!a_menu->root.GetMember("itemList", &list) || !list.GetMember("entryList", &entries) || !entries.IsArray()) return;
			const std::uint32_t count = std::min<std::uint32_t>(entries.GetArraySize(), a_menu->favorites.size());
			bool changed = false;
			for (std::uint32_t i = 0; i < count; ++i) {
				RE::GFxValue entry;
				if (!entries.GetElement(i, &entry) || Has(entry, "formId")) continue;
				const auto& fav = a_menu->favorites[i];
				ExtendEntry(a_menu->uiMovie.get(), entry, fav.item, fav.entryData);
				entry.SetMember("skyui_itemDataProcessed", RE::GFxValue(false));
				changed = true;
			}
			if (changed) list.Invoke("InvalidateData");
		}

		// ---- クラフトメニュー(錬金、調理や鍛冶、など) ----

		// オブジェクトの先頭の仮想関数の表が、a_ids[0] と同じか。クラフトのサブメニューの種類を見分けるのに使う。
		template <std::size_t N>
		bool IsType(const void* a_object, const std::array<REL::ID, N>& a_ids)
		{
			REL::Relocation<std::uintptr_t> vtbl{ a_ids[0] };
			return *reinterpret_cast<const std::uintptr_t*>(a_object) == vtbl.address();
		}

		// 錬金の効果が、有益、有害、その他のどれか(SKSE の AlchemyCategoryArgs と同じ分け方)。
		const char* AlchemyKind(const RE::EffectSetting* a_mgef)
		{
			using A = RE::EffectSetting::Archetype;
			const bool detrimental = a_mgef->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kDetrimental);
			switch (a_mgef->data.archetype) {
			case A::kValueModifier:
			case A::kDualValueModifier:
			case A::kPeakValueModifier:
				return detrimental ? "harmful" : "beneficial";
			case A::kAbsorb:
			case A::kCureDisease:
			case A::kInvisibility:
			case A::kCureParalysis:
			case A::kCureAddiction:
			case A::kCurePoison:
			case A::kDispel:
				return "beneficial";
			case A::kFrenzy:
			case A::kCalm:
			case A::kDemoralize:
			case A::kParalysis:
				return "harmful";
			default:
				return "other";
			}
		}

		// 錬金の分類(効果の一覧)のアイコンを、有益、有害、その他に直す。
		// SKSE は、ゲームが SetCategoriesList に渡す引数の印を、書き換えていた。ここでは、渡されたあとの分類の項目を直す。
		// 分類の項目の名前は、効果の名前なので、材料の効果の名前から、効果を探す。
		void FixAlchemyCategories(RE::CraftingSubMenus::AlchemyMenu* a_menu)
		{
			RE::GFxValue lists, categories, entries;
			if (!a_menu->craftingMenu.GetMember("InventoryLists", &lists) && !(a_menu->view && a_menu->view->GetVariable(&lists, "_root.Menu.InventoryLists"))) return;
			if (!lists.GetMember("CategoriesList", &categories) || !categories.GetMember("entryList", &entries) || !entries.IsArray()) return;
			const std::uint32_t count = entries.GetArraySize();
			if (count < 2) return;

			std::unordered_map<std::string, const RE::EffectSetting*> effects;
			for (const auto& ingredient : a_menu->ingredientEntries) {
				auto* item = ingredient.ingredient && ingredient.ingredient->object ? ingredient.ingredient->object->As<RE::IngredientItem>() : nullptr;
				if (!item) continue;
				for (auto* effect : item->effects) {
					if (effect && effect->baseEffect && effect->baseEffect->GetFullName()) effects[effect->baseEffect->GetFullName()] = effect->baseEffect;
				}
			}

			bool changed = false;
			for (std::uint32_t i = 1; i < count; ++i) {  // 0 番目は、「材料」
				RE::GFxValue entry, text;
				if (!entries.GetElement(i, &entry) || Has(entry, "skyshim_alchemy") || !entry.GetMember("text", &text) || !text.IsString()) continue;
				if (auto it = effects.find(text.GetString()); it != effects.end()) {
					entry.SetMember("iconLabel", RE::GFxValue(AlchemyKind(it->second)));
					entry.SetMember("skyshim_alchemy", RE::GFxValue(true));
					changed = true;
				}
			}
			if (changed) categories.Invoke("InvalidateData");
		}

		// クラフトのサブメニューの一覧に項目を足す。一覧の ActionScript の項目と、ゲーム側のレシピなどの配列は、同じ順序。
		// 数が合わないときは、何もしない(誤った項目を足さないため)。
		void ExtendCrafting(RE::CraftingMenu* a_menu)
		{
			auto* sub = a_menu ? a_menu->subMenu : nullptr;
			if (!sub || !sub->view || !sub->entryList.IsArray()) return;
			const std::uint32_t asCount = sub->entryList.GetArraySize();
			if (asCount == 0) return;

			using FormAndEntry = std::pair<RE::TESForm*, RE::InventoryEntryData*>;
			auto apply = [&](std::size_t a_nativeCount, auto&& a_formAt) {
				if (a_nativeCount != asCount) {
					static bool s_logged = false;
					if (!s_logged && g_log) {
						s_logged = true;
						g_log("CRAFTING_COUNT_MISMATCH native=%zu actionscript=%u (not extended)", a_nativeCount, asCount);
					}
					return;
				}
				bool changed = false;
				for (std::uint32_t i = 0; i < asCount; ++i) {
					RE::GFxValue entry;
					if (!sub->entryList.GetElement(i, &entry) || Has(entry, "formId")) continue;
					const FormAndEntry fe = a_formAt(i);
					if (!fe.first) continue;
					ExtendEntry(sub->view.get(), entry, fe.first, fe.second);
					entry.SetMember("skyui_itemDataProcessed", RE::GFxValue(false));
					changed = true;
				}
				if (changed) sub->itemList.Invoke("InvalidateData");
			};

			if (IsType(sub, RE::VTABLE_CraftingSubMenus__ConstructibleObjectMenu)) {
				auto* m = static_cast<RE::CraftingSubMenus::ConstructibleObjectMenu*>(sub);
				apply(m->recipes.size(), [&](std::uint32_t i) {
					auto* recipe = m->recipes[i].constructibleObject;
					return FormAndEntry{ recipe ? recipe->createdItem : nullptr, nullptr };
				});
			} else if (IsType(sub, RE::VTABLE_CraftingSubMenus__SmithingMenu)) {
				auto* m = static_cast<RE::CraftingSubMenus::SmithingMenu*>(sub);
				apply(m->recipes.size(), [&](std::uint32_t i) { return FormAndEntry{ m->recipes[i].item, nullptr }; });
			} else if (IsType(sub, RE::VTABLE_CraftingSubMenus__AlchemyMenu)) {
				auto* m = static_cast<RE::CraftingSubMenus::AlchemyMenu*>(sub);
				apply(m->ingredientEntries.size(), [&](std::uint32_t i) {
					auto* ingredient = m->ingredientEntries[i].ingredient;
					return FormAndEntry{ ingredient ? ingredient->object : nullptr, ingredient };
				});
				FixAlchemyCategories(m);
			}
		}

		// メニューごとの、割り込みの関数。元の ProcessMessage を呼んだあとで、項目を足す。
		using ProcessMessage_t = RE::UI_MESSAGE_RESULTS (*)(RE::IMenu*, RE::UIMessage&);

		template <class Menu>
		struct Hook
		{
			static inline ProcessMessage_t original = nullptr;

			static RE::UI_MESSAGE_RESULTS Thunk(RE::IMenu* a_menu, RE::UIMessage& a_message)
			{
				const auto result = original(a_menu, a_message);

				// メニューが、実際に開いている間だけ、項目を足す。閉じる最中や、セーブのロードで破棄されるときは、
				// 一覧やサブメニューが解放ずみのことがあり、触るとゲームが落ちる。
				auto* ui = RE::UI::GetSingleton();
				if (!ui || !ui->IsMenuOpen(Menu::MENU_NAME)) return result;

				switch (a_message.type.get()) {
				case RE::UI_MESSAGE_TYPE::kShow:
				case RE::UI_MESSAGE_TYPE::kReshow:
				case RE::UI_MESSAGE_TYPE::kInventoryUpdate:
				case RE::UI_MESSAGE_TYPE::kUpdate:
					if constexpr (std::is_same_v<Menu, RE::FavoritesMenu>) ExtendFavorites(static_cast<RE::FavoritesMenu*>(a_menu));
					else if constexpr (std::is_same_v<Menu, RE::MagicMenu>) ExtendMagicList(static_cast<RE::MagicMenu*>(a_menu)->itemList);
					else if constexpr (std::is_same_v<Menu, RE::CraftingMenu>) ExtendCrafting(static_cast<RE::CraftingMenu*>(a_menu));
					else ExtendItemList(static_cast<Menu*>(a_menu)->itemList);
					break;
				default: break;
				}
				return result;
			}

			static void Install(const REL::ID& a_vtableId, const char* a_name)
			{
				REL::Relocation<std::uintptr_t> vtbl{ a_vtableId };
				original = reinterpret_cast<ProcessMessage_t>(vtbl.write_vfunc(0x4, reinterpret_cast<std::uintptr_t>(&Thunk)));
				if (g_log) g_log("EXTEND_DATA_HOOK=PASS %s original=%p", a_name, reinterpret_cast<void*>(original));
			}
		};
	}

	bool InstallExtendData(void (*a_log)(const char*, ...))
	{
		g_log = a_log;
		Hook<RE::InventoryMenu>::Install(RE::VTABLE_InventoryMenu[0], "InventoryMenu");
		Hook<RE::ContainerMenu>::Install(RE::VTABLE_ContainerMenu[0], "ContainerMenu");
		Hook<RE::BarterMenu>::Install(RE::VTABLE_BarterMenu[0], "BarterMenu");
		Hook<RE::GiftMenu>::Install(RE::VTABLE_GiftMenu[0], "GiftMenu");
		Hook<RE::FavoritesMenu>::Install(RE::VTABLE_FavoritesMenu[0], "FavoritesMenu");
		Hook<RE::MagicMenu>::Install(RE::VTABLE_MagicMenu[0], "MagicMenu");
		Hook<RE::CraftingMenu>::Install(RE::VTABLE_CraftingMenu[0], "CraftingMenu");
		return true;
	}
}

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

#include <cstring>

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
			} else if (auto* book = a_form->As<RE::TESObjectBOOK>()) {
				SetNumber(a_obj, "flags", static_cast<double>(book->data.flags.underlying()));
				SetNumber(a_obj, "bookType", static_cast<double>(book->data.type.underlying()));
			}
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

		// メニューごとの、割り込みの関数。元の ProcessMessage を呼んだあとで、項目を足す。
		using ProcessMessage_t = RE::UI_MESSAGE_RESULTS (*)(RE::IMenu*, RE::UIMessage&);

		template <class Menu>
		struct Hook
		{
			static inline ProcessMessage_t original = nullptr;

			static RE::UI_MESSAGE_RESULTS Thunk(RE::IMenu* a_menu, RE::UIMessage& a_message)
			{
				const auto result = original(a_menu, a_message);
				switch (a_message.type.get()) {
				case RE::UI_MESSAGE_TYPE::kShow:
				case RE::UI_MESSAGE_TYPE::kReshow:
				case RE::UI_MESSAGE_TYPE::kInventoryUpdate:
				case RE::UI_MESSAGE_TYPE::kUpdate:
					if constexpr (std::is_same_v<Menu, RE::FavoritesMenu>) ExtendFavorites(static_cast<RE::FavoritesMenu*>(a_menu));
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
		return true;
	}
}

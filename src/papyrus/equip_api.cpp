// お気に入りのグループを装備するために SkyUI が使う、SKSE 追加のネイティブ関数。
// (SKI_FavoritesManager が使う: Form.GetName、Weapon.GetWeaponType、Armor.GetSlotMask、Spell.GetEquipType、
//  Actor の装備まわり、Game.IsObjectFavorited、Input.GetMappedControl)
//
// 入力: Input.GetMappedControl は、キー番号から操作名を求める(変換表は events.cpp)。
//
// アイテム ID(itemId)について: SKSE では ExtendData がアイテムごとの固有 ID を割り当てる。
// Skyshim の ExtendData は未実装なので、SkyUI は常に itemId = 0(「同じアイテムのどれでもよい」)を渡す。
// そのため GetEquippedItemId / GetWornItemId は常に 0 を返し、EquipItemById は itemId を使わずに装備する。
#include "../engine.h"
#include "../events/events.h"

#include "RE/Skyrim.h"

#include <string>

namespace skyshim::papyrus
{
	namespace
	{
		// SKSE の手の番号(装備と解除の関数): 0 = 既定、1 = 右手、2 = 左手
		const RE::BGSEquipSlot* HandSlot(std::int32_t a_slot)
		{
			auto* mgr = RE::BGSDefaultObjectManager::GetSingleton();
			if (!mgr) return nullptr;
			switch (a_slot) {
			case 1: return static_cast<const RE::BGSEquipSlot*>(mgr->GetObject(RE::DEFAULT_OBJECT::kRightHandEquip));
			case 2: return static_cast<const RE::BGSEquipSlot*>(mgr->GetObject(RE::DEFAULT_OBJECT::kLeftHandEquip));
			default: return nullptr;
			}
		}

		// ---- Form / Weapon / Armor / Spell ----

		std::string FormGetName(RE::TESForm* a_self)
		{
			const char* name = a_self ? a_self->GetName() : nullptr;
			return name ? std::string(name) : std::string();
		}

		std::int32_t WeaponGetWeaponType(RE::TESObjectWEAP* a_self) { return a_self ? static_cast<std::int32_t>(a_self->GetWeaponType()) : 0; }

		std::int32_t ArmorGetSlotMask(RE::TESObjectARMO* a_self) { return a_self ? static_cast<std::int32_t>(a_self->GetSlotMask()) : 0; }

		// 呪文の装備スロット(右手、左手、どちらでも、両手、声)を表すフォームを返す。
		RE::BGSEquipSlot* SpellGetEquipType(RE::SpellItem* a_self) { return a_self ? a_self->GetEquipSlot() : nullptr; }

		// ---- Actor ----

		// 防具の部位マスクに装備している防具を返す。
		RE::TESForm* ActorGetWornForm(RE::Actor* a_self, std::int32_t a_mask)
		{
			return a_self ? a_self->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(a_mask)) : nullptr;
		}

		// 0 = 左手、1 = 右手、2 = 声(シャウトまたはパワー)
		RE::TESForm* ActorGetEquippedObject(RE::Actor* a_self, std::int32_t a_slot)
		{
			if (!a_self) return nullptr;
			switch (a_slot) {
			case 0: return a_self->GetEquippedObject(true);
			case 1: return a_self->GetEquippedObject(false);
			case 2: return a_self->selectedPower;
			default: return nullptr;
			}
		}

		std::int32_t ActorGetEquippedItemId(RE::Actor*, std::int32_t) { return 0; }
		std::int32_t ActorGetWornItemId(RE::Actor*, std::int32_t) { return 0; }

		void Equip(RE::Actor* a_actor, RE::TESForm* a_item, std::int32_t a_slot, bool a_preventUnequip, bool a_sound)
		{
			auto* mgr = RE::ActorEquipManager::GetSingleton();
			if (!a_actor || !a_item || !mgr) return;
			if (auto* spell = a_item->As<RE::SpellItem>()) {
				mgr->EquipSpell(a_actor, spell, HandSlot(a_slot));
			} else if (auto* shout = a_item->As<RE::TESShout>()) {
				mgr->EquipShout(a_actor, shout);
			} else if (auto* object = a_item->As<RE::TESBoundObject>()) {
				mgr->EquipObject(a_actor, object, nullptr, 1, HandSlot(a_slot), false, a_preventUnequip, a_sound, true);
			}
		}

		void ActorEquipItemEx(RE::Actor* a_self, RE::TESForm* a_item, std::int32_t a_slot, bool a_preventUnequip, bool a_sound)
		{
			Equip(a_self, a_item, a_slot, a_preventUnequip, a_sound);
		}

		// itemId は使わない(上の説明を参照)。
		void ActorEquipItemById(RE::Actor* a_self, RE::TESForm* a_item, std::int32_t, std::int32_t a_slot, bool a_preventUnequip, bool a_sound)
		{
			Equip(a_self, a_item, a_slot, a_preventUnequip, a_sound);
		}

		void ActorUnequipItemEx(RE::Actor* a_self, RE::TESForm* a_item, std::int32_t a_slot, bool a_preventEquip)
		{
			auto* mgr = RE::ActorEquipManager::GetSingleton();
			auto* object = a_item ? a_item->As<RE::TESBoundObject>() : nullptr;
			if (!a_self || !object || !mgr) return;
			mgr->UnequipObject(a_self, object, nullptr, 1, HandSlot(a_slot), false, a_preventEquip, true, true);
		}

		// ---- Game / Input ----

		// お気に入りに入っているか。呪文とシャウトは、お気に入りの一覧にあるか。それ以外は、所持品のどれかに「お気に入り」の印があるか。
		bool GameIsObjectFavorited(RE::StaticFunctionTag*, RE::TESForm* a_form)
		{
			if (!a_form) return false;
			if (a_form->Is(RE::FormType::Spell, RE::FormType::Shout)) {
				auto* favorites = RE::MagicFavorites::GetSingleton();
				if (!favorites) return false;
				for (auto* f : favorites->spells)
					if (f == a_form) return true;
				return false;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* changes = player ? player->GetInventoryChanges() : nullptr;
			if (!changes || !changes->entryList) return false;
			for (auto* entry : *changes->entryList) {
				if (!entry || entry->object != a_form || !entry->extraLists) continue;
				for (auto* extra : *entry->extraLists)
					if (extra && extra->HasType(RE::ExtraDataType::kHotkey)) return true;
			}
			return false;
		}

		// キー番号に割り当てられている操作(ゲームプレイ時)の名前を返す。割り当てが無ければ空(変換は events.cpp)。
		// キー番号: キーボード 0〜255、マウス 256〜265、ゲームパッド 266〜
		std::string InputGetMappedControl(RE::StaticFunctionTag*, std::int32_t a_key)
		{
			return events::MappedControl(a_key);
		}
	}

	bool RegisterEquip(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) return false;
		a_vm->RegisterFunction("GetName", "Form", FormGetName);
		a_vm->RegisterFunction("GetWeaponType", "Weapon", WeaponGetWeaponType);
		a_vm->RegisterFunction("GetSlotMask", "Armor", ArmorGetSlotMask);
		a_vm->RegisterFunction("GetEquipType", "Spell", SpellGetEquipType);
		a_vm->RegisterFunction("GetWornForm", "Actor", ActorGetWornForm);
		a_vm->RegisterFunction("GetEquippedObject", "Actor", ActorGetEquippedObject);
		a_vm->RegisterFunction("GetEquippedItemId", "Actor", ActorGetEquippedItemId);
		a_vm->RegisterFunction("GetWornItemId", "Actor", ActorGetWornItemId);
		a_vm->RegisterFunction("EquipItemEx", "Actor", ActorEquipItemEx);
		a_vm->RegisterFunction("EquipItemById", "Actor", ActorEquipItemById);
		a_vm->RegisterFunction("UnequipItemEx", "Actor", ActorUnequipItemEx);
		a_vm->RegisterFunction("IsObjectFavorited", "Game", GameIsObjectFavorited);
		a_vm->RegisterFunction("GetMappedControl", "Input", InputGetMappedControl);
		return true;
	}
}

#pragma once
// SkyrimVM の中の impl(Papyrus の仮想マシン本体)の取り出し。
// CommonLib の定義は 1.5.97 の配置(+0x200)。1.6 以降(1.7.104 で実機確認)は全体が 0x10 後ろにずれ、+0x210 にある。
// 版ごとの切り替えは、CommonLib の SkyrimVM::GetImpl() にある(CommonLib 自身も、これを使うように直してある)。
#include "RE/Skyrim.h"

namespace skyshim
{
	inline RE::BSScript::IVirtualMachine* VMImpl(RE::SkyrimVM* a_vm)
	{
		return a_vm ? a_vm->GetImpl() : nullptr;
	}
}

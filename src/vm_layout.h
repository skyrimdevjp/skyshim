#pragma once
// SkyrimVM の中の impl(Papyrus の仮想マシン本体)の取り出し。
// CommonLib の定義は 1.5.97 の配置(+0x200)。1.6 以降(1.7.104 で実機確認)は全体が 0x10 後ろにずれ、+0x210 にある。
// ビルドの種類(SE / AE)は、実行するゲームの版と一致することを、runtime.cpp で確認している。
#include <cstdint>
#include "RE/Skyrim.h"

namespace skyshim
{
	inline RE::BSScript::IVirtualMachine* VMImpl(RE::SkyrimVM* a_vm)
	{
		if (!a_vm) return nullptr;
#if defined(SKYRIM_SUPPORT_AE)
		constexpr std::size_t kImplOffset = 0x210;
#else
		constexpr std::size_t kImplOffset = 0x200;
#endif
		return *reinterpret_cast<RE::BSScript::IVirtualMachine**>(reinterpret_cast<std::uintptr_t>(a_vm) + kImplOffset);
	}
}

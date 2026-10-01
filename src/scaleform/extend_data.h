#pragma once

// 拡張データ(SKSE の skse.ExtendData に相当): 一覧の各アイテムに、SkyUI が読む項目を足す。
namespace skyshim::scaleform
{
	// 各メニューの ProcessMessage に割り込む(仮想関数の表の書き換え)。メインメニューが開いてから、1 回だけ呼ぶ。
	bool InstallExtendData(void (*a_log)(const char*, ...));
}

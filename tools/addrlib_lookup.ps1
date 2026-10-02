# Address Library(version-*.bin、形式 1。Skyrim 1.5.x 用)で、実行ファイルの中のアドレス(RVA)から、
# その位置を含む関数の ID と、先頭のアドレスを求める。クラッシュのログの「SkyrimSE.exe +0x...」を調べるのに使う。
#
# 使い方:
#   .\addrlib_lookup.ps1 -Bin "<...>\Data\SKSE\Plugins\version-1-5-97-0.bin" -Rva 0x289D3C
#
# 結果の例:
#   nearest id=19154 offset=0x289D30 (+0xC) | next id=19155 offset=0x289D50
#   → 0x289D3C は、ID 19154(先頭 0x289D30)の関数の、先頭から 12 バイト目。
#
# 先頭のアドレスが分かったら、逆アセンブル(tools\disasm.bat)で、その関数の命令を読む。

param(
    [Parameter(Mandatory = $true)] [string] $Bin,
    [Parameter(Mandatory = $true)] [string] $Rva
)

$ErrorActionPreference = 'Stop'

$source = @'
using System;
using System.IO;
public static class AddrLib
{
    public static string Find(string path, ulong target)
    {
        var r = new BinaryReader(File.OpenRead(path));
        int format = r.ReadInt32();
        r.ReadInt32(); r.ReadInt32(); r.ReadInt32(); r.ReadInt32();      // バージョン(4 つ)
        int nameLength = r.ReadInt32(); r.ReadBytes(nameLength);
        int pointerSize = r.ReadInt32();
        int count = r.ReadInt32();
        ulong previousId = 0, previousOffset = 0;
        ulong bestId = 0, bestOffset = 0, nextId = 0, nextOffset = ulong.MaxValue;
        bool have = false;
        for (int i = 0; i < count; i++)
        {
            byte type = r.ReadByte();
            int lo = type & 7, hi = type >> 4;
            ulong id = 0, offset = 0;
            switch (lo)
            {
                case 0: id = r.ReadUInt64(); break;
                case 1: id = previousId + 1; break;
                case 2: id = previousId + r.ReadByte(); break;
                case 3: id = previousId - r.ReadByte(); break;
                case 4: id = previousId + r.ReadUInt16(); break;
                case 5: id = previousId - r.ReadUInt16(); break;
                case 6: id = r.ReadUInt16(); break;
                case 7: id = r.ReadUInt32(); break;
            }
            ulong basis = ((hi & 8) != 0) ? previousOffset / (ulong)pointerSize : previousOffset;
            switch (hi & 7)
            {
                case 0: offset = r.ReadUInt64(); break;
                case 1: offset = basis + 1; break;
                case 2: offset = basis + r.ReadByte(); break;
                case 3: offset = basis - r.ReadByte(); break;
                case 4: offset = basis + r.ReadUInt16(); break;
                case 5: offset = basis - r.ReadUInt16(); break;
                case 6: offset = r.ReadUInt16(); break;
                case 7: offset = r.ReadUInt32(); break;
            }
            if ((hi & 8) != 0) offset *= (ulong)pointerSize;
            previousId = id; previousOffset = offset;
            if (offset <= target && (!have || offset > bestOffset)) { bestOffset = offset; bestId = id; have = true; }
            if (offset > target && offset < nextOffset) { nextOffset = offset; nextId = id; }
        }
        return "format=" + format + " count=" + count + " | nearest id=" + bestId + " offset=0x" + bestOffset.ToString("X")
            + " (+0x" + (target - bestOffset).ToString("X") + ") | next id=" + nextId + " offset=0x" + nextOffset.ToString("X");
    }
}
'@

Add-Type -TypeDefinition $source
[AddrLib]::Find($Bin, [Convert]::ToUInt64($Rva.Replace('0x', ''), 16))

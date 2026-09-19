# -*- coding: utf-8 -*-
# 生成 GB2312 一级字库（0xB0A1-0xD7F9，3755 字）的拼音首字母映射表（C++ 格式）
import sys
from pypinyin import pinyin, Style

# GB2312 一级字库: 区 16-55 (0xB0-0xD7), 位 1-94 (0xA1-0xFE)
entries = []  # (unicode_cp, initial)
for qu in range(0xB0, 0xD8):
    for wei in range(0xA1, 0xFF):
        # GB2312 编码 -> Unicode
        gb = bytes([qu, wei])
        try:
            s = gb.decode('gb2312')
        except UnicodeDecodeError:
            continue
        cp = ord(s)
        # 取首字母
        py = pinyin(s, style=Style.FIRST_LETTER, strict=True)
        if not py or not py[0] or not py[0][0]:
            continue
        ini = py[0][0].lower()
        if not ('a' <= ini <= 'z'):
            continue
        entries.append((cp, ini))

# 去重（同一个码点只保留一次）
seen = set()
uniq = []
for cp, ini in entries:
    if cp not in seen:
        seen.add(cp)
        uniq.append((cp, ini))
uniq.sort()

# 输出 C++ 表
with open('pinyin_table.inc', 'w', encoding='utf-8') as f:
    f.write('// 自动生成：GB2312 一级字库拼音首字母映射（%d 字）\n' % len(uniq))
    f.write('struct PyUniRange { char ch; uint32_t lo; uint32_t hi; };\n')
    f.write('static const PyUniRange kUniTable[] = {\n')
    for cp, ini in uniq:
        f.write("    {'%s', 0x%04X, 0x%04X},\n" % (ini, cp, cp))
    f.write('};\n')
    f.write('static const size_t kUniCount = sizeof(kUniTable) / sizeof(kUniTable[0]);\n')

print('generated %d entries' % len(uniq))

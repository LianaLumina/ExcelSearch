#pragma once

#include <string>

// 拼音首字母工具：将中文字符串转换为拼音首字母串
// 例："核定工日" -> "hdgr"；"张三" -> "zs"
// 实现：GB2312 一级字库（3755 字，按拼音排序）区间查表 + 二级字库位图
// 非汉字字符（ASCII）原样保留小写
namespace pinyin {

// 返回字符串的拼音首字母（ASCII 小写），非中文保留原字符小写
std::string initials(const std::string& utf8str);

}

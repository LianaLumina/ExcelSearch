#pragma once
// 跨文件公共设施：语言包装与窗口通用常量。
// 说明：T() 目前是直通实现（单语言版本），保留它是为了将来接入多语言时只改这一处。
#include <QString>

inline QString T(const QString& s) { return s; }

inline constexpr int kTitleH = 46;   // 自绘标题栏高度（窗口拖动/双击区）

#pragma once
// 主题：调色板（Proto）、配色推导（makeProto）、半透明背景取样（backDropOf）与全局样式表（qssFor）。
#include <QColor>
#include <QString>

class QWidget;

struct Proto { QColor accent, panelBg, card, hover, pressed, text, sub, editBg, editBorder, border, altRow, headerBg, closeHover; };

Proto makeProto(bool dark, const QColor& accent);
QColor backDropOf(const QWidget* w, const Proto& p);
QString qssFor(const Proto& p);

#include "widgets.h"
#include "common.h"


StateTint* attachStateTint(QPushButton* b, const QString& sel, int kind, std::function<Proto()> proto) {
    if (!b || g_noAnim) return nullptr;   // 关闭动效：不安装，行为与动效前完全一致
    if (kind == HtPrimary) {   // 强调色底：hover 加深 12% → 按下再深到 25%（与 qssFor 的 :pressed 同源）
        return new StateTint(b, sel, [proto] { return proto().accent; },
                                      [proto] { return proto().accent.darker(112); },
                                      [proto] { return proto().accent.darker(125); },
                                      nullptr, nullptr, nullptr, nullptr, 8, 11);
    }
    if (kind == HtTb) {   // #tbBtn:hover 同时改底色与文字色；圆角 6 → 9
        return new StateTint(b, sel, [b, proto] { return backDropOf(b, proto()); },
                                      [proto] { return proto().hover; },
                                      [proto] { return proto().pressed; },
                                      [proto] { return proto().sub; },
                                      [proto] { return proto().text; },
                                      nullptr, nullptr, 6, 9);
    }
    if (kind == HtNav) {  // 顶部标签：底色恒等于底色本身（等于不画），只过渡文字色；无圆角变形
        return new StateTint(b, sel, [b, proto] { return backDropOf(b, proto()); },
                                      [b, proto] { return backDropOf(b, proto()); },
                                      nullptr,
                                      [proto] { return proto().sub; },
                                      [proto] { return proto().text; });
    }
    // #themeBtn 等：底色 → hover 底色 → 按下底色；圆角 8 → 11（只改绘制，不触发 relayout）
    return new StateTint(b, sel, [b, proto] { return backDropOf(b, proto()); },
                                  [proto] { return proto().hover; },
                                  [proto] { return proto().pressed; },
                                  nullptr, nullptr, nullptr, nullptr, 8, 11);
}


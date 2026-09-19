#include "theme.h"
#include "common.h"

#include <QWidget>

Proto makeProto(bool dark, const QColor& accent) {
    Proto p; p.accent = accent;
    if (dark) {
        p.panelBg = QColor("#17171C"); p.card = QColor("#24242B"); p.hover = QColor("#2C2C34");
        p.pressed = QColor("#35353E"); p.text = QColor("#E7E7EB"); p.sub = QColor("#8B8B95");
        p.editBg = QColor("#1F1F25"); p.editBorder = QColor("#35353E"); p.border = QColor("#2E2E37");
        p.altRow = QColor("#1E1E24"); p.headerBg = QColor("#282830"); p.closeHover = QColor("#C14242");
    } else {
        p.panelBg = QColor("#F5F6FA"); p.card = QColor("#FFFFFF"); p.hover = QColor("#E9EDF3");
        p.pressed = QColor("#DCE4EE"); p.text = QColor("#1D1D24"); p.sub = QColor("#6A6A72");
        p.editBg = QColor("#F6F8FA"); p.editBorder = QColor("#D8DDE6"); p.border = QColor("#E2E6EE");
        p.altRow = QColor("#F7F9FC"); p.headerBg = QColor("#ECF1F8"); p.closeHover = QColor("#E5484D");
    }
    return p;
}

QColor backDropOf(const QWidget* w, const Proto& p) {
    for (const QWidget* a = w ? w->parentWidget() : nullptr; a; a = a->parentWidget())
        if (a->objectName() == "card") return p.card;
    return p.panelBg;
}

QString qssFor(const Proto& p) {
    QString s, nl = "\n";
    auto C = [](const QColor& cc) { return cc.name(); };
    auto R = [&](const QString& sel, const QString& body) { s += sel + " { " + body + " }" + nl; };
    R(T("QWidget"), T("background:transparent;"));   // 默认透明，避免深色下未覆盖控件露白底；具体控件(panel/card/table等)再覆盖
    R(T("#panel"), T("background:") + C(p.panelBg) + "; border:1px solid " + C(p.border) + "; border-radius:12px");
    R(T("QLabel"), T("color:") + C(p.text));
    R(T("#appTitle"), T("font-size:15px; font-weight:600; color:") + C(p.text));
        R(T("#manualView"), T("background:transparent; border:none; padding:2px 8px; font-size:14px; color:") + C(p.text));
    R(T("#manualDeco"), T("background:") + C(p.altRow) + T("; border:1px dashed ") + C(p.border) + T("; border-radius:10px"));R(T("#cardTitle"), T("font-size:14px; font-weight:600; color:") + C(p.text));
    R(T("#statVal"), T("font-size:22px; font-weight:700; color:") + C(p.accent));
    R(T("QPushButton#statVal"), T("padding:0; background:transparent; border:none;"));
    R(T("#statLabel"), T("color:") + C(p.sub));
    R(T("#status"), T("color:") + C(p.sub) + "; font-size:12px; padding:2px 2px");
    R(T("QPushButton#tbBtn"), T("background:transparent; border:none; border-radius:6px; color:") + C(p.sub) + "; font-size:14px; padding:6px 12px");
    R(T("QPushButton#tbBtn:hover"), T("background:") + C(p.hover) + "; color:" + C(p.text));
    R(T("QPushButton#navBtn"), T("background:transparent; border:none; padding:9px 20px; color:") + C(p.sub) + "; font-size:15px; border-bottom:2px solid transparent");
    R(T("QPushButton#navBtn:hover"), T("color:") + C(p.text));
    R(T("QPushButton#navBtn:checked"), T("color:") + C(p.accent) + "; font-weight:600; border-bottom:2px solid " + C(p.accent));
    R(T("QListWidget"), T("background:transparent; border:none; outline:none;"));
    R(T("QListWidget::item"), T("padding:11px 14px; border-radius:8px; color:") + C(p.sub) + "; font-size:14px");
    // 注意：这里**故意只留文字色、不留 background** —— hover 底色改由 RowDelegate 自绘才能做颜色过渡
    // （QSS 不支持 transition）。若在这里补回 background，QSS 的瞬时不透明底色会直接盖住动画，
    // 过渡看起来"没生效"。文字色仍瞬时切换：QSS 的 ::item 文字色会覆盖委托里设的 palette，压不住。
    // 安装委托见 makeResultCard() / enableRowHoverAnim()。
    R(T("QListWidget::item:hover"), T("color:") + C(p.text));
    R(T("QListWidget::item:selected"), T("background:") + C(p.accent) + "; color:#FFFFFF; font-weight:600");
    R(T("QLineEdit"), T("background:") + C(p.editBg) + "; border:1px solid " + C(p.editBorder) + "; border-radius:8px; padding:7px 12px; color:" + C(p.text));
    R(T("QLineEdit:focus"), T("border:1px solid ") + C(p.accent));
    R(T("QComboBox"), T("background:") + C(p.editBg) + "; border:1px solid " + C(p.editBorder) + "; border-radius:8px; padding:6px 10px; color:" + C(p.text));
    R(T("QComboBox::drop-down"), T("border:none;"));
    R(T("QCheckBox"), T("color:") + C(p.text) + "; spacing:8px;");
    // 指示器必须显式画：全局 QWidget{background:transparent} 之后，未选中态的指示器会整个不可见
    R(T("QCheckBox::indicator"), T("width:16px; height:16px; border:1px solid ") + C(p.editBorder) + "; border-radius:4px; background:" + C(p.editBg));
    R(T("QCheckBox::indicator:hover"), T("border:1px solid ") + C(p.accent));
    R(T("QCheckBox::indicator:checked"), T("background:") + C(p.accent) + "; border:1px solid " + C(p.accent));
    R(T("QCheckBox::indicator:disabled"), T("border:1px solid ") + C(p.border) + "; background:transparent");
    R(T("QRadioButton"), T("color:") + C(p.text) + "; spacing:8px;");
    R(T("QRadioButton::indicator"), T("width:16px; height:16px; border:1px solid ") + C(p.editBorder) + "; border-radius:9px; background:" + C(p.editBg));
    R(T("QRadioButton::indicator:hover"), T("border:1px solid ") + C(p.accent));
    R(T("QRadioButton::indicator:checked"), T("background:") + C(p.accent) + "; border:1px solid " + C(p.accent));
    R(T("QSpinBox"), T("background:") + C(p.editBg) + "; border:1px solid " + C(p.editBorder) + "; border-radius:8px; padding:5px 8px; color:" + C(p.text));
    R(T("QScrollArea"), T("background:transparent; border:none;"));
    R(T("QLineEdit#searchEdit"), T("background:") + C(p.editBg) + "; border:1px solid " + C(p.editBorder) + "; border-radius:10px; padding:9px 14px; color:" + C(p.text));
    R(T("QLineEdit#searchEdit:focus"), T("border:1px solid ") + C(p.accent));
    R(T("QPushButton"), T("background:transparent; border:none; border-radius:8px; padding:8px 16px; color:" + C(p.text)));
    R(T("QPushButton:hover"), T("background:") + C(p.hover));
    R(T("QPushButton:pressed"), T("background:") + C(p.pressed));
    R(T("QPushButton#primaryBtn"), T("background:") + C(p.accent) + "; color:#FFFFFF; font-weight:600");
    R(T("QPushButton#primaryBtn:hover"), T("background:") + C(p.accent.darker(112)));
    R(T("QPushButton#primaryBtn:pressed"), T("background:") + C(p.accent.darker(125)));
    R(T("QPushButton#themeToggle"), T("border:1px solid ") + C(p.editBorder) + "; padding:7px 16px; color:" + C(p.text));
    R(T("QPushButton#themeToggle:checked"), T("background:") + C(p.accent) + "; color:#FFFFFF; border:1px solid " + C(p.accent));
    R(T("QFrame#card"), T("background:") + C(p.card) + "; border:1px solid " + C(p.border) + "; border-radius:14px");
    R(T("QTableWidget"), T("background:") + C(p.card) + "; alternate-background-color:" + C(p.altRow) + "; color:" + C(p.text) + "; gridline-color:transparent; border:1px solid " + C(p.border) + "; border-radius:12px");
    R(T("QTableWidget::item"), T("padding:6px; border:none"));
    R(T("QTableWidget::item:selected"), T("background:") + C(p.accent) + "; color:#FFFFFF");
    R(T("QPlainTextEdit"), T("background:") + C(p.card) + "; color:" + C(p.text) + "; border:1px solid " + C(p.border) + "; border-radius:8px; padding:10px; font-size:14px;");
    R(T("QHeaderView::section"), T("background:") + C(p.headerBg) + "; color:" + C(p.text) + "; border:none; padding:8px; font-weight:600");
    R(T("QScrollBar:vertical"), T("background:transparent; width:10px"));
    R(T("QScrollBar::handle:vertical"), T("background:") + C(p.hover) + "; border-radius:5px; min-height:30px");
    R(T("QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical"), T("height:0"));
    // 右键菜单：与无边框 MAA 风格统一（去掉原生 3D 边框/阴影感）
    R(T("QMenu"), T("background:") + C(p.card) + "; border:1px solid " + C(p.border) + "; border-radius:10px; padding:6px; color:" + C(p.text));
    R(T("QMenu::item"), T("padding:7px 26px 7px 14px; border-radius:6px; background:transparent"));
    R(T("QMenu::item:selected"), T("background:") + C(p.accent) + "; color:#FFFFFF");
    R(T("QMenu::item:disabled"), T("color:") + C(p.sub));
    R(T("QMenu::separator"), T("height:1px; background:") + C(p.border) + "; margin:5px 8px");
    // 左下角临时提示气泡（等价原版 g_hToast 的 10 秒提示）
    R(T("#toast"), T("background:") + C(p.accent) + "; color:#FFFFFF; border-radius:8px; padding:8px 14px; font-size:13px; font-weight:600");
    // 高级设置解锁层
    R(T("#lockIcon"), T("font-size:40px; color:") + C(p.sub));
    R(T("#lockErr"), T("color:#E5484D; font-size:12px;"));
    return s;
}

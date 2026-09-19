#include "data_model.h"
#include <QWidget>
#include <QPainter>

QString markColorName(int i) {
    switch (i) {
    case 0: return T("红色");
    case 1: return T("紫色");
    case 2: return T("蓝色");
    case 3: return T("绿色");
    case 4: return T("黄色");
    default: return QString();
    }
}

int markColorIndexByName(const std::string& name) {
    for (int i = 0; i < 5; i++) if (name == markColorName(i).toUtf8().toStdString()) return i;
    return -1;
}

std::vector<std::string> defaultExtraCols() {
    return { kMetaSheetCol, kMetaRowCol };
}

void enableRowHoverAnim(QListWidget* list, const Proto& p, std::vector<RowDelegate*>* reg) {
    if (!list) return;
    auto* d = new RowDelegate(list, p);
    d->setRounded(true);
    list->setItemDelegate(d);
    if (reg) reg->push_back(d);
}

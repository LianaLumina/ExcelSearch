#include "manual_window.h"
#include "common.h"


QString manualText() {
    // exe 同目录优先：MANUAL.md（新命名）→ 使用说明书.md（旧包兼容）→ 内嵌副本
    for (const char* name : { "/MANUAL.md", "/使用说明书.md" }) {
        QFile f(QCoreApplication::applicationDirPath() + QString::fromUtf8(name));
        if (f.exists() && f.open(QIODevice::ReadOnly)) return QString::fromUtf8(f.readAll());
    }
    QFile r(":/manual.md");
    if (r.open(QIODevice::ReadOnly)) return QString::fromUtf8(r.readAll());
    return QString();
}

QVector<QPair<QString, QString>> manualSections(const QString& md) {
    QVector<QPair<QString, QString>> out;
    out.push_back({ T("全部内容"), md });
    QString title; QStringList body;
    for (const QString& ln : md.split('\n')) {
        if (ln.startsWith("### ")) {
            if (!title.isEmpty()) out.push_back({ title, body.join('\n') });
            title = ln.mid(4).trimmed(); body.clear(); body << ln;
        } else if (!title.isEmpty()) body << ln;
    }
    if (!title.isEmpty()) out.push_back({ title, body.join('\n') });
    return out;
}

QString manualOverflowSections() {
    QTextBrowser tb;
    tb.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tb.setLineWrapMode(QTextEdit::WidgetWidth);
    tb.resize(648, 420);   // 与手册窗口里正文区的实际可用尺寸一致
    QStringList bad;
    for (const auto& s : manualSections(manualText())) {
        tb.setMarkdown(s.second);
        if (tb.horizontalScrollBar()->maximum() > 0) bad << s.first;
    }
    return bad.isEmpty() ? QStringLiteral("none") : bad.join(QStringLiteral(" / "));
}

QString manualHash(const QString& md) {
    return QString::fromLatin1(QCryptographicHash::hash(md.toUtf8(), QCryptographicHash::Md5).toHex());
}


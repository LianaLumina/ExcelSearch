#pragma once
// 对话框：附加密码输入、行详情、通用确认、关闭方式选择。
// 说明：全部沿用主题（theme.h）与自绘控件（widgets.h）的视觉，不弹原生 QMessageBox。
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QScrollArea>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QColor>
#include <QString>
#include <functional>

#include "common.h"
#include "theme.h"
#include "animations.h"
#include "widgets.h"

class PasswordDialog : public QDialog {
public:
    PasswordDialog(bool dark, const QColor& accent, const QString& title, const QString& prompt, QWidget* parent)
        : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setModal(true);
        setWindowTitle(T("使用说明书"));   // 无边框下不显示，但便于窗口枚举/辅助功能
        setStyleSheet(qssFor(makeProto(dark, accent)));
        auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0);
        auto* panel = new QWidget; panel->setObjectName("panel");
        auto* v = new QVBoxLayout(panel); v->setContentsMargins(24, 22, 24, 24); v->setSpacing(14);
        auto* t = new QLabel(title); t->setObjectName("appTitle"); v->addWidget(t);
        auto* pt = new QLabel(prompt); pt->setObjectName("statLabel"); pt->setWordWrap(true); v->addWidget(pt);
        m_edit = new QLineEdit; m_edit->setEchoMode(QLineEdit::Password); m_edit->setMinimumHeight(38); m_edit->setObjectName("searchEdit");
        v->addWidget(m_edit);
        auto* row = new QHBoxLayout; row->setSpacing(10);
        auto* ok = new QPushButton(T("确定")); ok->setObjectName("primaryBtn");
        auto* cancel = new QPushButton(T("取消")); cancel->setObjectName("themeBtn");
        row->addStretch(); row->addWidget(cancel); row->addWidget(ok);
        v->addLayout(row);
        root->addWidget(panel);
        connect(ok, &QPushButton::clicked, this, [this] { if (!m_edit->text().isEmpty()) accept(); });
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(m_edit, &QLineEdit::returnPressed, this, [this] { if (!m_edit->text().isEmpty()) accept(); });
    }
    QString password() const { return m_edit->text(); }
private:
    QLineEdit* m_edit = nullptr;
};

class DetailDialog : public QDialog {
public:
    DetailDialog(bool dark, const QColor& accent, const QString& title, const QString& text, QWidget* parent)
        : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setModal(true);
        setWindowTitle(T("使用说明书"));   // 无边框下不显示，但便于窗口枚举/辅助功能
        setStyleSheet(qssFor(makeProto(dark, accent)));
        auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0);
        auto* panel = new QWidget; panel->setObjectName("panel");
        auto* v = new QVBoxLayout(panel); v->setContentsMargins(24, 20, 24, 20); v->setSpacing(12);
        auto* t = new QLabel(title); t->setObjectName("appTitle"); v->addWidget(t);
        auto* te = new QPlainTextEdit(text); te->setReadOnly(true); te->setMinimumSize(540, 320); v->addWidget(te);
        auto* row = new QHBoxLayout; auto* close = new QPushButton(T("关闭")); close->setObjectName("primaryBtn");
        row->addStretch(); row->addWidget(close); v->addLayout(row);
        root->addWidget(panel);
        connect(close, &QPushButton::clicked, this, &QDialog::accept);
    }
};

class ConfirmDialog : public QDialog {
public:
    ConfirmDialog(bool dark, const QColor& accent, const QString& title, const QString& message,
                  const QString& okText, QWidget* parent)
        : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setModal(true);
        setWindowTitle(T("使用说明书"));   // 无边框下不显示，但便于窗口枚举/辅助功能
        setStyleSheet(qssFor(makeProto(dark, accent)));
        auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0);
        auto* panel = new QWidget; panel->setObjectName("panel");
        auto* v = new QVBoxLayout(panel); v->setContentsMargins(24, 22, 24, 24); v->setSpacing(14);
        auto* t = new QLabel(title); t->setObjectName("appTitle"); v->addWidget(t);
        auto* msg = new QLabel(message); msg->setObjectName("statLabel"); msg->setWordWrap(true);
        msg->setMinimumWidth(380); v->addWidget(msg);
        auto* row = new QHBoxLayout; row->setSpacing(10);
        auto* cancel = new QPushButton(T("取消")); cancel->setObjectName("themeBtn");
        auto* ok = new QPushButton(okText); ok->setObjectName("primaryBtn");
        row->addStretch(); row->addWidget(cancel); row->addWidget(ok);
        v->addLayout(row);
        root->addWidget(panel);
        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }
    static bool ask(QWidget* parent, bool dark, const QColor& accent, const QString& title,
                    const QString& message, const QString& okText = QString()) {
        ConfirmDialog dlg(dark, accent, title, message, okText.isEmpty() ? T("确定") : okText, parent);
        return dlg.exec() == QDialog::Accepted;
    }
};

class CloseDialog : public QDialog {
public:
    enum Choice { None = 0, ToTray = 1, DirectClose = 2 };
    CloseDialog(bool dark, const QColor& accent, QWidget* parent) : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setModal(true);
        setWindowTitle(T("使用说明书"));   // 无边框下不显示，但便于窗口枚举/辅助功能
        setStyleSheet(qssFor(makeProto(dark, accent)));
        auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0);
        auto* panel = new QWidget; panel->setObjectName("panel");
        auto* v = new QVBoxLayout(panel); v->setContentsMargins(24, 22, 24, 20); v->setSpacing(14);
        auto* t = new QLabel(T("关闭")); t->setObjectName("appTitle"); v->addWidget(t);
        auto* msg = new QLabel(T("请选择关闭方式。")); msg->setObjectName("statLabel"); msg->setWordWrap(true);
        v->addWidget(msg);
        auto* row = new QHBoxLayout; row->setSpacing(10);   // 左下：不再询问；右下：两个动作
        m_noAsk = new QCheckBox(T("不再询问"));
        row->addWidget(m_noAsk); row->addStretch();
        auto* trayBtn = new QPushButton(T("最小化到托盘")); trayBtn->setObjectName("themeBtn");
        auto* closeBtn = new QPushButton(T("直接关闭")); closeBtn->setObjectName("primaryBtn");
        row->addWidget(trayBtn); row->addWidget(closeBtn);
        v->addLayout(row);
        root->addWidget(panel);
        connect(trayBtn, &QPushButton::clicked, this, [this] { m_choice = ToTray; accept(); });
        connect(closeBtn, &QPushButton::clicked, this, [this] { m_choice = DirectClose; accept(); });
        // Esc 关闭对话框 = 取消本次关闭（窗口保持打开），避免被"两个选项"困住
    }
    bool noAsk() const { return m_noAsk && m_noAsk->isChecked(); }
    Choice choice() const { return (Choice)m_choice; }
private:
    QCheckBox* m_noAsk = nullptr;
    int m_choice = None;
};

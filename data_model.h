#pragma once
// 数据模型：加载后的文档与文件、标记与搜索历史存储、结果表行委托，以及相关的常量表。
#include "config_crypto.h"   // MarkStore 的屏蔽/标记与 config.ini 其余敏感项共用同一套加解密
#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QList>
#include <QListWidget>
#include <QSettings>
#include <QTableWidget>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QApplication>
#include <QFontMetrics>
#include <QMap>
#include <QPair>
#include <functional>
#include <vector>
#include <string>
#include <tuple>
#include <cstdint>

#include "common.h"
#include "theme.h"
#include "animations.h"
#include "widgets.h"
#include "search_engine.h"   // SheetData

// ---- 常量与别名 ----
using RowKey = std::tuple<std::string, std::string, int>;

static const int    kMinResultCols = 3;

static const int    kMaxResultCols = 7;

static const char*  kMetaSheetCol  = "工作表";

static const char*  kMetaRowCol    = "行号";

static const double kColFuzzyMin   = 60.0;   // 与核心 fuzzySearch 的阈值保持一致

static const int kHistStoreMax     = 20;   // 存储上限（沿用原版 kHistoryMax）

static const int kHistShowMax      = 10;   // 下拉最多显示条数

static const int kHistTtlMinMinute = 10;   // 时效最短：10 分钟

static const size_t kFuzzySupplementMaxExact = 5000;

struct Doc { std::string fn; std::vector<SheetData> sheets; };

struct DiskFile { std::string fn, path; int64_t mtime = 0, size = 0; };

struct CardDef { QString title; bool expand; std::function<QWidget*()> make; };

struct PageDef { QString title; std::function<QWidget*()> make; };

struct SecDef  { QString title; std::function<QWidget*()> make; };

struct HistItem { QString kw; qint64 ts; };

static const QColor kMarkColors[5] = {
    QColor("#E5484D"), QColor("#8B5CF6"), QColor("#326CF3"), QColor("#10B981"), QColor("#F59E0B")
};

struct MarkStore {
    std::set<RowKey>      blockedEntries;   // 条目级屏蔽
    std::set<std::string> blockedFiles;     // 文件级屏蔽
    std::map<RowKey, int> marked;           // 标记色索引 0红 1紫 2蓝 3绿 4黄（沿用原版顺序）

    bool isBlocked(const SearchResult& r) const {
        return blockedFiles.count(r.filename) > 0 ||
               blockedEntries.count({ r.filename, r.sheetName, r.row }) > 0;
    }
    int colorOf(const SearchResult& r) const {
        auto it = marked.find({ r.filename, r.sheetName, r.row });
        return it == marked.end() ? -1 : it->second;
    }
    size_t filterBlocked(std::vector<SearchResult>& v) const {   // 等价原版 FilterBlocked
        size_t before = v.size();
        v.erase(std::remove_if(v.begin(), v.end(),
            [this](const SearchResult& r) { return isBlocked(r); }), v.end());
        return before - v.size();
    }
    bool empty() const { return blockedEntries.empty() && blockedFiles.empty() && marked.empty(); }
    void clearAll() { blockedEntries.clear(); blockedFiles.clear(); marked.clear(); }

    int lastLoadFailures = 0;   // 上次 load() 里"是密文却解不开"的项数（汇总进 --report 的 cfgEncFail=）

    // 屏蔽/标记也是敏感内容（文件名/表名/行号），与 config.ini 其余敏感项走同一套 config_crypto：
    //   读：解密失败 → 计数并跳过该项（绝不静默清空，也不把密文当数据用）；
    //   写：主密钥不可用时**整体跳过**，保留原密文不被覆盖。
    static QString decValue(const QString& raw, int* failCount) {
        std::string out; bool wasProtected = false;
        if (cfgcrypto::Unprotect(raw.toUtf8().toStdString(), &out, &wasProtected)) {
            return QString::fromUtf8(out.c_str());
        }
        if (wasProtected && failCount) ++(*failCount);
        return QString();
    }
    static void encValue(QSettings& s, const char* key, const QString& plain) {
        if (!cfgcrypto::KeyReady()) return;   // 密钥不可用：不写，保留原值
        const std::string e = cfgcrypto::Protect(plain.toUtf8().toStdString());
        if (!e.empty()) s.setValue(QString::fromLatin1(key), QString::fromUtf8(e.c_str()));
    }

    void load(QSettings& s) {
        clearAll();
        lastLoadFailures = 0;
        auto splitLines = [](const QString& text) {
            std::vector<QString> out;
            for (const QString& ln : text.split('\n', Qt::SkipEmptyParts)) { QString t = ln.trimmed(); if (!t.isEmpty()) out.push_back(t); }
            return out;
        };
        for (const QString& f : splitLines(decValue(s.value("block/files").toString(), &lastLoadFailures))) blockedFiles.insert(f.toUtf8().toStdString());
        for (const QString& ln : splitLines(decValue(s.value("block/entries").toString(), &lastLoadFailures))) {
            auto p = ln.split('|');   // fn|sheet|row
            if (p.size() == 3) blockedEntries.insert({ p[0].toUtf8().toStdString(), p[1].toUtf8().toStdString(), p[2].toInt() });
        }
        for (const QString& ln : splitLines(decValue(s.value("mark/entries").toString(), &lastLoadFailures))) {
            auto p = ln.split('|');   // fn|sheet|row|color
            if (p.size() == 4) marked[{ p[0].toUtf8().toStdString(), p[1].toUtf8().toStdString(), p[2].toInt() }] = p[3].toInt();
        }
    }
    void save(QSettings& s) const {
        QStringList bf, be, me;
        for (const auto& f : blockedFiles) bf << QString::fromUtf8(f.c_str());
        for (const auto& [fn, sn, r] : blockedEntries) be << QString::fromUtf8((fn + "|" + sn + "|" + std::to_string(r)).c_str());
        for (const auto& [k, c] : marked) {
            const auto& [fn, sn, r] = k;
            me << QString::fromUtf8((fn + "|" + sn + "|" + std::to_string(r) + "|" + std::to_string(c)).c_str());
        }
        encValue(s, "block/files", bf.join('\n'));
        encValue(s, "block/entries", be.join('\n'));
        encValue(s, "mark/entries", me.join('\n'));
    }
};

class RowDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    RowDelegate(QAbstractItemView* view, const Proto& p)
        : QStyledItemDelegate(view), m_view(view), m_hover(p.hover), m_card(p.card), m_altRow(p.altRow), m_accent(p.accent) {
        view->setMouseTracking(true);   // cellEntered/itemEntered 需要鼠标跟踪（只增加 hover 事件，不影响选择行为）
        view->viewport()->installEventFilter(this);
        if (auto* t = qobject_cast<QTableWidget*>(view)) {
            QObject::connect(t, &QTableWidget::cellEntered, this, [this](int, int row) { hoverRow(row); });
        } else if (auto* l = qobject_cast<QListWidget*>(view)) {
            QObject::connect(l, &QListWidget::itemEntered, this, [this, l](QListWidgetItem* it) { hoverRow(it ? l->row(it) : -1); });
        }
        // B3 选中态交接：选中项从 A 换到 B 时，让 A 的强调色"滑走淡出"，而不是凭空消失（空间连续性）
        if (view->selectionModel()) {
            QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
                             [this](const QModelIndex& cur, const QModelIndex& prev) { handoff(prev.row(), cur.row()); });
        }
    }
    void setTheme(const Proto& p) {
        m_hover = p.hover; m_card = p.card; m_altRow = p.altRow; m_accent = p.accent;
        if (m_view) m_view->viewport()->update();
    }
    void setRounded(bool r) { m_rounded = r; }   // 列表项有 8px 圆角，表格行没有
    void setMarkBar(bool on) { m_markBar = on; }  // 只有结果表需要首列标记色条
    bool isTableView() const { return m_markBar; }  // 结果表（=当前唯一需要入场错峰的视图）
    // 自检/截图用：直接把行 hover 推到指定行（委托自绘的 hover 没有真实鼠标事件可用）
    void setHoverRow(int row) { hoverRow(row); }
    // B1 结果表入场错峰：一次搜索/筛选后，首屏若干行的文字自上而下依次"洗"进来。
    // 关键设计：**单动画 + 每行相位** —— progress(row) = curve(clamp((pv*total - row*step)/dur, 0, 1))，
    //   是 pv 的确定性函数（同一帧必然渲染同一结果），不用墙上时钟、也不为每行起定时器。
    //   超出 rows 的行直接算 1（立刻可见），避免大结果集里"越往下越慢"。
    void startReveal(int rows) {
        rows = qMin(rows, kRowStaggerMax);
        const int dur = animMs(kDurSlow);
        if (rows <= 0 || dur <= 0) {                       // 关闭动效 / 空结果：直接落终态
            m_revealDone = true;
            m_revealRows = 0;
            if (m_view) m_view->viewport()->update();
            return;
        }
        m_revealRows = rows;
        m_revealTotal = dur + (rows - 1) * kRowStaggerMs;
        m_revealDone = false;
        if (!m_revealAnim) {
            m_revealAnim = new QVariantAnimation(this);
            m_revealAnim->setEasingCurve(QEasingCurve::Linear);   // 节奏由每行相位公式给，不再叠曲线
            QObject::connect(m_revealAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
                m_revealPv = v.toDouble();
                if (m_view) m_view->viewport()->update();
            });
            QObject::connect(m_revealAnim, &QVariantAnimation::finished, this, [this] {
                m_revealDone = true; m_revealRows = 0;
                if (m_view) m_view->viewport()->update();
            });
        }
        m_revealAnim->stop();
        m_revealAnim->setDuration(m_revealTotal);
        m_revealAnim->setStartValue(0.0);
        m_revealAnim->setEndValue(1.0);
        m_revealAnim->start();
    }
protected:
    bool eventFilter(QObject* o, QEvent* e) override {
        if (o == (m_view ? m_view->viewport() : nullptr) && e->type() == QEvent::Leave) hoverRow(-1);
        return QStyledItemDelegate::eventFilter(o, e);
    }
    // 说明：hover 底色**必须自绘**，不能靠 QStyleOptionViewItem::backgroundBrush 交给基类铺 ——
    //   本表装了 QSS（`QTableWidget::item { padding:6px; border:none }`），QStyleSheetStyle 会自己
    //   接管 ::item 的绘制，backgroundBrush 被丢掉（实测：设了也一像素不变）。
    //   自绘位置放在基类之前：基类随后在它上面画文字，所以文字既不被盖住也不被染色。
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        const bool hovered = (idx.row() == m_row && m_hv.v > 0.001);
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        // B3 选中态交接：把"上一项"的强调色底滑走 + 淡出（画在基类之前，文字仍由基类画在最上层）
        if (m_rounded && idx.row() == m_handRow && m_handP < 0.999) {
            QColor c = m_accent;
            c.setAlphaF(1.0 - m_handP);
            QRectF r = QRectF(o.rect).adjusted(1, 1, -1, -1);
            r.translate(0.0, m_handP * 0.4 * r.height());
            p->save();
            p->setRenderHint(QPainter::Antialiasing, true);
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            p->drawRoundedRect(r, 8, 8);
            p->restore();
        }
        if (hovered && !(o.state & QStyle::State_Selected)) {
            QColor c = m_hover;
            c.setAlphaF(c.alphaF() * m_hv.v);   // 令牌色 × 过渡进度
            p->save();
            p->setRenderHint(QPainter::Antialiasing, true);
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            if (m_rounded) p->drawRoundedRect(QRectF(o.rect).adjusted(1, 1, -1, -1), 8, 8);   // 与 QSS item 的 8px 圆角一致
            else           p->fillRect(o.rect, c);                                            // 表格行：铺满整行
            p->restore();
        }
        QStyledItemDelegate::paint(p, o, idx);   // 背景与文字仍走默认绘制（QSS 主题、选中优先级不变）
        // B1 入场错峰：用"该行底色"以 (1-进度) 的透明度盖一层，把文字"洗"进来。
        //   为什么用盖而不是改 palette/直接画字：装了 QSS 的表格里，item 的文字色由 QStyleSheetStyle
        //   自己配置 palette，改 option.palette 不一定生效；盖一层底色是 QSS 无关的确定性做法。
        if (!m_revealDone && !(o.state & QStyle::State_Selected)) {
            const qreal rp = rowReveal(idx.row());
            if (rp < 0.999) {
                QColor c = (idx.row() % 2) ? m_altRow : m_card;
                c.setAlphaF(1.0 - rp);
                p->fillRect(o.rect, c);
            }
        }
        // 标记色条只属于结果表首列：列表没有标记这回事，且列表项的 Qt::UserRole 是空的，
        // toInt() 会给出 0（=红色）→ 不判断视图类型就会在每个列表项左边画一条红杠。
        if (!m_markBar || idx.column() != 0) return;
        int ci = idx.data(Qt::UserRole).toInt();
        if (ci < 0 || ci >= 5) return;
        const QRect r = opt.rect;
        QRect bar(r.left() + 1, r.top() + 4, 3, std::max(4, r.height() - 8));
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(Qt::NoPen);
        p->setBrush(kMarkColors[ci]);
        p->drawRoundedRect(bar, 1.5, 1.5);
        p->restore();
    }
private:
    void hoverRow(int row) {
        if (row == m_row) return;
        m_row = row;
        m_hv.to(this, row >= 0, [this](qreal) { if (m_view) m_view->viewport()->update(); });
    }
    // 每行在入场时间线上的进度（pv 的确定性函数）
    qreal rowReveal(int row) const {
        if (m_revealDone || row < 0 || row >= m_revealRows) return 1.0;
        const qreal t = m_revealPv * m_revealTotal;                                  // 当前时间线位置（ms）
        const qreal local = qBound(0.0, (t - row * kRowStaggerMs) / (qreal)animMs(kDurSlow), 1.0);
        return curveStandard().valueForProgress(local);
    }
    // B3：上一选中项 → 新选中项的交接触发（只在列表上用；表格选中是整行强调色，不需要这个提示）
    void handoff(int prevRow, int newRow) {
        if (!m_rounded || prevRow < 0 || prevRow == newRow) return;
        const int dur = animMs(140);
        if (dur <= 0) return;
        m_handRow = prevRow;
        m_handP = 0.0;
        if (!m_handAnim) {
            m_handAnim = new QVariantAnimation(this);
            m_handAnim->setEasingCurve(curveExit());
            QObject::connect(m_handAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
                m_handP = v.toDouble();
                if (m_view) m_view->viewport()->update();
            });
            QObject::connect(m_handAnim, &QVariantAnimation::finished, this, [this] {
                m_handRow = -1;
                if (m_view) m_view->viewport()->update();
            });
        }
        m_handAnim->stop();
        m_handAnim->setDuration(dur);
        m_handAnim->setStartValue(0.0);
        m_handAnim->setEndValue(1.0);
        m_handAnim->start();
    }
    QAbstractItemView* m_view = nullptr;
    QColor m_hover, m_card, m_altRow, m_accent;
    int m_row = -1;
    bool m_rounded = false;   // 列表 = true（圆角自绘）；表格 = false（交给基类铺满整行）
    bool m_markBar = false;   // 是否绘制结果表首列的标记色条（列表恒为 false）
    HoverT m_hv;
    // B1 入场错峰
    QVariantAnimation* m_revealAnim = nullptr;
    qreal m_revealPv = 0.0;
    qreal m_revealTotal = 0.0;
    int m_revealRows = 0;
    bool m_revealDone = true;   // 默认"已经完成" → 没有动效时不做任何额外绘制
    // B3 选中态交接
    QVariantAnimation* m_handAnim = nullptr;
    qreal m_handP = 1.0;
    int m_handRow = -1;
};

// ---- 函数声明 ----
QString markColorName(int i);
int markColorIndexByName(const std::string& name);
std::vector<std::string> defaultExtraCols();
void enableRowHoverAnim(QListWidget* list, const Proto& p, std::vector<RowDelegate*>* reg);

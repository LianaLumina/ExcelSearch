#pragma once

// 应用主窗口 AppWindow：由 main.cpp 原样搬出（Stage B1 纯搬移，逻辑一字未改）。
// 说明：本文件当前仍是「类内内联定义」形态；后续 B2..B11 逐步把各职能区的方法体
//       移到对应 app_window_*.cpp，并在此处只保留声明。

// —— 只保留「类声明本身需要」的头；其余（重头模板库、只在实现里用到的 Qt 头）已下沉到各 .cpp ——
// B12 的目的：不再让每个 .cpp 都被 rapidfuzz / 各 reader / dialogs / widgets / manual_window 拖住。
#include <QWidget>            // 基类
#include <QString>
#include <QStringList>        // allHeaderNames() 按值返回
#include <QColor>             // m_accent 等按值成员
#include <QIcon>              // appIconIsResource() 内联体
#include <QColorDialog>       // pickCustomAccent() 内联体
#include <QToolTip>           // showHitTip() 内联体
#include <QCursor>            // showHitTip() 内联体（QCursor::pos）
#include <QSystemTrayIcon>    // trayActive() 内联体（isVisible）
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include "common.h"           // T()
#include "search_engine.h"    // SearchEngine / SearchResult 按值成员
#include "data_model.h"       // MarkStore / HistItem / CardDef / PageDef / SecDef / RowKey 按值成员
#include "loading.h"          // 头文件内联体用到 LoadWorker/ProbeWorker 与 readInventory 等（.cpp 各自按需再 include）
#include "manual_window.h"    // demoManualPixmap/manualNavReport 的返回类型与 manualText/manualHash（内联体用到）
#include "dialogs.h"          // CloseDialog/PasswordDialog：内联体里有按值构造（demoCloseDialogPixmap 等在 .cpp）
#include <QLineEdit>          // demoSearch() 内联体
#include <QCheckBox>          // 通用设置内联体
#include <QComboBox>          // 智能列/历史设置内联体
#include <QRadioButton>       // 共享/筛选模式内联体
#include <QStackedWidget>     // 页面栈内联体
#include <QMenu>              // 托盘菜单内联体
#include <QButtonGroup>       // 单选组内联体
#include <QDateTime>          // 内联体里的时间处理
#include <QFile>
#include <QDir>

// 以下类型只以「指针 / 引用」出现在类声明里 → 前向声明即可，不必拉进它们的完整头文件（这就是提速的关键）
class QLabel; class QLineEdit; class QPushButton; class QCheckBox; class QFrame;
class QScrollArea; class QSpinBox; class QButtonGroup; class QRadioButton; class QComboBox;
class QListWidget; class QTableWidget; class QStackedWidget; class QPlainTextEdit;
class QMenu; class QTimer; class QVariantAnimation; class QGraphicsOpacityEffect;
class QAbstractItemView; class QAction; class QScrollBar; class QPixmap;
class QCloseEvent; class QMouseEvent; class QResizeEvent;
class QListWidgetItem; class QTableWidgetItem;
class ManualDialog; class WinBtn; class FolderBtn; class CardHeader; class LoadBar;
class CollapseCard; class StateTint; class LoadWorker; class ProbeWorker;

// ============================================================================
// 控件动效总纲（V0.3.2 新增）
// ----------------------------------------------------------------------------
// 设计原则（改这段之前先读，改完再回去读 docs/动效设计.md）：
//   1) **只作用在子控件上**。顶级无边框窗口开了 WA_TranslucentBackground，Win10 下做
//      尺寸/位置动画会闪（历史已确认，用户选择"质感优先、接受静止"），所以窗口本体永远静止，
//      所有过渡都发生在窗口内部的控件上。
//   2) **颜色一律取自 Proto 令牌**（hover/pressed/text/sub/accent/closeHover…），不硬编码；
//      深浅色 + 5 种强调色下都要成立。
//   3) 克制：时长 90–240ms，曲线分三类（进入用 emphasis-decel、退出用 accel、强调用轻回弹），
//      幅度 ≤ 10px；只表达"层级 / 因果 / 状态"，不做装饰性动作，不给每个控件发明一套新效果。
//   4) **必须能一键关闭**：环境变量 `UI_PROTO_NO_ANIM=1` 或 CLI `--no-anim`。
//      关闭后 animMs() 一律返回 0 → 动效"直接落终态"（不装 effect、不起动画），
//      因此离屏截图与自检可复现、与动效前逐像素一致。
// ★回迁注意：动效与 makeProto()/qssFor() 的令牌强绑定，回迁时要么整段搬走、要么整段不搬，
//   不要只搬一半（只搬画面不搬令牌会出现"深浅色下颜色对不上"）。详见 docs/回迁标注.md 第 19/20 条。
// ============================================================================

// —— 动效令牌：时长档位（所有效果只许用这三档，层次感来自档位差而不是随手取值）——
// —— 错峰（stagger）：现代感的来源——不是所有东西同时动，而是"容器先到、内容随后"——
// —— 各效果时长（都由上面的档位表达）——

// —— 曲线库：三类曲线一处定义，全项目共用（改这里 = 全局动效性格一起变）——
// 用 cubic-bezier / 自定义弹簧表达，零依赖（只用到 QEasingCurve 自带能力）。
// enter（EmphasizedDecel，Material 3 的"进场"曲线）：起步快、尾巴长 → 比 OutCubic 更"利落又不生硬"
// standard（Standard，位移/颜色通用）：两端都平滑，适合"从 A 状态到 B 状态"
// exit（EmphasizedAccel）：起步慢、尾巴快 → 退出动作"化开走掉"，不拖泥带水
// count（数值滚动专用，OutExpo）：起步极快、尾巴很长 —— 数字用它比位移曲线更有"滚上去"的观感；
// 它不表达物理位移，所以不跟位移类共用曲线。
static inline QEasingCurve curveCount() { return QEasingCurve(QEasingCurve::OutExpo); }
// spring（二阶阻尼，ζ=0.7 / ω=16）：约 4.6% 过冲、峰值在过程 30% 处 → 有生命力但不夸张。
// 只用在**位置/旋转**这类能表达"过冲"的属性上；颜色一律不用弹簧（颜色过冲会看成闪）。

// 令牌色线性插值（含 alpha）：自绘控件与"QSS 覆盖"两条路径共用它，
// 保证动画两端点颜色一定来自 Proto，不会出现中间态硬编码色。
// 半透明令牌色写进 QSS 必须用 rgba()：QColor::name() 会丢掉 alpha。
// 注：hover 过渡是**全程不透明**的（见 backDropOf），这个函数留给以后需要半透明底色的场景。
[[maybe_unused]] static inline QString rgbaCss(const QColor& c) {
    return QString("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
}

// ============================================================================
// 自绘动效效果：淡入 + 位移（正负都行）
// ----------------------------------------------------------------------------
// 为什么不用"动画布局上边距"（第一版的做法）：那会**每帧触发整棵子树的 relayout**
//   ——页面里挂着结果表，每帧重排一次既费又会引起轻微抖动，而且只能往一个方向位移。
// 这里改成在 effect 内部平移渲染结果：布局完全不动、方向可正可负、也没有边距累加的坑。
// 用法：动画期间挂上，结束立刻摘掉（长期挂着会改渲染路径、伤性能）。
// 关闭动效时根本不安装 —— 渲染路径与动效前逐像素一致。
// ============================================================================

// 子控件进入动效：淡入 + 轻微位移（curveEnter）。
//   dyPx  ：位移幅度（正 = 从下方滑入；负 = 从上方滑入；0 = 只淡入）
//   child ：可选"随后到"的子控件 —— 容器先到约 30%，内容再淡入（层级因果，现代感的来源）
// 关闭动效（dur<=0）时**什么都不做**：终态本就"没有位移、没有 effect"。

// 一次性抖动（用于"密码错误"这类明确的失败反馈）：横向阻尼振荡后归位，不碰布局。
// 挂在控件的 FadeSlideEffect 上做，因此和淡入/位移共用同一套机制。

// 自绘控件的 hover 进度驱动：0 ↔ 1 的 QVariantAnimation（默认 standard 曲线），每帧回调 apply(v)。
// 关闭动效时直接落终态（0 或 1），因此在事件循环里不留任何定时器。
// curve 可换：位置/旋转类属性用 curveSpring()，颜色类一律用 curveStandard()（颜色过冲会被看成"闪"）。




// 数据目录里所有可加载文件的磁盘信息（文件名/路径/mtime原始计数/size）

// 缓存清单文件（cache.inv）：记录写入时刻数据目录「全量」文件(fn,mtime,size)，作为缓存是否可复用的唯一凭据。
// 只在清单逐项精确相等（数量/文件名/mtime/size 全部一致）时才允许复用索引，绝不因 .xse 跳过态而放宽。

// 解密 .xse -> SheetData 列表（worker 与主线程共用；pwdEnabled 输出该文件是否启用附加密码）



// 共享目录连通性探测：UNC 的 exists() 可能阻塞数秒到数十秒，必须放在 worker 线程，
// 否则界面会像卡死。结果通过 finished 回主线程，自动串行"测试 → 成功才强制重载"。


// 按钮"实际压在什么颜色上"（panelBg 或卡片 card）。
// 为什么需要它：透明底按钮（#themeBtn / #tbBtn / #navBtn 的常态）做 hover 过渡时，如果往 QSS 里写
// **半透明**背景，Qt 会把它合成到一个不由我们控制的中介底色上 —— 实测中间帧偏成深灰（#BBBBBF），
// 也就是 hover 会"闪一下"。所以这里取真实底色，把「透明 → hover 色」做成**两个不透明色之间的插值**，
// 全程 alpha=1，颜色走向可预测、可截图核对。
// ★回迁注意：按钮若挪到别的容器（不是 #panel / #card），这里要跟着补容器判定。
// 取出容器里"随后到"的内容控件（cardFrame 建卡片时登记在 animChild 属性上）。
// 用于父子错峰：容器先到、内容延迟 kStaggerMs 再淡入 —— 层级因果比"一起淡入"清楚得多。


// 自绘文件夹图标按钮（替换原来的「导出 xlsx」文字按钮）：
// 现代极简风格 —— 只描边、不上色，颜色跟随主题文字色（hover 变亮 + 淡底色）。
// 动效：hover 底色与描边色都由 HoverT 驱动做过渡（令牌色插值），按下仍是即时反馈。

// 标题栏三键（最小化/最大化/关闭）：hover 底色与前景色走过渡；关闭键的红色取自 Proto.closeHover
// 令牌（此前是硬编码 #E5484D，深色下与令牌不一致 —— 顺手对齐，取消 hover 后无任何视觉差异）。

// ============================================================================
// QSS 控件的 hover 颜色过渡（按钮）
// ----------------------------------------------------------------------------
// 为什么这么做：QSS 自身不支持 transition，hover 只能瞬间换色。这里在 hover 期间往**控件自身的
// 样式表**挂一条与 qssFor() 同名（必要时加 `:hover`）的选择器规则来覆盖 background/color，
// 并用 QVariantAnimation 在两端令牌色之间插值。
// 四条硬约束（改选择器前必读）：
//   ① 选择器的"主体"必须与 qssFor() 里那条**一致** —— `#themeBtn` 在 qssFor 里没有专属规则、
//      只有通用 `QPushButton:hover`，所以它的选择器要用 `QPushButton`；否则优先级压不住，动画不可见；
//   ② 离开（或动画结束）立刻清空控件级样式表 → 回到全局 QSS，换主题/换强调色不受影响；
//   ③ 勾选态（顶部标签当前页）不参与过渡，`:checked` 的强调色是状态表达，不能被 hover 冲掉；
//   ④ UI_PROTO_NO_ANIM=1 时**根本不安装** —— hover 与动效前逐像素一致。
// 不在覆盖范围内的（见 docs/动效设计.md「刻意不做」）：checkable 的 #themeToggle（选中态是强调色底）、
// 强调色色块（自带样式表）、QMenu 菜单项（QMenu 的自绘路径插不进过渡，菜单寿命极短，瞬时高亮更跟手）。
// ============================================================================

// 状态过渡（QSS 控件通用）：hover / 按下 / 焦点 三种状态的**颜色与圆角**过渡。
// ----------------------------------------------------------------------------
// 为什么这么做：QSS 没有 transition。这里在状态期间往**控件自身的样式表**挂一条与 qssFor() 同名
// （必要时加 `:hover`）的选择器规则来覆盖 background/color/border-color/border-radius，并用
// QVariantAnimation 在两端令牌色之间插值；状态结束立刻清空控件级样式表 → 回到全局 QSS。
// 硬约束（改选择器/加控件前必读）：
//   ① 选择器主体必须与 qssFor() 里那条一致 —— `#themeBtn` 在 qssFor 里没有专属规则、只吃通用
//      `QPushButton:hover`，所以它的选择器就用 `QPushButton`；否则优先级压不住，动画不可见；
//   ② 颜色**全程不透明**：透明底按钮的"透明"用 backDropOf() 取真实底色代替（半透明背景在 QSS 里
//      会被合成到不受控的中介底色上，实测中间帧偏成深灰，hover 会"闪一下"）；
//   ③ 勾选态（顶部标签"当前页"）不参与过渡：`:checked` 的强调色是状态表达，不能被 hover 冲掉；
//   ④ UI_PROTO_NO_ANIM=1 时**根本不安装** —— 与动效前逐像素一致。
// 颜色用 curveStandard 进出（颜色过冲会被看成"闪"）；位移/旋转类才用 curveSpring()。
// ============================================================================

// 给一个 QSS 按钮挂状态过渡（kind 决定两端令牌色与圆角，选择器由调用方给全）。
// proto 每帧重新求值 → 换主题 / 换强调色 / 状态中切主题都跟着变。
// 注意所有端点色都**不透明**（透明底用 backDropOf 取真实底色代替），理由见 backDropOf。
// ============================================================================
// 可折叠卡片 + 卡片标题栏（搜索页「数据概览」/「搜索结果」）
// ----------------------------------------------------------------------------
// 交互：点标题栏展开/收起，内容体做**高度动画**（180ms OutCubic），标题栏箭头随进度旋转 90°。
// 为什么标题仍用 QLabel#cardTitle：QSS 的字体/颜色/行高原样复用，布局高度与动效前逐像素一致
//   （自己 drawText 就得复刻字体，稍不留神整体位移 1–2px）。
// 折叠状态**不持久化**：不新增配置键 → 配置格式与回迁面完全不变（见 docs/回迁标注.md 第 19 条）。
// ============================================================================

// ============================================================================
// B4 加载进度条（不确定进度）
// ----------------------------------------------------------------------------
// 只表达"后台在干活"这一因果：一条 2px 强调色细条带渐变尾，从面板顶部滑过。
// **不假装知道百分比** —— 真实进度（x/y 个文件）仍在状态栏文字里。
// 叠加在面板顶部、不进布局 → 不改变任何控件的位置（进布局会让整页在加载时上下跳）。
// 关闭动效时**完全不出现**：它本身就是一个运动提示（功能性进度由状态栏文字承担），
// 而且这样截图无论何时都拍不到它。
// ============================================================================


// 附加密码输入对话框（MAA 风格，与主界面主题统一：深/浅色 + 圆角 + 主色按钮）

// 行详情对话框（MAA 风格，与主界面主题统一：深/浅色 + 圆角 + 主色按钮）

// 确认对话框（MAA 风格，替代原生 QMessageBox —— 原生外观与无边框窗口冲突）

// 关闭方式选择对话框（MAA 风格）：点 × 时让用户选「直接关闭 / 最小化到托盘」，
// 左下角可选「不再询问」——勾选后按本次选择记住，不再弹。

// ============================================================================
// 使用说明书窗口（版式与交互对齐 MAA 的「公告」框；实现为本项目自有代码）
// ----------------------------------------------------------------------------
// 版式：左「章节列表」+ 右「可滚动正文」；左下为**装饰位（默认留白，预留图片接口）**；
//       右下「☐ 下次说明书更新前不再显示」+「确认」。
// 交互（与 MAA 一致）：
//   · 启动时若「当前说明书内容 ≠ 上次已关闭的版本」且未永久关闭 → 自动弹出；
//   · **必须滚动到底**才能关闭；未读到底时点确认会依次出现几句调侃，连续点 20 次以上才放行；
//   · 勾「下次说明书更新前不再显示」→ 记住当前内容版本，**内容更新后自动恢复提示**；
//   · 「设置 → 通用设置」另有永久开关（对应 MAA 的「不显示公告」）。
// 内容来源：exe 同目录的 `MANUAL.md`（可外部替换，无需重编译）→ 缺省用内嵌副本 :/manual.md
// 装饰图片接口：exe 同目录放 `说明书插图.png` 即自动显示（未放则留白）。
// ============================================================================
// 按 `### 标题` 分节；首项固定为「全部内容」（与 MAA 公告的 ALL 项一致）
// 体检：哪些章节在真实控件里会需要横向滚动（= 内容比窗口宽 → 会被裁掉，必须改文案）
// 使用说明书窗口（版式与交互对齐 MAA 的「公告」框）
// ----------------------------------------------------------------------------
// 独立顶层窗口（非模态）：可与其他窗口并用；右上角是**最大化/还原**（不是关闭），
// 无边框窗口自己支持「拖动标题区移动 + 双击标题区最大化」；关闭走「确认」按钮。
// 版式：左「章节导航」（禁止横向滚动条，标题已缩短）+ 右 Markdown 正文 +
//       左下装饰位（默认留白；exe 旁放 `说明书插图.png` 即自动显示）+ 右下「☐ 下次更新前不再展示」+「确认」。
// 交互（与 MAA 一致）：必须**滚动到底**才能关；未读完点确认依次出现调侃文案，连续点 20 次以上放行；
//       勾「下次更新前不再展示」→ 记住当前内容版本，内容更新后自动恢复提示（Alt+F4 同样受门禁约束）。
// 内容来源：exe 旁 `MANUAL.md`（可外部替换）→ 内嵌副本 :/manual.md 兜底。


// ============================================================================
// 屏蔽 / 标记存储（V0.3.2）
// 身份键与 V0.3.0 完全一致 —— (文件名, 工作表, 行号)：便于回迁与数据互导。
// 持久化文本格式亦沿用原版：block/entries = "fn|sheet|row"，mark/entries = "fn|sheet|row|color"。
// 注意：屏蔽/标记属 UI 层概念，不进索引、不进 cache.dat，因此不影响缓存命中判定。
// ============================================================================

// 管理门禁：普通管理密码可修改；超管密码硬编码、不可修改，供开发者维护时直达
// ★敏感口令：**构建期注入**（仓库内不保存真值）
//   真值放在本机 local_secrets.cmake（已被 .gitignore 忽略），由 CMakeLists.txt 注入为编译宏。
//   未注入时的行为：超管通道**关闭**（不匹配任何输入）；默认管理密码退回弱默认值。
#ifndef ES_SUPER_PASSWORD
#define ES_SUPER_PASSWORD ""
#endif
#ifndef ES_DEFAULT_ADMIN_PASSWORD
#define ES_DEFAULT_ADMIN_PASSWORD "excelsearch"
#endif
static const char* kSuperPassword = ES_SUPER_PASSWORD;
static const char* kDefaultAdminPassword = ES_DEFAULT_ADMIN_PASSWORD;

// 标记 5 色（索引沿用原版顺序：0红 1紫 2蓝 3绿 4黄）。
// 呈现为「行首色条」而非原版的整行文字变色 —— 深色主题下更干净，也不会和斑马纹/选中蓝打架。
// 由色名反查色索引（原版 GetColorIndexByName），供「已标记红色」这类关键词使用

// ============================================================================
// 可配置智能列：结果表总列数 3..7。
// 固定三列（不可改动）：序号（首）、文件名（次）、匹配内容（末）；
// 中间为用户自定义列，来源可以是内建元数据（工作表 / 行号），也可以是任意工作簿表头关键词。
// 表头关键词按「表头包含 + 精确匹配加权」选列；都不命中时再用 rapidfuzz 兜底，
// 兜底分数仍低于阈值 → 视为「列不存在」。
// ============================================================================

// ★回迁注意：原版结果列表是固定 7 列（含硬编码的 核定工日/工时代码）；此处改为可配置 3–7 列，
//   默认只放内建元数据（工作表/行号），避免数据里没有该表头时出现空列。详见 docs/回迁标注.md 第 5 条。
// 缺省布局：只放「内建元数据」两列（工作表 / 行号），任何数据下都必然有效。
// 注意不要把「核定工日」「工时代码」之类硬编码进默认值 —— 数据里没这一列时默认就会出现两个空列；
// 这类智能列交给用户在「智能列设置」里按实际表头配置。

// ============================================================================
// 搜索历史：只有「一级搜索」记录历史，二级筛选一律不记录（避免同一关键词反复入栈）。
// 存储上限 20 条；下拉最多显示 10 条，其余到「搜索历史」设置页查看/回填。
// 时效 = 条目存活多久后删除：最短 10 分钟，最长「关闭程序后删除」（该档完全不落盘）。
// ============================================================================
// 模糊补充的护栏：精确命中已达此数量时不再做模糊补充（见 doSearch 里的性能修复说明）
// 项目开源许可证名称（发行时在此一处填写，界面与文档共用；留空则界面显示"（待定）"）
// 例：return QStringLiteral("MIT");
static QString projectLicense() { return QStringLiteral("GPL-3.0"); }


// ============================================================================
// 结果表 / 设置列表的行委托（定义在 MarkStore 之后：要复用标记 5 色 kMarkColors）
// ----------------------------------------------------------------------------
// 职责一：行 hover 的颜色过渡（QSS 不支持 transition，只有自绘才能做过渡）。
//   颜色取 Proto.hover 令牌；进度由 HoverT 驱动（关闭动效时直接落终态）。
// 职责二：结果表首列的标记色条（原 MarkBarDelegate 的职责，绘制完全不变；
//   docs/回迁标注.md 第 7 条提到的 MarkBarDelegate = 本类）。
// 列表与表格的差异：列表项有 8px 圆角，故先自绘圆角底再交给基类画文字；
//   表格有斑马纹（基类会用 backgroundBrush 铺满整行），改成设置 backgroundBrush 由基类铺。
// ============================================================================

// 给一个列表装上行 hover 过渡（沿用 MarkBarDelegate 的用法：只补画，不改默认绘制）
// 注：原 `MarkBarDelegate` 的"行首标记色条"职责已并入上面的 RowDelegate（绘制逻辑一字未改），
//     保留这段指向是为了让回迁时能对上 docs/回迁标注.md 第 7 条里提到的类名。

// ============================================================================
// 【预留接口】实用工具标签页 —— 当前不启用，只保留注册通道
// ----------------------------------------------------------------------------
// 分类规则（回迁/后续开发都按这个判，别再另起一套）：
//   1) 顶部标签 = 主流程页：搜索 / 设置 /（预留）实用工具；
//   2) 「实用工具」放：使用频率明显低于主流程（搜索·筛查·查看·导出·屏蔽·标记）
//      的辅助功能，例如统计报表(已决定不迁移，若复活则放这里)、批量转换、数据体检等；
//   3) 需要密码的管理类内容仍然只放「设置 → 高级设置」，不要混进实用工具。
// 启用方式（将来只需两步，不要新建第二套标签注册逻辑）：
//   a) 把下面的 kEnableUtilTab 置为 true；
//   b) 在构造函数里对每个工具调用一次 registerUtilTool(标题, 工厂)。
//      makeUtilPage() 会自动生成"左列表 + 右内容栈"，无需改布局代码。
// ★回迁注意：若不搬这个标签，至少要把"分类规则"搬进说明书，避免以后把低频功能随手塞回
//   主界面或设置页；若搬，则连注册方式一起搬。详见 docs/回迁标注.md 第 10 条。
// ============================================================================
static const bool kEnableUtilTab = false;          // ← 预留开关：当前明确不启用
static QString utilTabTitle() { return T("实用工具"); }

// 行悬停动效：需要 RowDelegate 的完整定义，故留在本文件（跟随 RowDelegate 一起拆分）

class AppWindow : public QWidget {
    bool m_dark = false;
    QColor m_accent = QColor("#326cf3");
    bool m_maxed = false;
    QPushButton *m_themeBtn = nullptr, *m_lightBtn = nullptr, *m_darkBtn = nullptr;
    QStackedWidget* m_stack = nullptr;
    QListWidget* m_navList = nullptr;
    QStackedWidget* m_secStack = nullptr;
    // 高级设置（门禁页）：分区同样由列表驱动，加新内容只需往 m_advSecs 追加一项
    std::vector<SecDef> m_advSecs;
    QListWidget* m_advNavList = nullptr;
    QStackedWidget* m_advStack = nullptr;
    QStackedWidget* m_advGateStack = nullptr;   // 0 = 解锁层（盖住内容）1 = 真实内容
    QLineEdit* m_advPwdEdit = nullptr;
    QLabel*    m_advErrLabel = nullptr;
    int  m_advSecIndex = -1;       // 「高级设置」在 m_secs 中的下标
    int  m_curPage = 0;
    bool m_advUnlocked = false;    // 离开该分区即置回 false（重新上锁）
    bool m_adminIsSuper = false;
    std::string m_adminPassword = kDefaultAdminPassword;
    QListWidget *m_blkListEntry = nullptr, *m_blkListFile = nullptr;
    std::vector<RowKey> m_blkEntryKeys;      // 与 m_blkListEntry 行一一对应
    std::vector<std::string> m_blkFileKeys;
    QLineEdit *m_pwdNew = nullptr, *m_pwdNew2 = nullptr;
    WinBtn *m_minBtn = nullptr, *m_maxBtn = nullptr, *m_closeBtn = nullptr;
    std::vector<QPushButton*> m_navBtns;
    std::vector<QColor> m_accents = { QColor("#326cf3"), QColor("#8b5cf6"), QColor("#10b981"), QColor("#f59e0b"), QColor("#ef4444") };
    std::vector<QPushButton*> m_accentBtns;   // 强调色色块（选中项要加描边，故持引用统一刷新）
    std::vector<CardDef> m_cards;
    std::vector<PageDef> m_pages;
    // 【预留】实用工具标签的工具注册表：加工具 = registerUtilTool 一次（当前为空，kEnableUtilTab=false）
    std::vector<SecDef> m_utilTools;
    std::vector<SecDef>  m_secs;
    SearchEngine m_engine;
    std::vector<SearchResult> m_results;
    MarkStore m_marks;
    int m_lastBlocked = 0;   // 本次搜索被屏蔽过滤掉的条数（供状态提示/后续 toast）
    // 智能列映射：(文件,工作表) -> 列号。本步只用「任务标题」（标记筛选态下替换「匹配内容」）；
    // buildColMap 是通用的，后续要加「核定工日」「工时代码」只是多调一次。
    std::map<std::pair<std::string, std::string>, int> m_renWuColMap;
    // 可配置智能列（中间列，0..4 个）；结果表顺序 = 序号 | 文件名 | [自定义列…] | 匹配内容
    std::vector<std::string> m_extraCols = defaultExtraCols();
    // source -> (文件,工作表) -> 列号 的惰性缓存（数据重载时清空；mutable 以便在 const 取值函数里填充）
    mutable std::map<std::string, std::map<std::pair<std::string, std::string>, int>> m_colMapCache;
    QListWidget* m_colList = nullptr;      // 智能列设置：当前列预览
    QComboBox*   m_colCombo = nullptr;     // 智能列设置：可编辑下拉（内建 + 已检测表头）
    QLabel*      m_colErr = nullptr;       // 智能列设置：校验错误提示
    // 搜索历史
    std::vector<HistItem> m_history;       // 最新在前
    int m_histShow = 5;                    // 下拉显示条数 0..10（0 = 不显示）
    int m_histTtlMin = 0;                  // 时效（分钟）；0 = 关闭程序后删除（不落盘）
    bool m_histRecord = true;              // 无头自检模式下关闭记录，避免污染用户历史
    QPushButton* m_histBtn = nullptr;
    QListWidget* m_histList = nullptr;
    QComboBox *m_histShowCombo = nullptr, *m_histTtlCombo = nullptr;
    QTimer* m_histTimer = nullptr;
    std::string m_dataDir;
    // 共享模式（★回迁注意：原版 DoFilter 无关，这里是数据源切换 + 不可达兜底，详见 docs/回迁标注.md 第 12 条）
    bool m_shareMode = false;                 // true = 数据源为共享目录（只读）
    std::string m_sharePath = "\\\\server\\share\\excel_search";   // UNC 路径（已规范化）
    bool m_forceReload = false;               // 强制重载：跳过缓存命中
    QRadioButton *m_modeOfflineBtn = nullptr, *m_modeShareBtn = nullptr;
    QLineEdit* m_shareEdit = nullptr;         // 共享设置里的路径输入
    QLabel* m_shareStatus = nullptr;          // 共享设置里的状态行
    QString m_shareStatusText;                // 当前共享状态（模式/来源/测试结果）
    ProbeWorker* m_probe = nullptr;           // 连通性探测（worker，避免 UNC 阻塞 UI）
    // 托盘（★回迁注意：对齐原版 V0.3.0.1「隐藏到托盘」——点 × 只隐藏，真退出走托盘菜单）
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_trayMenu = nullptr;
    bool m_trayHintShown = false;             // 本次启动首次隐藏是否已气泡提示
    bool m_migratedFromOld = false;           // 本次启动是否刚完成「V0.3.0 注册表 → config.ini」搬迁
    bool m_manualNeverShow = false;           // 启动时永久不提示说明书（对应 MAA 的 DoNotShow）
    bool m_quitting = false;                  // true = 真的在退出（closeEvent 放行）
    // 关闭行为：0=每次询问 1=直接关闭 2=最小化到托盘（可在 设置 → 通用设置 调整）
    int m_closeAction = 0;
    QRadioButton *m_closeAskBtn = nullptr, *m_closeDirectBtn = nullptr, *m_closeTrayBtn = nullptr;
    QCheckBox* m_manualChk = nullptr;   // 通用设置：启动时不再提示说明书
    ManualDialog* m_manualWin = nullptr;   // 说明书独立窗口（同一时刻只开一个；随主窗销毁）
    std::string m_settingsPath;
    std::string m_cachePath;
    std::string m_cacheInvPath;
    bool m_usedCache = false;
    QTableWidget* m_table = nullptr;
    // —— 动效（V0.3.2）涉及的控件登记表：换主题/换强调色时统一刷新，避免各写一套刷新逻辑 ——
    std::vector<CollapseCard*> m_collapseCards;   // 搜索页可折叠卡片（标题栏自绘，需 setTheme）
    std::vector<RowDelegate*>  m_rowDelegates;    // 行 hover 过渡委托（结果表 + 各设置列表）
    std::vector<StateTint*>    m_stateTints;      // QSS 控件的状态过渡（按钮 hover/按下 + 输入框焦点）
    std::map<QPushButton*, QVariantAnimation*> m_statAnims;   // 数值滚动动画（A2），每控件一条
    LoadBar* m_loadBar = nullptr;   // B4 加载进度细条（叠加在面板顶部，不进布局）
    QWidget* m_panel = nullptr;     // #panel（进度条挂它下面，手动定位）
    QPushButton *m_statFiles = nullptr, *m_statIndex = nullptr, *m_statHit = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    FolderBtn* m_folderBtn = nullptr;       // 自绘文件夹图标：打开当前数据源目录
    QLineEdit* m_filterEdit = nullptr;      // 二级筛选输入框
    bool m_markedFilterActive = false;      // 当前结果是否来自「已标记」筛选（供二级颜色精筛）
    // ★回迁注意：二级筛选的「基准」必须是一级搜索结果快照，绝不能就地缩（原版 V0.3.0 的 DoFilter 是
    //   在 g_results 上原地筛选并覆盖，改条件会在旧结果里继续缩；本版改为从基准重算）。
    std::vector<SearchResult> m_baseResults;   // 一级搜索结果快照（已过屏蔽过滤）
    std::vector<QString>      m_filterChain;   // 已生效的筛选条件；标准模式恒为 0/1 条
    bool m_chainMode = false;                  // false=标准模式(默认) true=逐级模式
    QString m_baseStatus;                      // 一级搜索时的状态栏文案（清除筛选后还原它）
    QRadioButton *m_modeStdBtn = nullptr, *m_modeChainBtn = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_toast = nullptr;         // 左下角临时提示（等价原版 g_hToast）
    QTimer* m_toastTimer = nullptr;
    // toast 动效（淡入淡出 + 上浮）：effect 只在显示期间挂着，隐藏时摘掉
    QVariantAnimation* m_toastAnim = nullptr;
    QGraphicsOpacityEffect* m_toastEff = nullptr;
    bool m_toastFading = false;
    bool m_fuzzyEnabled = true;
    int m_lastExact = 0, m_lastFuzzy = 0;
    QCheckBox* m_fuzzyBox = nullptr;
    QCheckBox* m_encChk = nullptr;
    QLineEdit* m_encEdit = nullptr;
    int m_loadedFiles = 0;
    int m_skipped = 0;   // 需要附加密码未加载的加密文件数
    int m_failedCount = 0;        // 读取失败的文件数（--report 的 failedFiles=）
    QString m_failedList;         // 读取失败的文件名清单（--report 的 failedList=）
    bool m_loadTimedOut = false;  // waitForLoad 是否超时（--report 的 loadTimeout=）
    int m_cfgEncFail = 0;         // config.ini 里"是密文却解不开"的项数（--report 的 cfgEncFail=）
    bool m_cfgKeyReady = false;   // 主密钥是否装载成功（DPAPI 解不开时为 false，此时不覆写敏感键）
    LoadWorker* m_worker = nullptr;
    bool m_addPwdEnabled = false;      // 附加密码（加密设置）
    std::string m_addPwd;
    std::string m_sessionAddPwd;       // 本次会话已输入的附加密码
    bool m_allowPrompt = true;         // 是否允许弹附加密码框（离屏截图时关闭）
public:
    // 构造：成员初始化 + 分区表 + 托盘/定时器挂钩（实现见 app_window.cpp）
    AppWindow();
    void goSettingsPage() { switchPage(1); }
    void goSection(int i) { if (m_navList) m_navList->setCurrentRow(i); }
    // 高级设置：截图/自检用（unlock=true 直接解锁到真实内容；否则停在解锁层）
    // 导航（实现见 app_window.cpp）
    void goAdvPage(bool unlock = false);
    int advSectionCount() const { return (int)m_advSecs.size(); }
    // 自检钩子：--page closedlg 时渲染关闭方式对话框（它平时是模态的，没法直接截）
    // 自检钩子：--migrate 强制搬迁（实现见 app_window_hooks.cpp）
    QString demoMigrate();
    // ---- 使用说明书（版式与交互对齐 MAA「公告」框）----
    // 实现见 app_window_manual.cpp（B2 起不再在类内内联定义）
    void openManual();
    void maybeShowManual();
    QPixmap demoManualPixmap();
    QString manualNavReport();
    // 自检钩子：--page closedlg（实现见 app_window_hooks.cpp）
    QPixmap demoCloseDialogPixmap();
    void demoTab(int i) { switchPage(i); }   // 自检钩子：--tab N 切到第 N 个顶级标签
    // 自检钩子：--reload 用。走的就是「重新加载」按钮那条路径（loadData，非强制），
    // 用来离屏复现"已有数据 → 重新加载"时的加载反馈与统计数字复位。
    void forceReloadForDemo() { loadData(); }
    // —— 以下两个是**截图/自检专用**入口（只影响本次渲染，不写配置、不改业务行为）——
    // 截图钩子：--collapse 0,1（实现见 app_window_hooks.cpp）
    void demoCollapse(const QString& spec);
    // 截图钩子：--hover <名>（实现见 app_window_hooks.cpp）
    void demoHover(const QString& what);
    void forceRowHover(QAbstractItemView* v, int row);

    void goAdvSection(int i) { if (m_advNavList) m_advNavList->setCurrentRow(i); }
    // 自检钩子：--advpwd <口令>（实现见 app_window_hooks.cpp）
    void demoAdvUnlock(const char* pw);
    // 高级设置的门禁栈（解锁层 ↔ 真实内容）切换（实现见 app_window_settings_advanced.cpp）
    void advGateAnim();
    // 设置页分区切换：进入「高级设置」若未解锁则显示解锁层；离开则重新上锁（实现见 app_window_settings.cpp）
    void onSettingsSectionChanged(int row);
    // 解锁层里的「进入」：密码正确则掀起解锁层（实现见 app_window_settings_advanced.cpp）
    void tryAdvUnlock();
    void switchPage(int idx);
    void demoSearch(const char* kw) { if (m_searchEdit) { m_searchEdit->setText(QString::fromUtf8(kw)); doSearch(); } }
    qulonglong demoHits(const char* kw) { demoSearch(kw); return (qulonglong)m_results.size(); }
    // 自检钩子：--colprobe 打印列名解析结果（空串 = UI 会提示「所输入的列不存在」）
    QString demoResolveColumn(const char* text) const { return u8(resolveColumnSource(text)); }
    // 自检查询：读取失败的文件数与清单、加载是否超时（供 --report 输出）
    int failedFileCountC() const { return m_failedCount; }
    QString failedListC() const { return m_failedList; }
    bool loadTimedOutC() const { return m_loadTimedOut; }
    // 加固自检：config.ini 解密失败项数 / 主密钥是否可用（供 --report 输出）
    int cfgEncFailC() const { return m_cfgEncFail; }
    bool cfgKeyReadyC() const { return m_cfgKeyReady; }
    // 自检钩子：--search <一级> --filter <二级> 时返回二级筛选后的条数
    // 截图钩子：--filter 二级筛选（实现见 app_window_hooks.cpp）
    qulonglong demoFilter(const char* kw);
    // 首条命中行的身份键 fn|sheet|row（实现见 app_window_hooks.cpp）
    QString firstHitKey() const;
    int demoBlocked() const { return m_lastBlocked; }
    // 无头钩子：--block / --mark / --clearmarks（实现见 app_window_hooks.cpp）
    void applyMarkCli(const QString& blockSpec, const QString& markSpec, bool clearAll);
    int blockedEntryCount() const { return (int)m_marks.blockedEntries.size(); }
    int blockedFileCount() const { return (int)m_marks.blockedFiles.size(); }
    int markedCount() const { return (int)m_marks.marked.size(); }
    int historyCount() const { return (int)m_history.size(); }
    int historyShow() const { return m_histShow; }
    int historyTtl() const { return m_histTtlMin; }
    const char* filterMode() const { return m_chainMode ? "chain" : "standard"; }
    // 自检钩子：--share <UNC 路径> / --shareoff（实现见 app_window_settings_share.cpp）
    void applyShareCli(const QString& path, bool off);
    bool shareEnabled() const { return m_shareMode; }
    // 自检：托盘是否真的可用（无托盘环境应返回 false，此时 × 仍按普通关闭处理）
    bool trayActive() const { return m_tray && m_tray->isVisible(); }
    bool manualNeverShowC() const { return m_manualNeverShow; }
    const char* closeActionName() const { return m_closeAction == 1 ? "close" : (m_closeAction == 2 ? "tray" : "ask"); }
    // 动效开关状态（--report 新增字段 anim=on|off，用来证明开关真的生效；既有字段含义不变）
    const char* animName() const { return g_noAnim ? "off" : "on"; }
    // 截图/自检用：等动效落定（实现见 app_window_hooks.cpp；kSettleMs 的取值理由见该文件）
    void settleAnim(int ms = 0);
    const char* sharePathC() const { return m_sharePath.c_str(); }
    void setAllowPrompt(bool v) { m_allowPrompt = v; m_histRecord = v; }   // 无头自检同时关闭历史记录
    // 打开当前数据源目录：离线 = 程序旁 data\，共享 = 共享目录（与原版「打开文件夹」同义）
    // 打开当前数据源目录（实现见 app_window.cpp）
    void openDataFolder();
    // ---- 托盘（对齐原版 V0.3.0.1：点 × 隐藏到托盘，真退出走托盘菜单）----
    // 程序图标：**统一用 exe 内嵌的 app.ico**（`app_icon.qrc` 嵌入 → `:/app.ico`）。
    //   早期版本这里是按强调色程序化绘制，导致「资源管理器（读 exe 内嵌图标）」与
    //   「任务栏/托盘（读代码绘制）」显示**两个不同图标**（用户反馈）。现三处统一。
    //   程序化绘制保留为**兜底**：万一 Qt 的 ico 插件缺失导致资源加载失败，也不至于出现空白图标。
    static bool appIconIsResource() { return !QIcon(":/app.ico").isNull(); }
    // 图标 / 托盘 / 关闭流程（实现见 app_window.cpp）
    QIcon makeAppIcon() const;
    void setupTray();
    void showFromTray();
    void quitApp();
    void refreshCloseRadios();
    void hideToTray();
    // ---- 共享模式 ----
    std::string dataSourceDir() const { return m_shareMode ? m_sharePath : m_dataDir; }
    void setShareStatus(const QString& s) { m_shareStatusText = s; if (m_shareStatus) m_shareStatus->setText(s); }
    // 规范化共享路径（静态方法，实现见 app_window_settings_share.cpp）
    static QString normalizeSharePath(const QString& raw);
    // 以下共享模式方法均实现于 app_window_settings_share.cpp：
    //   连通性探测 startProbe / 手动测试 testShareConnection / 强制重载 forceReloadNow + doForceReload
    //   路径编辑收尾 onSharePathEdited / 模式切换 setShareMode
    void startProbe(std::function<void(bool)> onDone);
    void testShareConnection();
    void forceReloadNow();
    void doForceReload();
    void onSharePathEdited();
    void setShareMode(bool on);
    int exportAllTo(const char* file) { return exportXlsxTo(m_results, file) ? 1 : 0; }
    int loadedCount() const { return m_loadedFiles; }
    int skippedCount() const { return m_skipped; }
    qulonglong entryCount() const { return (qulonglong)m_engine.getEntryCount(); }
    bool usedCache() const { return m_usedCache; }
    void setDark(bool d) { m_dark = d; apply(); }
    // 等后台加载结束（带超时上界；实现见 app_window_hooks.cpp）
    void waitForLoad(int timeoutMs = 120000);
    // 给界面上的 QSS 控件统一挂状态过渡：按钮（hover/按下/圆角）+ 输入框（焦点边框）。
    // 放在 buildUi() 之后扫描一遍 —— 以后新增按钮只要沿用同名 objectName 就自动获得过渡，
    // 不需要在每个工厂里重复写一行（注册表驱动的老规矩）。
    // 选择器主体必须与 qssFor() 里的那条一致，否则控件级规则压不住全局规则。
    // 给界面上的 QSS 控件统一挂状态过渡（实现见 app_window.cpp）
    void setupStateTints();
    // 把当前配置落到界面（QSS + 强调色 + 状态过渡）；实现见 app_window_config.cpp
    void apply();
protected:
    // 点 × 的三种行为（可在 设置 → 通用设置 调整，默认「每次询问」）：
    //   每次询问 → 弹关闭方式对话框（左下角「不再询问」可记住本次选择）
    //   直接关闭 → 真退出（清理托盘图标）
    //   最小化到托盘 → 只隐藏窗口，进程常驻
    // 无边框窗口交互与关闭流程（实现见 app_window.cpp）
    void closeEvent(QCloseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
private:
    void toggleMax();
    void flipTheme() { m_dark = !m_dark; saveSettings(); apply(); }
    void setAccent(const QColor& c) { m_accent = c; saveSettings(); apply(); }
    // 强调色色块：统一 28px 圆点；当前生效的那个加一圈"主题文字色"描边，否则看不出选的是哪个
    // 强调色色块选中态（实现见 app_window.cpp）
    void refreshAccentButtons();
    void pickCustomAccent() { QColor c = QColorDialog::getColor(m_accent, this, T("选择强调色")); if (c.isValid()) setAccent(c); }

    static QString u8(const std::string& s) { return QString::fromUtf8(s.c_str()); }

    // ================= 从 V0.3.0 注册表搬迁设置（一次性） =================
    // 旧版设置存在 HKCU\Software\ExcelSearch；本版改用 %APPDATA%\ExcelSearch\config.ini。
    // 不搬的话老用户升级后主题 / 屏蔽 / 标记 / 搜索历史 / 共享路径全丢。策略：
    //   · 只在「从未搬迁过」（config.ini 无 meta/registryMigrated）且注册表确有可迁键时执行一次；
    //   · **注册表只读、原样保留**（便于回退到旧版）；任何异常都不影响启动；
    //   · 废弃项一律不迁：Title / IconPath / BlurPercent / AlphaPercent（见回迁清单第 15 条）；
    //     原版的 DataPath 是**死键**（源码里从未读取，实测确认），同样不迁；
    //   · 原版有两级历史（SearchHistory1=一级搜索，SearchHistory2=二级筛选）；本版只保留一级历史，
    //     故只迁 SearchHistory1 并丢弃 SearchHistory2（本版无对应功能）。
    // 说明：屏蔽/标记的文本格式与原版**逐字节一致**（fn|sheet|row / fn|sheet|row|color），直接复用。
    // V0.3.0 注册表一次性搬迁（实现见 app_window_config.cpp）
    bool migrateFromRegistry(bool force, QString* detail = nullptr);
    // config.ini 读写（实现见 app_window_config.cpp；敏感值加密见 CONFIG-HARDENING-PLAN.md）
    void loadSettings();
    void saveSettings();

    // 共享模式：缓存 TTL 单独缩短为 30 小时（共享数据由分享机管理员维护，避免长期陈旧）
    long long cacheTtlSec() const { return m_shareMode ? (30LL * 3600) : (10LL * 24 * 3600); }
    // 数据加载与缓存复用（实现见 app_window_config.cpp）
    void loadData(bool forceReload = false);
    bool tryLoadCache();
    // 共享目录不可达：直接吃缓存（不校验清单，因为清单也读不到），并明确标注数据来源与新鲜度
    // 文件名列表 → 简短提示语（最多列 3 个，其余归并成"等 N 个"）
    // 提示文案（实现见 app_window_config.cpp）
    static QString briefNames(const std::vector<std::string>& names);
    QString emptySourceHint() const;
    // 缓存回退与加载收尾（实现见 app_window_config.cpp）
    void loadCacheFallback();
    void onLoadFinished();
    // 智能列：按「表头包含关键词 + 精确匹配加权」选列（实现见 app_window_settings_smartcols.cpp）
    std::map<std::pair<std::string, std::string>, int> buildColMap(const std::string& target) const;
    void buildColMaps() { m_renWuColMap = buildColMap("任务标题"); m_colMapCache.clear(); }
    // ---- 可配置智能列 ----
    static bool isMetaCol(const std::string& s) { return s == kMetaSheetCol || s == kMetaRowCol; }
    // 去重收集全部工作簿表头（供下拉候选与校验）——实现见 app_window_settings_smartcols.cpp
    QStringList allHeaderNames() const;
    // 把用户输入解析成「实际可用的列来源」：内建元数据原样通过 → 表头包含 → rapidfuzz 兜底（低于阈值视为不存在）
    std::string resolveColumnSource(const std::string& text) const;
    // 取某结果行在指定来源列上的显示值
    std::string cellValueFor(const SearchResult& r, const std::string& source) const;
    // 按当前配置重建结果表列（固定三列夹住自定义列），并重填数据
    // 结果表列与内容（实现见 app_window_search.cpp）
    void applyColumns();
    std::vector<SearchResult> buildMarkResults(int colorFilter) const;
    // ---- 搜索历史 ----
    // 实现见 app_window_history.cpp（B3 起不再在类内内联定义）
    void purgeHistory();
    void addHistory(const QString& kw);
    void clearHistory();
    void refreshHistoryUI();
    void showHistoryMenu();
    // 统计卡片数值滚动（实现见 app_window_search.cpp；四条要点见该文件头注释）
    void setStat(QPushButton* btn, qulonglong target, int delayMs = 0);
    // 三个数字之间的错峰：已加载文件 → 索引记录 → 本次命中（0 / 40 / 80ms）
    void setHitStat(qulonglong n) { setStat(m_statHit, n, 2 * kStaggerMs); }
    void updateStats();
    // 一级搜索（实现见 app_window_search.cpp）
    void doSearch();
    void showHitTip() { QToolTip::showText(QCursor::pos(), QString(T("精确 %1 条 · 模糊 %2 条")).arg(m_lastExact).arg(m_lastFuzzy), this); }
    // 筛选链应用：从一级结果快照逐级重算（实现见 app_window_search.cpp）
    void applyFilterChain();
    // 二级筛选（实现见 app_window_search.cpp）
    void doFilter();
    void clearFilter();
    // 行详情对话框（实现见 app_window_search.cpp）
    void showDetail(int row);
    // 导出所选行（实现见 app_window_search.cpp）
    void exportSelected();
    // 左下角提示气泡：淡入 + 上浮（150ms OutCubic），到时淡出再隐藏。
    // 关闭动效时退化成原来的"直接显示/直接隐藏"，位置与时长都不变。
    // 左下角提示气泡（实现见 app_window.cpp）
    void showToast(const QString& text, int ms = 10000);
    void stopToastAnim();
    void hideToastAnim();
    // 结果表行交互：选中行 / 右键菜单 / 标记 / 屏蔽（实现见 app_window_search.cpp）
    std::vector<int> selectedRows() const;
    void onTableContextMenu(const QPoint& pos);
    void markSelected(const std::vector<int>& sel, int color);
    void blockSelectedEntries(const std::vector<int>& sel);
    void blockSelectedFiles(const std::vector<int>& sel);
    // 结果表填充与搜索页卡片（实现见 app_window_search.cpp）
    void fillTable(const std::vector<SearchResult>& res);
    QWidget* statCell(const QString& label, QPushButton** valOut);
    QWidget* makeStatCard();
    QWidget* makeResultCard();
    // 搜索页装配（实现见 app_window_search.cpp）
    QWidget* makeSearchPage();
    // 通用卡片工厂：标题栏 + 可折叠内容（设置页与搜索页共用；实现见 app_window.cpp）
    QWidget* cardFrame(const QString& title, QWidget* body);
    // 高级设置分区：外层是一个「解锁层 + 真实内容」两页栈。
    // 未解锁时只显示解锁层（盖住内容，看不到任何设置项）；密码正确后切到真实内容页，再离开该分区即上锁。
    // ★回迁注意：原版是搜索框隐藏入口关键词 + 弹窗密码；此处改为设置页内分区 + 解锁层，入口形式不同。
    //   详见 docs/回迁标注.md 第 6 条。
    // 高级设置分区与屏蔽/标记/密码管理（实现见 app_window_settings_advanced.cpp）
    QWidget* makeAdvSec();
    QWidget* makeBlockAdminSec();
    QWidget* makeMarkAdminSec();
    QWidget* makePasswordSec();
    void refreshBlockAdmin();
    void clearMarksByColor(int color);
    void refreshTableMarks();
    // 设置页「智能列设置」子卡 + 列增删改动作（实现见 app_window_settings_smartcols.cpp）
    QWidget* makeSmartColSec();
    bool smartColValidateInput(std::string& out, QString& note);
    void smartColError(const QString& msg);
    void smartColAdd();
    void smartColUpdate();
    void smartColRemove();
    void smartColApplied(const QString& toast);
    void smartColRefresh();
    // 设置页「搜索历史」子卡（实现见 app_window_history.cpp）
    QWidget* makeHistorySec();
    // 设置页外壳 / 关于页 / 预留实用工具页（实现见 app_window_settings.cpp）
    void registerUtilTool(const QString& title, std::function<QWidget*()> make);
    QWidget* makeUtilPage();
    QWidget* makeSettingsPage();
    // 通用设置页（外观 + 关闭选项）与其搜索设置子卡（实现见 app_window_settings_general.cpp）
    QWidget* makeGeneralSec();
    QWidget* makeSearchSec();
    // 设置页「加密设置」与「共享设置」子卡（实现见 app_window_settings_share.cpp）
    QWidget* makeEncSec();
    QWidget* makeShareSec();
    // 关于页（实现见 app_window_settings.cpp）
    QWidget* makeAboutSec();
    // 界面装配：标题栏 + 顶部标签 + 页面栈（实现见 app_window.cpp）
    void buildUi();
};

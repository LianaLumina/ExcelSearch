# 第三方组件与开源声明

> 软件：Excel 表格关键字搜索工具 V0.3.2　作者 / 发行者：觉心恋影
> 本项目许可证：**GPL-3.0**（全文见安装目录下的 LICENSE）

> 本文件随程序分发（安装目录 `licenses\`）。本程序以**动态链接**方式使用 Qt，
> 用户可自行替换 `licenses\` 同级目录下的 Qt 动态库（`Qt6*.dll`）。

---

## 1. Qt 6（LGPL-3.0-only）

本程序使用 Qt 6（模块：Core / Gui / Widgets / Network），以**动态链接**方式使用，
未对 Qt 做任何修改。Qt 版权归 **The Qt Company Ltd.** 及其贡献者所有。

- 许可证：**GNU Lesser General Public License v3.0**（LGPL-3.0-only）
- 全文：见同目录 `Qt-LGPL-3.0-only.txt`
- 获取 Qt 源码：<https://download.qt.io/archive/qt/>
- **替换说明**：本程序直接加载同目录下的 `Qt6*.dll`。若你希望使用自行编译的 Qt，
  直接用你的版本覆盖这些 DLL 即可，无需改动本程序。

## 2. 本程序自带/静态编译的第三方库

| 组件 | 用途 | 许可证 | 版权 |
|---|---|---|---|
| miniz | ZIP 读写（xlsx/docx 解包） | MIT | (c) 2013-2014 RAD Game Tools and Valve Software；(c) 2010-2014 Rich Geldreich and Tenacious Software LLC |
| pugixml | XML 解析 | MIT | (c) 2006-2026 Arseny Kapoulkine |
| rapidfuzz（rapidfuzz-cpp） | 模糊匹配 | MIT | (c) 2022 Max Bachmann |
| libxls | 旧版 .xls 读取 | BSD-2-Clause 系 | (c) 2004 Komarov Valery；(c) 2006 Christophe Leitienne；(c) 2008-2017 David Hoer |
| win_iconv | 编码转换 | **Public Domain** | (c) 2009-2016 Yukihiro Nakadaira 等（文件头注明"placed in the public domain"） |

MIT / BSD-2-Clause 的完整条文见同目录 `MIT.txt`、`BSD-2-Clause.txt`。

## 3. Qt 运行时所依赖的库

`windeployqt` 部署时会一并带上若干依赖库（如 libpng、freetype、harfbuzz、zlib、pcre2 等），
它们的许可证文本已随包收录在 `licenses\msys2\` 子目录中（由 `tools\deploy.ps1` 按实际部署的
DLL 自动从构建环境收录）。

## 4. 界面设计：视觉风格参考致谢

本程序的界面**视觉风格**（配色体系、圆角、深色面板、无边框布局等）参考了以下开源项目的设计语言，
在此致谢：

- **MAA / MaaWpfGui** — <https://github.com/MaaAssistantArknights/MaaAssistantArknights>
- **MaaEnd** — <https://github.com/MaaEnd/MaaEnd>

> **说明**：仅为**视觉风格参考**。本程序未使用、未复制上述项目的任何源代码、样式表或资源文件，
> 因此不受其（AGPL-3.0）许可条款约束；此处为出于尊重的自愿致谢。
> 上述项目名称与商标归各自权利人所有，与本程序无隶属或背书关系。

## 5. 其他

- 程序图标：沿用本项目历史版本（V0.3.0）的图标资源。
- Windows 平台加密调用（BCrypt）属操作系统 API，无第三方许可要求。

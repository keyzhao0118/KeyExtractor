# KeyExtractor 软件设计文档

> 版本: 1.0 | 日期: 2026-06-29 | 状态: 草案

---

## 1. 项目概述

KeyExtractor 是一款面向 Windows 平台的现代化解压软件，通过 Qt6 渲染界面、7zip 内核驱动解压，最终以 MSIX 格式打包上架微软应用商店。

### 1.1 核心目标
- 支持 ZIP / RAR / 7Z / TAR 四种压缩格式的解压
- 文件关联：对四种格式注册"打开方式"，作为主要调起入口
- 右键菜单扩展：提供"用 KeyExtractor 打开"和"解压到..."两个 Shell 扩展
- 树型视图展示压缩包内文件列表
- 符合微软商店上架的全部技术与安全规范
- 支持英文与简体中文界面

### 1.2 技术栈
| 层 | 选型 | 说明 |
|---|------|------|
| 语言 | C++20 | Windows SDK 兼容性 |
| 界面 | Qt 6.x (via vcpkg) | Widgets 模块用于主窗口，Core/Gui 作为基础 |
| 解压内核 | 7zip (via vcpkg) | vcpkg 编译动态库，直接链接 7zip C API |
| 包管理 | vcpkg (manifest mode) | 管理 Qt6、7zip 等三方依赖 |
| 构建 | CMake 3.21+ | 跨平台构建配置 |
| 打包 | MSIX (Windows Application Packaging) | 用于 Microsoft Store 上架 |
| 国际化 | Qt Linguist (`.ts` / `.qm`) | 英文 + 简体中文 |

---


## 2. 系统架构

### 2.1 整体分层

```
┌──────────────────────────────────────────────┐
│                 MSIX 安装包                    │
│  ┌────────────────────────────────────────┐   │
│  │          UI 层（Qt6 Widgets）           │   │
│  │  · MainWindow                          │   │
│  │  · FileTreeView / FileTreeModel        │   │
│  │  · ExtractPathDialog                   │   │
│  │  · ExtractionDialog                   │   │
│  │  · PasswordDialog / OverwriteDialog    │   │
│  │  · ErrorDialog                         │   │
│  ├────────────────────────────────────────┤   │
│  │          核心逻辑层（C++）              │   │
│  │  · ArchiveManager                     │   │
│  │  · ExtractionWorker（QThread）         │   │
│  │  · CommandLineParser                  │   │
│  ├────────────────────────────────────────┤   │
│  │          解压内核层                     │   │
│  │  · SevenZipEngine（封装 7zip API）     │   │
│  ├────────────────────────────────────────┤   │
│  │        系统集成层（MSIX 声明式）         │   │
│  │  · AppxManifest.xml（文件关联+右键菜单） │   │
│  │  · 部署引擎自动处理注册表写入（第4章）    │   │
│  └────────────────────────────────────────┘   │
└──────────────────────────────────────────────┘
```

### 2.2 进程模型

```
KeyExtractor.exe  — 唯一进程：承载 UI + 核心逻辑 + 解压内核
```

本应用采用**单进程架构**。在 MSIX 声明式集成模型下，文件类型关联和右键
菜单均通过 `AppxManifest.xml` 声明完成，部署引擎在安装时自动向真实注册表
写入必要键值——不需要 COM Shell Extension DLL、不需要进程外服务器、不需要
后台常驻进程。

### 2.3 MSIX 声明式集成总览

```
AppxManifest.xml 声明
        │
        │  安装时
        ▼
AppX 部署引擎（SYSTEM 权限）
        │
        ├──→ HKLM\Software\Classes\kext.archive\shell\...   （ProgID + ShellVerbs）
        ├──→ HKLM\Software\Classes\.zip\OpenWithProgids\... （候选列表 × 4 种后缀）
        └──→ HKCU\...\FileExts\.zip\UserChoice              （默认打开程序）
                │
                │  运行时
                ▼
    用户双击 / 右键压缩文件
                │
                ▼
    Explorer 读取真实注册表 → 启动 KeyExtractor.exe <path>
```

**核心设计原则**：系统集成不靠程序运行时修改注册表，全靠清单声明。程序本身
不包含注册表操作代码，只需在 `main.cpp` 中解析命令行参数（文件路径 + verb）。

---
## 3. 模块设计

### 3.1 模块总览

```
src/
├── main.cpp                          # 入口，命令行参数解析；多文件时启动多个独立进程
├── app/
│   ├── Application.h/cpp             # QApplication 子类，全局初始化
│   └── CommandLineParser.h/cpp       # 解析命令行（文件路径、Shell verb）
├── ui/
│   ├── MainWindow.h/cpp              # 主窗口（标题栏、文件树、状态栏）
│   ├── FileTreeView.h/cpp            # QTreeView 封装，展示压缩包内文件
│   ├── FileTreeModel.h/cpp           # QAbstractItemModel 子类，文件树数据模型
│   ├── ExtractPathDialog.h/cpp       # 解压目录选择对话框
│   ├── ExtractionDialog.h/cpp        # 解压进度对话框
│   ├── PasswordDialog.h/cpp          # 密码输入对话框
│   ├── OverwriteDialog.h/cpp         # 文件覆盖对话框
│   ├── ErrorDialog.h/cpp             # 错误提示对话框
├── core/
│   ├── ArchiveManager.h/cpp          # 解压调度中心，协调 UI 与内核
│   ├── ArchiveItem.h/cpp             # 压缩包文件条目数据结构
│   ├── ExtractionOptions.h           # 解压选项（目标路径、覆盖策略等）
│   └── ExtractionWorker.h/cpp        # 后台解压线程
├── engine/
│   ├── SevenZipEngine.h/cpp          # 7zip C API 封装
│   └── ArchiveFormat.h               # 格式枚举 (ZIP/RAR/7Z/TAR) + MIME 映射
├── shell/
│   └── ShellCommandVerb.h            # Shell verb 常量定义
└── resources/
    ├── translations/
    │   ├── keyextractor_zh_CN.ts
    │   └── keyextractor_zh_CN.qm
    ├── KeyExtractor.rc               # Windows 资源文件（图标、版本信息）
    ├── app.ico
    ├── app.manifest                  # SxS 清单
    └── qt.conf                       # Qt 运行时配置
```

### 3.2 关键模块说明

#### 3.2.1 ArchiveItem (数据模型)
```cpp
struct ArchiveItem {
    std::wstring name;      // 文件名（含后缀）
    std::wstring path;      // 压缩包内完整路径
    uint64_t    size;       // 解压后大小（bytes）
    bool        isDirectory;// 是否为目录
    // 树结构：父节点指针 + 子节点列表
};```

#### 3.2.2 ArchiveManager
- 接收解压任务（打开查看 / 解压到指定目录）
- 调用 `SevenZipEngine` 列出文件列表、执行解压
- 管理 `ExtractionWorker` 线程，通过 Qt 信号槽与 UI 通信
- 接口：

```cpp
class ArchiveManager : public QObject {
    Q_OBJECT
public:
    void openArchive(const QString& filePath);           // 打开压缩包，列出文件树
    void extractAll(const QString& destDir);             // 全部解压
    void extractSelected(const QStringList& itemPaths,
                         const QString& destDir);        // 解压选中条目
    void extractAndOpen(const QString& itemPath);        // 单文件解压到临时目录 + 系统默认程序打开
    void cancelExtraction();
signals:
    void fileListReady(const QVector<ArchiveItem>& items);
    void extractionProgress(int percent, const QString& currentFile);
    void extractionFinished(bool success, const QString& errorMsg);
};
```

#### 3.2.3 SevenZipEngine
通过 vcpkg 链接 7zip 动态库，提供：
- `listArchive(path)` → `QVector<ArchiveItem>`
- `extractItems(path, destDir, itemPaths)` → 进度回调
- 支持密码输入（加密压缩包）
- 内部使用 `NzCom.h` / `7zTypes.h` 等 7zip SDK 头文件

> **vcpkg 集成说明**: 通过 `vcpkg.json` 声明 `7zip` port，vcpkg 编译产物为
> 可链接的动态库，`SevenZipEngine` 直接链接并调用 7zip C API。


---

## 4. Windows Shell 集成设计

### 4.0 背景：MSIX 声明式部署模型

MSIX 是一种**声明式部署单元**。传统 `.exe` 安装器通过自执行代码直接
修改注册表、写入文件、注册 COM 组件来完成系统集成；MSIX 则将"我想要
什么能力"写入清单文件（`AppxManifest.xml`），由 Windows 的 **AppX 部署
引擎**在安装时替你完成所有系统级配置。

类比：传统安装器是你自己拿钥匙进门布置家具；MSIX 是你填好布置表格，
物业帮你摆好——并在你搬家（卸载）时自动复原。

核心优势：
1. **安装即注册、卸载即清除**——不会残留注册表垃圾
2. **不需要你写注册表代码**——你只需把声明写对

---

### 4.1 文件类型关联 (File Type Association)

#### 4.1.1 清单声明

在 `AppxManifest.xml` 中声明 `windows.fileTypeAssociation` 扩展：

```xml
<Extensions>
    <uap3:Extension Category="windows.fileTypeAssociation">
        <uap3:FileTypeAssociation Name="kext.archive" Parameters='"%%1"'>
            <uap3:SupportedFileTypes>
                <uap3:FileType>.zip</uap3:FileType>
                <uap3:FileType>.rar</uap3:FileType>
                <uap3:FileType>.7z</uap3:FileType>
                <uap3:FileType>.tar</uap3:FileType>
            </uap3:SupportedFileTypes>
        </uap3:FileTypeAssociation>
    </uap3:Extension>
</Extensions>
```

#### 4.1.2 Windows 内部处理流程

用户安装 KeyExtractor.msix 时，AppX 部署引擎读取 `AppxManifest.xml`，
自动完成以下等价注册表操作（**你不需要写任何注册表代码**）：

**步骤 ① 创建 ProgID**

`Name` 属性值 `kext.archive` 成为系统的 ProgID（程序标识符）。
部署引擎在注册表中创建对应键值：

```
HKEY_CLASSES_ROOT\kext.archive\
    shell\open\command\
        (默认) = "C:\Program Files\WindowsApps\<PackageId>\KeyExtractor.exe" "%1"
```

`Parameters` 中的 `%1` 在 Shell 调起时被替换为文件路径。

**步骤 ② 建立后缀关联（两层机制）**

对 `.zip` `.rar` `.7z` `.tar` 每种后缀，部署引擎写入两层注册表：

```
;; 层 A：候选列表 — 出现在右键"打开方式"子菜单
HKEY_CLASSES_ROOT\.zip\OpenWithProgids\
    kext.archive = ""

;; 层 B：默认打开程序 — 双击直接启动你的 App
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\
    Explorer\FileExts\.zip\UserChoice\
        Progid = "Applications\KeyExtractor.exe"
```

| 关联层 | 注册表位置 | 效果 |
|--------|-----------|------|
| 候选列表 (OpenWithProgids) | `HKCR\.zip\OpenWithProgids` | 右键 → "打开方式"列表中出现 KeyExtractor |
| 默认关联 (UserChoice) | `HKCU\...\FileExts\.zip\UserChoice` | 双击文件 → 直接启动 KeyExtractor |

**步骤 ③ 卸载自动清理**

用户卸载 KeyExtractor 时，部署引擎自动移除上述所有注册表项。
这是传统安装器做不到的——MSIX 从根本上解决了卸载残留问题。

#### 4.1.3 完整调用链路

```
用户双击 .zip 文件
        │
        ▼
Windows Shell 查找 .zip → ProgID → 可执行文件路径
        │
        ▼
CreateProcess: KeyExtractor.exe "C:\Users\..\archive.zip"
        │
        ▼
main.cpp → CommandLineParser → ArchiveManager::openArchive(path)
```

#### 4.1.4 多选传参机制

用户在资源管理器中多选压缩文件后右键打开，Windows Shell 会将**所有
选中文件路径作为独立命令行参数一次性传入**：

```
KeyExtractor.exe "C:\a.zip" "C:\b.rar" "C:\c.7z"
```

这与单选时传入单个 `%1` 的机制一致——Shell 自动为你拼接参数列表。
应用程序的 `CommandLineParser` 需支持接收并遍历多个文件路径。

---

### 4.2 右键菜单 (Context Menu)

#### 4.2.1 清单声明

在同一 `FileTypeAssociation` 中追加 `desktop2:ShellVerbs`，声明两个
右键菜单项：

```xml
<uap3:FileTypeAssociation Name="kext.archive" Parameters='"%%1"'>
    <uap3:SupportedFileTypes>
        <uap3:FileType>.zip</uap3:FileType>
        <uap3:FileType>.rar</uap3:FileType>
        <uap3:FileType>.7z</uap3:FileType>
        <uap3:FileType>.tar</uap3:FileType>
    </uap3:SupportedFileTypes>
    <desktop2:ShellVerbs>
        <desktop2:Verb Id="open"
            DisplayName="用 KeyExtractor 打开"
            Parameters='"%%1"' />
        <desktop2:Verb Id="extract"
            DisplayName="解压到..."
            Parameters='"--extract" "%%1"' />
    </desktop2:ShellVerbs>
</uap3:FileTypeAssociation>
```

部署引擎自动在 `HKCR\kext.archive\shell\` 下创建 `open` 和 `extract`
两个子键。用户右键压缩文件时，Explorer 读取这些键值生成菜单项。

#### 4.2.2 菜单行为（已简化：不做动态控制）

**设计决策：两个菜单项在任何选择状态下均显示，不根据选中数量动态切换。**

| 选择状态 | "用 KeyExtractor 打开" | "解压到..." | 传入参数 |
|----------|----------------------|------------|---------|
| 单选文件 | ✅ 显示 | ✅ 显示 | `"%1"` |
| 多选文件 | ✅ 显示 | ✅ 显示 | `"%1" "%2" "%3" ...` |

多选时显示"用 KeyExtractor 打开"虽非核心用途，但不导致功能错误——
用户自然会选择"解压到..."。`CommandLineParser` 接收多个路径后遍历处理。

#### 4.2.3 为何不做动态菜单？

MSIX 标准的 `desktop2:ShellVerbs` 只能声明固定的 Verb 集，无法在运行
时根据选中数量动态显示 / 隐藏菜单。实现动态菜单需要 COM 扩展和额外的进程外服务器，复杂度远超收益，当前版本不做。

#### 4.2.4 技术限制与要求

- `desktop2:ShellVerbs` 要求 Windows 10 1809+（Build 17763，2018 年发布）
- Verb 的 `Parameters` 仅支持固定字符串模板，不含条件逻辑
- `DisplayName` 列出的文本直接用于菜单项标题

---

### 4.3 命令行接口规范

| 触发源 | 命令行示例 | 程序行为 |
|--------|-----------|---------|
| 双击文件 | `KeyExtractor.exe <path>` | 打开压缩包，在树型视图中显示内容 |
| 右键"用 KeyExtractor 打开" | `KeyExtractor.exe <path>` | 同双击打开 |
| 右键"解压到..." | `KeyExtractor.exe --extract <path1> [path2...]` | 弹出解压目录选择对话框，确认后弹出解压进度对话框 |

> **多文件场景**：
> - `--extract` 后跟多个文件路径：弹出**单个**目录选择对话框，串行逐个解压
> - 不带 `--extract` 的多文件打开：为每个文件启动独立进程，每个进程一个主窗口，最多打开前 10 个文件

---

### 4.4 真实注册表与虚拟注册表：声明式关联的底层机制

上面说"部署引擎自动在注册表中创建键值"，这个注册表是**真实注册表**还是
MSIX 的虚拟注册表？答案是——**取决于写入来源**。

#### MSIX 注册表双层模型

| 写入来源 | 落盘位置 | 谁可见 | 卸载时 |
|---------|---------|--------|--------|
| `AppxManifest.xml` 声明（部署引擎） | **真实注册表** `HKCR\...` `HKCU\...\FileExts\...` | 整个系统（Explorer + 所有程序） | 部署引擎自动删除 |
| App 运行时调用 `RegSetValue` | **虚拟注册表**（私有 hive） | 仅你的 App 自己 | 私有 hive 整体删除 |


#### 插曲：`HKEY_CLASSES_ROOT` 的物理真相

`HKEY_CLASSES_ROOT`（`HKCR`）经常被当成一个独立的注册表 hive，但实际上
**`HKCR` 是一个合并视图**，它将两个物理 hive 的内容逻辑叠加展示：

```
你看到的 HKCR\.zip\OpenWithProgids\...
         │
         │ 合并视图 (merged view)
         │
    ┌────┴────┐
    ▼         ▼
HKCU\Software\Classes     ← 用户级（优先级更高，覆盖同名键）
    ...
                                  +
HKLM\Software\Classes     ← 机器级（所有用户共享的基础）
    ...
```

当你通过注册表编辑器或 API 读写 `HKCR` 时，Windows 实际按以下规则路由：
- **写操作**：根据调用者权限和键的现有位置，路由到 `HKCU\...\Classes` 或 `HKLM\...\Classes`
- **读操作**：先查 `HKCU\...\Classes`，没有再到 `HKLM\...\Classes`；两处都有则在 API 层面已合并

#### 部署引擎实际写入了哪里？

AppX 部署引擎以 **SYSTEM 权限** 处理 `AppxManifest.xml` 声明，将不同内容
写入物理注册表的不同 hive：

| 清单声明内容 | 物理写入位置 | Hive | 理由 |
|-------------|-------------|------|------|
| ProgID 定义（`kext.archive\shell\...`） | `HKLM\Software\Classes\kext.archive\...` | 机器级 | 所有用户共享此 ProgID |
| 后缀候选列表（`.zip\OpenWithProgids`） | `HKLM\Software\Classes\.zip\OpenWithProgids\...` | 机器级 | 所有用户都能在"打开方式"列表中看到 |
| 默认关联（`UserChoice`） | `HKCU\Software\...\FileExts\.zip\UserChoice` | 用户级 | 每个用户可以独立选择默认打开程序 |
| `ShellVerbs` 右键菜单 | `HKLM\Software\Classes\kext.archive\shell\...` | 机器级 | 所有用户共享相同右键菜单 |

**关键判断逻辑**：
- 需要**所有用户**共享的内容 → 写 `HKLM`（SYSTEM 权限确保可写）
- 每个用户**独立决策**的内容 → 写当前用户的 `HKCU`（`UserChoice` 的 hash 校验由 Explorer 处理，防篡改）

所有写入均为**真实注册表**的物理 hive——未经 MSIX 虚拟化重定向——因为 Explorer
进程在容器外运行，必须从真实注册表读取这些键值。


#### 为什么文件关联必须写入真实注册表？

Explorer（资源管理器）是系统进程，运行在 MSIX 容器**外部**。它判断
"`.zip` 双击用什么打开""右键菜单有哪些"时，**直接读取真实注册表**。

如果文件关联落在虚拟注册表里，Explorer 根本看不见——关联就无法生效。
MSIX 设计者早已意识到这点，所以赋予部署引擎 SYSTEM 权限，使其能将
清单中的声明**安全地写入真实注册表**。

#### 为什么你的 App 运行时注册表操作被虚拟化？

这是 MSIX 的沙箱安全模型：容器内的进程对 `HKLM` 无写权限，对 `HKCU`
的写入被重定向到私有 hive。这保证了：
- App 卸载时不留垃圾
- App 之间不会通过注册表互相干扰
- App 无法篡改系统级配置

#### 本项目的注册表策略（重申）

- **文件关联 & 右键菜单**：`AppxManifest.xml` 声明 → 部署引擎写入真实注册表
- **应用自身配置**：运行时写 `HKCU`（自动虚拟化，不影响真实注册表）
- **程序代码中不包含任何 Shell 注册逻辑**（`ShellRegistrar` 模块已移除）




## 5. UI 设计

### 5.1 主窗口布局

极简窗口：可拖动的标题栏 + 文件树视图 + 底部状态栏。无菜单栏、无工具栏。

```
┌──────────────────────────────────────────────┐
│  KeyExtractor — archive.zip              ─ ✕ │
│  ─────────────────────────────────────────── │
│  ┌──────────────────────────────────────┐    │
│  │ 名称          │ 类型     │ 解压后大小 │    │
│  ├──────────────────────────────────────┤    │
│  │ 📁 docs/      │ 文件夹   │          │    │
│  │   📄 readme.md│ Markdown │ 1.2 KB   │    │
│  │ 📁 src/       │ 文件夹   │          │    │
│  │   📄 main.cpp │ C++源文件│ 3.5 KB   │    │
│  │   📄 utils.h  │ C头文件  │ 1.1 KB   │    │
│  └──────────────────────────────────────┘    │
│  ─────────────────────────────────────────── │
│  共 15 个文件 | 解压后大小 2.3 MB  [全部解压] │
└──────────────────────────────────────────────┘
```

- **标题栏**：显示 `KeyExtractor — <压缩包文件名>`，支持拖动窗口
- **树视图**：QTreeView 三列（名称 / 类型 / 解压后大小），仅支持单选。类型列通过文件名后缀从系统获取图标和文本
- **状态栏**：统计信息 + "全部解压"按钮
- "全部解压"按钮触发解压流程（弹出目录选择对话框 → 确认后弹出解压进度对话框）

### 5.2 右键菜单

用户在树视图中选中一个条目后右键，弹出上下文菜单：

| 选中类型 | 右键菜单项 | 行为 |
|---------|-----------|------|
| 目录 | **打开** | 展开该目录节点 |
| 目录 | **解压选中项** | 弹出目录选择对话框 → 确认后弹出解压进度对话框 |
| 文件 | **打开** | 将文件解压到 `<压缩包同级目录>\<包名>_temp\` → 用系统默认程序打开 |
| 文件 | **解压选中项** | 弹出目录选择对话框 → 确认后弹出解压进度对话框 |

"打开"与"解压选中项"的区分：
- **打开**：面向"快速查看"场景，零交互
- **解压选中项**：面向"自定义位置"场景，弹出路径选择 → 进度对话框

### 5.3 解压目录选择对话框

触发解压时（全部解压按钮 / 右键"解压选中项"），首先弹出该对话框：

```
┌──────────────────────────────────────────┐
│  解压到...                           ✕   │
│  ─────────────────────────────────────── │
│                                         │
│  目标路径：                              │
│  ┌─────────────────────────────────────┐│
│  │ C:\Users\...\archive               ││
│  └─────────────────────────────────────┘│
│  [浏览...]                               │
│                                         │
│  ☑ 解压后打开目录                        │
│                                         │
│                     [解压]  [取消]       │
└──────────────────────────────────────────┘
```

- **默认路径**：`<压缩包所在目录>\<压缩包文件名（不含后缀）>\`
- **"浏览..."按钮**：调用系统目录选择对话框
- **"解压后打开目录"勾选框**：默认勾选，解压完成后调用 `ShellExecute` 打开目标文件夹
- **"解压"按钮**：校验路径有效性 → 弹出解压进度对话框


### 5.4 解压进度对话框

用户在解压目录选择对话框中点击"解压"后弹出。所有解压路径（包括
系统右键菜单"解压到..."）均先经过目录选择对话框，再进入此对话框。

```
┌──────────────────────────────────────────┐
│  正在解压...                         ✕   │
│  ─────────────────────────────────────── │
│                                         │
│  来源: archive.zip                       │
│  目标: C:\Users\...\archive             │
│                                         │
│  ████████████████████░░░░░░  68%         │
│                                         │
│  当前文件: src/main.cpp                   │
│  已解压: 12 / 15 个文件                   │
│                                         │
│                     [后台]  [取消解压]    │
└──────────────────────────────────────────┘
```

- **来源 / 目标**：显示压缩包路径和解压目标路径
- **进度条 + 百分比**：实时更新
- **当前文件**：正在处理的文件名
- **文件计数**：`已解压 / 总数`
- **[后台]按钮**：最小化到系统托盘，后台继续解压；完成后托盘弹出气泡通知
- **[取消解压]按钮**：终止所有进行中的解压任务


### 5.5 密码输入对话框

当 `SevenZipEngine` 检测到压缩包（或其内部条目）需要密码时弹出：

```
┌─────────────────────────────────────┐
│  需要密码                        ✕  │
│  ─────────────────────────────────  │
│                                     │
│  archive.zip 已加密，请输入密码：     │
│                                     │
│  ┌─────────────────────────────────┐│
│  │ ●●●●●●●●                        ││
│  └─────────────────────────────────┘│
│  ☐ 显示密码                          │
│                                     │
│                      [确定] [取消]    │
└─────────────────────────────────────┘
```

- 按钮根据触发场景决定行为：
  - **确定**：用输入密码重试解压，密码错误则再次弹出并标注"密码错误"
  - **取消**：终止本次解压任务
- "显示密码"勾选后密文切换为明文

### 5.6 文件覆盖对话框

当目标路径已存在同名文件时弹出：

```
┌──────────────────────────────────────────────┐
│  确认文件替换                             ✕   │
│  ──────────────────────────────────────────  │
│                                              │
│  目标已包含同名文件：                          │
│  readme.md                                    │
│                                              │
│  源文件: archive.zip \ readme.md               │
│  目标文件: C:\...\archive\readme.md            │
│                                              │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐      │
│  │ 替换 │  │ 跳过 │  │ 重命名│  │ 取消 │      │
│  └──────┘  └──────┘  └──────┘  └──────┘      │
│                                              │
│  ☐ 对所有冲突执行相同操作                      │
└──────────────────────────────────────────────┘
```

- 四个按钮：
  - **替换**：覆盖目标文件
  - **跳过**：跳过当前文件不解压
  - **重命名**：自动追加 ` (1)` 后缀后写入
  - **取消**：终止整个解压任务
- "对所有冲突执行相同操作"：勾选后不再弹出，自动执行上一步选择的策略

### 5.7 错误提示对话框

解压过程中发生不可恢复的错误时弹出：

```
┌──────────────────────────────────────┐
│  解压失败                         ✕  │
│  ──────────────────────────────────  │
│                                      │
│  无法解压以下文件：                    │
│  archive.zip                          │
│                                      │
│  原因：文件已损坏或格式不受支持。       │
│                                      │
│                           [确定]      │
└──────────────────────────────────────┘
```

- 典型触发条件：文件损坏、格式不识别、磁盘空间不足、权限拒绝
- 用户点击"确定"后关闭对话框，不重试

### 5.8 整体交互流程

```
                      ┌──────────────────────┐
                      │   KeyExtractor.exe    │
                      └──────────┬───────────┘
                                 │
              ┌──────────────────┼──────────────────┐
              ▼                  ▼                   ▼
      双击/右键"打开"      主窗口[全部解压]      右键"解压到..."
      打开压缩包             树视图"解压选中项"    (--extract)
      显示文件树                  │                   │
              │                  │                   │
              │                  ▼                   ▼
              │        ┌──────────────────────────────┐
              │        │   解压目录选择对话框           │
              │        │   默认路径 + ☑打开目录 + 浏览  │
              │        └─────────────┬────────────────┘
              │                      │ 点击"解压"
              │                      ▼
              │        ┌──────────────────────────────┐
              │        │   解压进度对话框               │
              │        │   进度条 + 文件名 + 计数        │
              │        │   [后台] [取消]               │
              │        └─────────────┬────────────────┘
              │                      │
              │         ┌────────────┼────────────┐
              │         ▼            ▼            ▼
              │    [密码对话框] [覆盖对话框] [错误对话框]
              │    (按需弹出)   (按需弹出)   (按需弹出)
```

---

## 6. 构建与打包

### 6.1 vcpkg 清单 (`vcpkg.json`)

```json
{
  "name": "keyextractor",
  "version": "1.0.0",
  "dependencies": [
    "qtbase",
    "qtwidgets",
    "qtwindows",
    "7zip"
  ]
}
```

### 6.2 CMake 结构 (`CMakeLists.txt`)

```cmake
cmake_minimum_required(VERSION 3.21)
project(KeyExtractor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# 核心库
add_library(keyextractor_core STATIC
    src/core/ArchiveManager.cpp
    src/core/ArchiveItem.cpp
    src/core/ExtractionWorker.cpp
    src/engine/SevenZipEngine.cpp
)

# 主可执行文件
add_executable(KeyExtractor WIN32
    src/main.cpp
    src/app/Application.cpp
    src/app/CommandLineParser.cpp
    src/ui/MainWindow.cpp
    src/ui/FileTreeView.cpp
    src/ui/FileTreeModel.cpp
    src/ui/ExtractPathDialog.cpp
    src/ui/ExtractionDialog.cpp
    src/ui/PasswordDialog.cpp
    src/ui/OverwriteDialog.cpp
    src/ui/ErrorDialog.cpp
    src/resources/KeyExtractor.rc
)

# Qt 翻译文件
qt6_add_translations(KeyExtractor
    TS_FILES resources/translations/keyextractor_zh_CN.ts
    QM_FILES_OUTPUT_VARIABLE qm_files
)


target_link_libraries(KeyExtractor PRIVATE
    keyextractor_core
    Qt6::Core Qt6::Gui Qt6::Widgets
)

# MSIX 打包（使用 Windows Application Packaging Project 或 MSIX Packaging Tool）
```

### 6.3 MSIX 打包要点

1. **`AppxManifest.xml`** 声明能力：`runFullTrust`（如需）、`fileTypeAssociation`
2. **数字签名**：商店上架需要受信任的代码签名证书
3. **注册表虚拟化**：MSIX 对注册表写入进行重定向；文件关联和右键菜单应优先通过清单声明实现
4. **Package Support Framework (PSF)**：可用于处理注册表重定向、文件系统修复等兼容性问题
5. **WinRT API**：某些 Shell 集成功能需要调用 WinRT API（如 `Windows.ApplicationModel.AppExtensions`）

---

## 7. 安全与合规

### 7.1 Microsoft Store 上架要求
- ✅ 不使用 `HKEY_LOCAL_MACHINE` 不受控注册表写入
- ✅ 不使用 `%WINDIR%` / `%ProgramFiles%` 以外的非沙箱路径写入
- ✅ 不使用内核驱动
- ✅ 不使用未公开的 Windows API
- ✅ 清楚声明文件类型关联
- ✅ 提交前通过 Windows App Certification Kit (WACK) 验证

### 7.2 国际化

- 支持英文（默认）和简体中文
- 通过 Qt 翻译系统实现：`.ts` 源文件 → `lrelease` → `.qm` 二进制
- 运行时根据系统语言自动选择

### 7.3 用户隐私
- 不解压时扫描或上传用户文件
- 不创建隐藏的持久化进程
- 文件关联与右键菜单由 MSIX 声明驱动，卸载即清除

---

## 8. 开发路线图

| 阶段 | 内容 | 产出 |
|------|------|------|
| P0 — 基础骨架 | CMake + vcpkg 搭建、Qt6 主窗口空壳、`main.cpp` 入口 | 可编译运行的空窗口 |
| P1 — 解压内核 | 集成 7zip 引擎、`ArchiveManager`、`FileTreeModel` | 可从命令行解压，终端输出文件列表 |
| P2 — 主 UI | `FileTreeView`、右键菜单、五个对话框、状态栏 | 完整可交互的主界面 |
| P3 — Shell 集成 | `AppxManifest.xml` 声明文件关联与右键菜单 | MSIX 安装后自动注册 |
| P4 — MSIX 打包 | `AppxManifest.xml`、打包脚本、签名 | 可安装的 MSIX 包 |
| P5 — 测试 | WACK 验证、压缩格式覆盖测试、边界测试 | 通过认证、无崩溃 |
| P6 — 上架准备 | 商店列表页素材、隐私策略、EULA | 可提交到 Microsoft Store |

---


## 9. 开放问题 (Open Questions)

1. **7zip vcpkg port**：当前 vcpkg 的 `7zip` port 是否提供可链接的库？还是仅为 CLI 工具？这影响 `SevenZipEngine` 的实现方式。
2. **密码支持**：是否需要在 v1.0 支持加密压缩包？如需要，UI 需要增加密码输入流程。
3. **多选解压行为**：用户多选文件后点击"解压到..."，是逐个弹出目录选择框，还是统一一个对话框批量解压到同一目录？
4. **国际化**：是否需要多语言支持（中文 / English）？

> **已决策**：
> - 右键菜单不做动态控制，单选多选均显示两条菜单
> - Shell 集成完全通过 `AppxManifest.xml` 声明，不写注册表代码
> - 不引入 ShellRegistrar / COM 扩展 / IExplorerCommand 等模块
> - 文件关联和右键菜单均通过 MSIX 标准能力实现
> - 支持加密压缩包密码输入（`PasswordDialog`）
> - 支持文件覆盖策略选择（`OverwriteDialog`）
> - **7zip 集成方式**：通过 vcpkg 编译出的可链接动态库（非 CLI 调用）
> - **密码支持**：需要，已设计 `PasswordDialog`（5.5 节）
> - **多选解压行为**：系统右键"解压到..."弹出单个目录选择对话框，代码中串行逐个解压
> - **多选打开行为**：系统右键"用 KeyExtractor 打开"启动多个独立进程，每进程一个主窗口，最多打开前 10 个文件
> - **国际化**：支持英文和简体中文（Qt 翻译系统 `.ts`/`.qm` 文件）

---

> **下一步**：请审阅以上设计文档，标记需要修改的地方，或对开放问题给出决策。确认后我们可以进入 P0 — 基础骨架编码阶段。

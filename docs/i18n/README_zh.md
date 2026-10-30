# (本 README 由 CURSOR AI 创建，包括代码在内的其他所有内容均由人工编写。)

# Wilfred

**网站:** https://ajakfial.github.io/Wilfred/

Wilfred 是一个快速、轻量、跨平台的桌面搜索引擎和应用程序启动器。它被设计为操作系统的深度搜索层：文件、文件夹、应用程序、浏览器集成、计算器和过滤查询，均可从单一可调用的搜索栏完成。

**快速。深度。模块化。低资源。跨平台。可扩展。**

## 功能

* 带写入前日志的增量、持久化文件名/路径/元数据索引
* 文件系统监视器 + 重新扫描回退
* 模糊匹配、首字母缩写、令牌/路径排名、新近性和频率信号
* 打字错误安全: Damerau (转置感知) near-miss 匹配 + `您是不是要找` 修正
* 内联自动完成: 幽灵补全 (`→` 接受) + 历史、命令、索引的前 N 个候选
* 上下文感知排名 (最近文件夹、文件类型、一天中的时间、会话查询)
* 剪贴板作为次要来源 (文本匹配、复制路径、剪贴板历史)
* 持久化剪贴板管理器 (`clips [query]`, 固定, `clips clear`, `clips url|email|path|code|ip`)
* 源代码、配置、文本文档的内容索引
* 真实文档文本提取 (PDF、Office、RTF、HTML) 到内容索引
* 可选语义搜索: 本地嵌入 (llama.cpp 服务器/模型或内置哈希) + HNSW 向量索引 (`vectors.bin` 位于 WAL/快照旁) 与三元软匹配合并 (`providers.semantic`, `embedding.*`)
* 可选 AI 助手 (`ai ...`/`ask ...`, 屏幕感知 `ai see ...`) 使用您自己的密钥: OpenAI, Anthropic, Gemini, Groq
* 带文件夹、 `{date}`/`{clipboard}`/`{query}` 占位符的片段, 可选全局缩写
* 快速笔记 + 待办事项 (`note ...`, `todo ...`, 数据目录持久化)
* 番茄钟 / 倒计时计时器 + 秒表 (`timer`, `pomodoro`, `stopwatch`)
* 丰富的文件预览 (文档、CSV、图像、文件夹) 在覆盖层 (`F3`) + `wilfred preview`
* 日历 (`.ics`)、联系人 (`.vcf`)、笔记 (Markdown) 来源 + 可选通过 `tesseract` 的 OCR 索引 (`sources.*`)
* 跨平台覆盖层 (Windows WebView2 置顶, macOS 悬浮面板, Linux WebKitGTK 自动 X11 画布回退, Wayland 合成器上的层外壳锚点, 全部置顶/跳过任务栏) 带 `Ctrl`/`⌘`+`K` 动作弹出框, 两个 Linux 后端均有匹配快捷键
* 浏览器书签、历史、打开标签页 (`bm`, 提供商支持)
* Windows 上通过 USN 日志即时 NTFS 全磁盘枚举
* 官方操作系统索引联合 (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` 填补 Wilfred 索引遗漏的空白 (`os <query>` 强制, 重复项去重)
* 覆盖层预览窗格 (`F3`) + `wilfred preview`, 链式结果动作 (`open+copy_path`) + 命名多步工作流 (`workflows:`)
* 天气、时间、磁盘、内存、CPU、进程、电池、窗口、计时器、笔记、媒体、网络、布局等迷你结果
* 窗口管理 (`windows`, `minimize`/`maximize`/`close window`, 吸附) + 保存的窗口布局 (`layout save`, `layout`)
* 屏幕感知 AI (`ai see ...` 发送屏幕截图作为视觉输入给提供商)
* 快速笔记现在作为真实 Markdown 文件存在, 既可作为卡片搜索也可通过笔记搜索
* 系统级自动化: 额外全局热键 (`hotkeys:`), `wilfred workflow` / `wilfred exec` CLI 触发器
* 搜索宏 (`!yt`, `gh`, `wiki`, 配置中的自定义模板) + 参数化快捷链接 (`quicklinks:` 含 `{1}`/`{*}`/`{query}`)
* 内置文件操作之上的按应用额外上下文操作 (`app_actions:`)
* 大文件查找器 + 索引中的重复候选 (`large`, `dupes`)
* 文件转换 (`convert song.wav to mp3`, `convert photo.bmp to png`: 原生 WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg 用于 mp3/ogg/flac/jpg/webp) + 背景移除 (`bgremove photo.png` 至透明 PNG, 无需模型)
* 进程终止 (`kill <pid|name>`), 原生媒体键 (`media play/next/mute/vol` -- 无需安装助手), 网络工具 (`ping`, `dns`, `myip`), 语音转文字 (`transcribe` 通过 whisper CLI 或内置 spoke 引擎)
* 可组合过滤器 (`*.cpp in Projects`, `type:image`, `size:>10mb`, 命名作用域)
* 应用程序发现 (开始菜单 / `.app` 包 / `.desktop` 文件)
* 丰富文件操作 (复制路径变体, SHA-256 哈希, zip, 此处终端/编辑器, 新建文件/文件夹, 打开方式)
* 默认浏览器检测, URL 打开, 网络搜索回退
* 安全表达式解析器 (算术, 函数, 单位/货币/颜色/日期转换, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, 位, regex/url/jwt)
* 原生和 stdio 插件, 加上通过 provider 的额外搜索后端
* 带单位、货币、颜色、日期和小型开发工具的计算器
* 覆盖层中的系统命令 (`lock`, `sleep`, `shutdown`, `restart`, `logout`, 清空回收站)
* 系统切换 (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + 设置深层链接 (`settings wifi/bluetooth/sound/display/...`) -- Windows/macOS/Linux 真实 OS 后端, 移动/BSD 空白处诚实的不支持错误
* 包管理器搜索 (`winget firefox`, `brew switch`, `apt vlc` -- 回车安装, `Tab` 复制命令; `packages` 列出检测到的管理器)
* 设置 + 设置 (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) 和可选远程搜索后端 (`remotes:` -- 仅您的端点, 默认关闭; 每源 `headers:` + `max_results:`)
* 首次使用信任插件 (`plugins` 批准卡片, `wilfred plugin list/install/approve/revoke`, 可选 `plugins.registry` 索引, sha256 验证安装)
* 窗口平铺 (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + 显示器变更自动应用 (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* Android 应用 (Kotlin UI + 通过 NDK 的 C++ 核心 → `.apk`): 悬浮 W 按钮, 搜索栏, 相同索引/搜索/排名 -- 见 `docs/android.md`
* iOS 应用 (SwiftUI + 相同 C++ 核心 → 模拟器 `.app`): 与 Android 相同的搜索 UI (应用图标入口; iOS 禁止覆盖按钮), 仅沙箱索引 -- 见 `docs/ios.md`
* 本地备份/恢复和可选远程同步 (`backup`, `restore`, `sync-push`, `sync-pull`)
* 从其他启动器导入 (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- 见 `docs/import.md`
* 带验证、 `您是不是要找` 提示和人类可读错误的 YAML 配置
* 全局热键: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), 可配置
* 本地搜索历史 (可选; 可禁用或 `wilfred history-clear`)
* 覆盖层 UI + CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## 构建

需要 CMake 3.16+ 和 C++20 编译器 (MSVC, Apple Clang, 或 GCC)。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

在 Linux 上, X11 开发头文件启用覆盖层 (`libx11-dev`); 在 Wayland 会话中, 安装 `libgtk-layer-shell-dev` 时 WebKit 覆盖层额外成为原生 layer-shell 表面 (Sway/wlroots, KDE Plasma -- GNOME 没有 layer-shell 协议, 窗口保持普通 toplevel)。

## 运行

```bash
# 后台守护进程: 覆盖层, 索引器, 热键
wilfred

# 针对本地索引的一次性搜索
wilfred search firefox

# 搜索并打开顶部结果
wilfred launch firefox

# 立即索引配置的根目录
wilfred index

# 统计信息
wilfred status

# 从其他启动器导入设置
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
```

用户配置在首次运行时创建:

| 平台 | 配置 | 索引数据 |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

完整模式 (索引根目录, 排除项, 系统目录, 排名权重, 别名, 热键, 历史, 浏览器) 请参见 `config/wilfred.default.yml`。

## 从其他启动器迁移

`wilfred import` 将热键、自定义 Web 搜索 (作为 `macros:` + `quicklinks:`)、片段、别名、主题和默认搜索模板导入 `wilfred.yml`。默认合并; `--overwrite` 替换冲突, `--dry-run` 预览, 每次真实导入前写入 `.pre-import.bak` 备份。

| 操作系统 | 支持的来源 |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` 捆绑包), Raycast (快速链接 JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

请参阅 `docs/import.md` (和 `wilfred import --list`) 了解每个启动器的映射。

## 查询语言 (示例)

| 查询 | 含义 |
|---|---|
| `firefox` | 模糊文件/应用搜索 |
| `25 * 42` | 计算器 |
| `https://example.com` | 打开 URL |
| `? 猫` / `g 猫` | 默认浏览器中网络搜索 |
| `*.cpp in Projects` | 扩展名 + 目录过滤器 |
| `type:image name:logo` | 种类 + 文件名 |
| `content:widget` / `intext foo` | 搜索已索引文件内容 |
| `> notepad` | 命令 / 启动样式 |
| `scope:home 笔记` | 来自配置的命名目录组 |
| `天气` / `天气 北京` | 迷你卡片: 本地或城市预报 |
| `speedtest` | 实时 ping、下载和上传 (`speedtest again` 重新运行) |
| `时间` `磁盘` `磁盘使用` `内存` `cpu` | 时钟、驱动器、内存、处理器 |
| `进程 chrome` / `top` | 实时进程 CPU、内存、线程 (`kill <pid|name>` 终止) |
| `截图` / `截图 window` / `截图 region` | 全屏、窗口或区域捕获到 `Pictures/Wilfred` |
| `emoji 微笑` / `symbol 欧元` | 表情符号和符号选择器 (回车复制) |
| `100 usd 到 eur` / `fx 25 gbp jpy` | 货币转换 |
| `1 汤匙 到 g` / `1 杯 面粉 到 g` | 烹饪体积↔质量 (水默认, 可选配料) |
| `#ff5500` / `rgb(255, 85, 0)` / `颜色 coral` | 颜色转换 (hex / rgb / hsl) |
| `3pm est 到 pst` / `now in tokyo` / `tz london` | 时区 |
| `今天 + 7 天` / `2024-01-31 + 1 月` | 日期数学 |
| `uuid` / `base64 你好` / `sha256 abc` / `lorem 12` / `json {"a":1}` | 开发工具 (回车复制) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | 数字基数 + 位工具 |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | 正则测试器, URL 编解码, JWT 解码 |
| `timer 10m` / `pomodoro` / `stopwatch start` | 倒计时, 番茄钟预设, 秒表 |
| `note 买牛奶` / `notes` / `todo 发货` / `todos` | 快速笔记 + 待办事项 (`todo done 1`) |
| `media play` / `media next` / `media mute` | 原生媒体键 + 音量 (Win 媒体键; mac HID + Music/Spotify/VLC; Linux 原生 MPRIS D-Bus + wpctl/pactl/amixer, playerctl 可选) |
| `ping example.com` / `dns example.com` / `myip` | Ping, DNS 查找, 公共 IP |
| `large 10` / `dupes` | 索引中最大文件 + 重复候选 |
| `transcribe 讲话.mp3` / `dictate 10` | mp3/mp4 音频语音转文字, 麦克风听写 (`transcription.engine`: whisper CLI 或内置 spoke 引擎) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | 文件转换 (原生 PCM + BMP/PNG/PPM/TGA, ffmpeg 用于压缩) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | 背景移除至透明 PNG (自动角落或显式颜色) |
| `窗口 chrome` / `最小化 spotify` | 列出、切换、最小化、最大化、关闭、吸附窗口 |
| `layout save work` / `layout work` | 保存和恢复窗口布局 |
| `define resilient` / `thesaurus happy` | 离线定义 + 同义词 (回车复制) |
| `pins` / `pin firefox` / `unpin firefox` | 固定收藏夹 (通过 `ranking.pinned` 增强) |
| `event add 团队同步 | 明天 10am` | 创建日历事件 (写入 `.ics`) |
| `contact add Jane Doe jane@x.com 555-0100` | 创建联系人 (写入 `.vcf`) |
| `rename ./photos photo-{n}.jpg` | 批量重命名 (`{n}` `{name}` `{ext}`) |
| `template python foo in ./dir` | 从模板新建文件 (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | 按类型过滤的剪贴板历史 (text/url/email/path/code/ip/image) |
| `workflow 审查` / `run 审查` | 命名多步骤工作流 (`workflows` 列表) |
| `ql docs 你好` / `ticket:ABC-123` | 参数化快速链接 (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | 系统命令 |
| `wifi` / `wifi off` / `bluetooth toggle` | Wi-Fi + Bluetooth 状态和开/关 (回车应用) |
| `volume 50` / `volume mute` / `brightness 70` | 音量级别/静音 + 显示亮度 (回车应用) |
| `settings wifi` / `settings sound` / `settings` | 打开操作系统设置页面 (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | 首次运行状态 + 编辑/验证 wilfred.yml (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | 启动检查的新版本状态 (回车复制链接, `wilfred update` 安装) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | 就地更新/读取一个设置, 已验证 (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | 插件列表 + 信任批准 (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout work` | 平铺打开的窗口或应用保存的布局 (`wilfred tile`, 显示器变更时 `layouts.auto_apply`) |
| `winget firefox` / `brew switch` / `apt vlc` | 包搜索 -- 回车安装, `Tab` 复制命令 (`packages` 列出管理器) |
| `clip` / `clips` | 剪贴板和最近片段 |
| `!yt 猫` / `gh wilfred` | 搜索宏 (`macros` 列出) |

## 架构

平台 I/O 位于 `wilfred/platform/native.hpp` 之后。核心模块:

* `config` -- YAML 解析 + 模式验证
* `index` -- 字符串内部池, 紧凑记录, 倒排/三元索引, WAL
* `fs` -- 遍历器, 分类, 卷, 监视器
* `search` -- 模糊, 排名权重, 过滤器, 查询缓存
* `query` -- 分类 + 解释器
* `math` -- 递归下降计算器
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `plugin` -- 原生 `.dll`/`.so`/`.dylib` ABI 和 stdio 插件, 信任存储, 注册表索引
* `sync` -- 本地备份归档和可选远程推送/拉取
* `import` -- 跨操作系统启动器设置导入器 (热键, 搜索, 片段)
* `providers` -- 额外搜索后端注册表 (语义, 浏览器库, 日历/联系人/笔记, 远程, 包)

## 测试和基准测试

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # 或 build/Release/wilfred_tests.exe
./build/wilfred_bench
```

测试涵盖索引 CRUD、持久性/恢复、模糊/排名/过滤、数学、查询分类、配置验证、历史记录、路径、IPC 和应用程序发现。

## 性能

Wilfred 是一个没有供应商第三方库的单一 C++20 二进制文件 -- 所有内容 (JSON, YAML, mmap 包装器, CRC32) 均在树中实现, 因此无需额外安装 (Linux 覆盖层在有系统 X11/WebKit 库时使用, 加上用于原生 Wayland 锚定的 gtk-layer-shell, 并带有 X11 画布回退)。速度来自索引设计 (见 `docs/architecture.md`):

* 带字符串驻留池的紧凑内存映射记录存储 (内存中无重复路径/名称字符串)
* 三元组 + 令牌索引, 查询命中发布列表而非完整扫描
* 带定期快照和崩溃恢复的预写日志 -- 写入为追加, 读取从不因持久化而阻塞
* 增量、去抖动文件系统监视器更新代替重新扫描
* 有界结果集 (前 40 个) 且插件有每查询超时

`wilfred_bench` (`benches/bench_main.cpp`) 在合成 1K-100K 文件名数据集上测量热路径 (10 次计时迭代 + 1 次预热, 100K 插入为 5 次) 并报告最小/中位数/平均/P95/最大毫秒数:

| 基准测试 | 测量内容 |
|---|---|
| 字符串池: intern 100K | 实习吞吐量 (索引内存效率) |
| 分词器: 分词 100K | 文件名分词 |
| 索引插入: 1K / 10K / 100K | 批量索引构建 |
| 索引查询: 10K / 100K 记录 | 发布列表查找 |
| 模糊搜索: 10K 目标 | Damerau 感知评分吞吐量 |
| 排名: 10K 评分命中 | 加权评分 + 前 40 选择 |
| 过滤链: 10K 记录 | 扩展/种类过滤器评估 |
| 数学评估: 1K 表达式 | 计算器解析 + 求值 |
| 快照保存/加载: 10K | 持久化往返 |
| 内存报告: 100K 索引 | 索引占用空间核算 |

在您自己的硬件上运行后再引用数字 -- 结果取决于 CPU、磁盘和数据集形状:

```bash
cmake --build build --config Release --target wilfred_bench
./build/wilfred_bench          # 或 build/Release/wilfred_bench.exe
```

## 打包

推送 `v*.*.*` 标签运行 `.github/workflows/release.yml`, 它构建 Release 二进制文件并将每个平台的一个存档附加到 GitHub Release (详见 `docs/building.md#packaging-what-ci-does`):

| 作业 | 产物 |
|---|---|
| `windows-x64` | 便携 `.zip`, 已签名 `.msi` (WiX v3, 每用户, 无 UAC), Chocolatey `.nupkg` (嵌入式便携, 安装时无下载) |
| `linux-x64` | 便携 `.tar.gz` |
| `freebsd-x64` | 便携 `.tar.gz` (FreeBSD 发布作业; 其他 BSD 从源代码构建 -- 见 `docs/bsd.md`) |
| `macos-arm64` | 便携 `.tar.gz` |
| `android` | `.apk` + `.aab` -- 存在密钥库机密时签名, 否则 `-unsigned` 后缀 (见 `docs/android.md`) |
| `ios` | 未签名模拟器 `.zip` (见 `docs/ios.md`) |

桌面归档将二进制文件与 `ui/overlay/`, `README.md`, `config/wilfred.default.yml` 一起暂存。Windows 二进制文件始终通过 VERSIONINFO 携带发布者印章 `Wilfred Open Contributors`; Authenticode 信任来自配置时的 PFX 或 Azure Trusted Signing 机密 (参见 `docs/signing.md`, `docs/installer.md`)。

## 生产部署

* **运行守护进程, 而非一次性命令。** `wilfred daemon` (裸 `wilfred`) 是长期运行的进程: 索引器 + 监视器 + 热键 + 覆盖层 + IPC 服务器。作为登录项 / 自动启动条目运行 (`wilfred autostart enable` 在 Windows/macOS/Linux/BSD 上注册此二进制文件); 托盘图标 (Windows) 退出它。CLI 子命令 (`search`, `launch`, `status`, ...) 在可用时通过本地 IPC 套接字与正在运行的守护进程通信 (见 `docs/cli.md`, `docs/ipc-and-api.md`)。
* **配置即代码。** 首次运行写入经过验证的 `wilfred.yml` (上表位置); 无效键会导致人类可读的错误和 `Did you mean` 提示, 绝不静默默认值。对于车队, 分发基准 `wilfred.yml` (根目录, 排除项, 权重, 别名 -- 完整架构见 `docs/configuration.md` 和 `config/wilfred.default.yml`) 并让用户在本地覆盖。
* **数据与灾难恢复。** 索引、历史记录、片段、笔记和日志位于每个操作系统的数据目录中 (见上表)。`backup` / `restore` 移动机器状态; `sync-push` / `sync-pull` 复制它 (见 `docs/sync-and-backup.md`)。在需要之前测试恢复。
* **观察它。** `wilfred status` 报告索引健康状况; `logging.*` 键控制日志详细程度/保留; 可选本地 HTTP API (`api.*`, 认证) 为仪表板和自动化暴露 `/search`, `/status`, `/index`, `/launch` 以及备份/同步端点。
* **隐私姿态。** 本地优先: 无账户, 无遥测, 除非您启用 (天气/AI/同步) 否则无网络调用。策略要求时通过 `history.*` 禁用或限制历史记录; AI 提供商需要用户自己的 API 密钥 (`ai.*`, 默认关闭)。
* **移动端为实验性 (自 v46.5.8 起)。** Android (`.apk`) 和 iOS (模拟器 `.app`) 构建可安装并运行, 但因等待进一步测试和开发而尚未准备好作为日常驱动程序 -- 优先部署桌面端 (见 `docs/android.md`, `docs/ios.md`)。
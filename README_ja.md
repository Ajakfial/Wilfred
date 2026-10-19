# (この README は CURSOR AI によって作成されたものです。コードを含むそれ以外のすべては人間の手によって書かれています。)

# Wilfred

**ウェブサイト:** https://ajakfial.github.io/Wilfred/

Wilfred は、高速、軽量、クロスプラットフォームなデスクトップ検索エンジンおよびアプリケーション ランチャーです。ファイル、フォルダ、アプリケーション、ブラウザ統合、電卓、フィルタリングされたクエリを、単一の呼び出し可能な検索バーから実行するための、OS の深い検索レイヤーとして設計されています。

**高速。深層。モジュール式。低リソース。クロスプラットフォーム。拡張可能。**

## 機能

* インクリメンタル、永続的なファイル名/パス/メタデータ インデックス (Write-Ahead Log 付き)
* ファイルシステム ウォッチャー (ReadDirectoryChangesW, FSEvents, inotify) と再スキャン フォールバック
* ファジーマッチング、頭字語、トークン/パス ランキング、新規性と頻度のシグナル
* タイポセーフ: Damerau (転置対応) near-miss マッチング (名前、トークン、パス) + `もしかして` 修正 (コマンド、履歴、ファイル名)
* インライン オートコンプリート: ゴースト補完 (`→` で受け入れ) + 履歴、コマンド、インデックスからのトップ N 候補
* コンテキスト認識ランキング (最近のフォルダ、ファイル タイプ、時間帯、セッション クエリ)
* クリップボードをセカンダリ ソースとして利用 (テキスト一致、コピーされたパス、クリップ履歴)
* 永続的なクリップボード マネージャー (`clips [query]`, pin, `clips clear`, `clips url|email|path|code|ip`)
* ソース、設定、テキスト ドキュメントのコンテンツ インデックス作成
* 実際のドキュメント テキスト抽出 (PDF、Office、RTF、HTML) をコンテンツ インデックスに
* オプションのセマンティック検索: ローカル埋め込み (llama.cpp サーバー/モデルまたは組み込みハッシュ) + HNSW ベクトル インデックス (`vectors.bin` を WAL/スナップショットの隣に配置) をトリグラム ソフト マッチとマージ (`providers.semantic`, `embedding.*`)
* オプションの AI アシスタント (`ai ...`/`ask ...`, 画面認識 `ai see ...`) で独自のキーを使用: OpenAI, Anthropic, Gemini, Groq
* フォルダ、 `{date}`/`{clipboard}`/`{query}` プレースホルダ、および任意のアプリでのオプションのグローバル略語を持つスニペット
* クイック ノート + ToDo (`note ...`, `todo ...`, データ ディレクトリで持続)
* ポモドーロ / カウントダウン タイマー + ストップウォッチ (`timer`, `pomodoro`, `stopwatch`)
* リッチ ファイル プレビュー (ドキュメント、CSV、画像、フォルダ) オーバーレイ内 (`F3`) + `wilfred preview`
* カレンダー (`.ics`)、連絡先 (`.vcf`)、ノート (Markdown) ソース + オプションの OCR インデックス作成 via `tesseract` (`sources.*`)
* クロスプラットフォーム オーバーレイ (Windows WebView2 最前面、macOS フローティング パネル, Linux WebKitGTK (自動 X11 キャンバス フォールバック付き), Wayland コンポジターでのレイヤー シェル アンカー, すべて最前面/スキップ タスクバー) と `Ctrl`/`⌘`+`K` アクション ポップオーバー + 両 Linux バックエンドでの一致するショートカット
* ブラウザのブックマーク、履歴、開いているタブ (`bm`, プロバイダー支援)
* Windows で USN ジャーナルによる即時 NTFS フルディスク列挙
* 公式 OS インデックス フェデレーション (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` が Wilfred のインデックスの欠落を補完 (`os <query>` で強制、重複は除外)
* オーバーレイ内のプレビュー ペイン (`F3`) + `wilfred preview`, チェーン化された結果アクション (`open+copy_path`) + 名前付きマルチステップ ワークフロー (`workflows:`)
* 天気、時刻, ディスク, RAM, CPU, プロセス, バッテリー, ウィンドウ, タイマー, メモ, メディア, ネットワーク, レイアウトなどのミニ結果
* ウィンドウ管理 (`windows`, `minimize`/`maximize`/`close window`, スナップ) + 保存済みウィンドウ レイアウト (`layout save`, `layout`)
* 画面認識 AI (`ai see ...` はスクリーンショットをビジョン入力としてプロバイダーに送信)
* クイック ノートが実際の Markdown ファイルとして存在し、カードとしてもノート検索でも検索可能
* システムワイド オートメーション: 追加グローバル ホットキー (`hotkeys:`), `wilfred workflow` / `wilfred exec` CLI トリガー
* 検索マクロ (`!yt`, `gh`, `wiki`, プラス設定のカスタム テンプレート) + パラメータ化クイックリンク (`quicklinks:` で `{1}`/`{*}`/`{query}`)
* ビルトイン ファイル アクションの上に追加のアプリごとのコンテキスト アクション (`app_actions:`)
* 大きなファイルの検索 + インデックスからの重複候補 (`large`, `dupes`)
* ファイル変換 (`convert song.wav to mp3`, `convert photo.bmp to png`: ネイティブ WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg で mp3/ogg/flac/jpg/webp) + 背景除去 (`bgremove photo.png` を透過 PNG に, モデル不要)
* プロセス キラー (`kill <pid|name>`), ネイティブ メディア キー (`media play/next/mute/vol` -- ヘルパー インストール不要), ネットワーク ツール (`ping`, `dns`, `myip`), 音声認識 (`transcribe` via whisper CLI または組み込み spoke エンジン)
* 組み合わせ可能なフィルター (`*.cpp in Projects`, `type:image`, `size:>10mb`, 名前付きスコープ)
* アプリケーション検出 (スタート メニュー / `.app` バンドル / `.desktop` ファイル)
* 豊富なファイル アクション (コピー パス フレーバー, SHA-256 ハッシュ, zip, ターミナル/エディタをここで開く, 新規ファイル/フォルダ, Open With)
* デフォルト ブラウザ検出, URL 開く, Web 検索フォールバック
* 安全な式パーサー (算術, 関数, 単位/通貨/色/日付変換, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, ビット, regex/url/jwt)
* ネイティブおよび stdio プラグイン, およびプロバイダ経由の追加検索バックエンド
* 単位, 通貨, 色, 日付, および小さな開発ユーティリティを備えた電卓
* オーバーレイからのシステム コマンド (`lock`, `sleep`, `shutdown`, `restart`, `logout`, ゴミ箱を空にする)
* システム トグル (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + 設定ディープリンク (`settings wifi/bluetooth/sound/display/...`) -- Windows/macOS/Linux で本物の OS バックエンド, モバイル/BSD の隙間では正直な unsupported エラー
* パッケージ マネージャ検索 (`winget firefox`, `brew switch`, `apt vlc` -- Enter でインストール, `Tab` でコマンド コピー; `packages` で検出されたマネージャをリスト)
* 設定 + セットアップ (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) およびオプトイン リモート検索バックエンド (`remotes:` -- 独自のエンドポイントのみ, デフォルトでオフ; ソースごとに `headers:` + `max_results:`)
* トラスト オン ファースト ユース プラグイン (`plugins` 承認カード, `wilfred plugin list/install/approve/revoke`, オプションの `plugins.registry` インデックス, sha256 検証済みインストール)
* ウィンドウ タイリング (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + モニター変更時の自動適用 (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* Android アプリ (Kotlin UI + C++ コア via NDK → `.apk`): 浮動 W ボタン, 検索バー, 同じインデックス/検索/ランキング -- `docs/android.md` を参照
* iOS アプリ (SwiftUI + 同じ C++ コア → シミュレータ `.app`): Android と同じ検索 UI (アプリアイコン エントリ; iOS はオーバーレイ ボタンを禁止), サンドボックスのみのインデックス -- `docs/ios.md` を参照
* ローカル バックアップ/リストアとオプションのリモート同期 (`backup`, `restore`, `sync-push`, `sync-pull`)
* 他のランチャーからのインポート (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- `docs/import.md` を参照
* 検証, `もしかして` ヒント, 人間が読めるエラーを備えた YAML 設定
* グローバル ホットキー: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), 設定可能
* ローカル検索履歴 (オプション; 無効化可能 または `wilfred history-clear`)
* オーバーレイ UI + CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## ビルド

CMake 3.16+ と C++20 コンパイラ (MSVC, Apple Clang, または GCC) が必要です。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Linux では、X11 開発ヘッダーでオーバーレイが有効になります (`libx11-dev`); Wayland セッションでは、`libgtk-layer-shell-dev` がインストールされている場合、WebKit オーバーレイがネイティブ レイヤー シェル サーフェスになります (Sway/wlroots, KDE Plasma -- GNOME にはレイヤー シェル プロトコルがなく, ウィンドウは通常のトップレベルのまま)。

## 実行

```bash
# バックグラウンド デーモン: オーバーレイ, インデクサー, ホットキー
wilfred

# ローカル インデックスに対するワンショット検索
wilfred search firefox

# トップ結果を検索して開く
wilfred launch firefox

# 設定されたルートを今すぐインデックス作成
wilfred index

# 統計
wilfred status

# 他のランチャーから設定をインポート
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
```

ユーザー設定は初回実行時に作成されます:

| プラットフォーム | 設定 | インデックス データ |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

完全なスキーマ (インデックス ルート, 除外, システム ディレクトリ, ランキング重み, エイリアス, ホットキー, 履歴, ブラウザ) は `config/wilfred.default.yml` を参照してください。

## 他のランチャーからの移行

`wilfred import` はホットキー, カスタム Web 検索 ( `macros:` + `quicklinks:` として), スニペット, エイリアス, テーマ, デフォルトの検索テンプレートを `wilfred.yml` に取り込みます。マージがデフォルトで, `--overwrite` で上書き, `--dry-run` でプレビュー, 実際のインポートの前に `.pre-import.bak` バックアップが作成されます。

| OS | サポートされるソース |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` バンドル), Raycast (クイックリンク JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

詳細と `wilfred import --list` によるランチャーごとのマッピングは `docs/import.md` を参照してください。

## クエリ言語 (例)

| クエリ | 意味 |
|---|---|
| `firefox` | ファジーファイル/アプリ検索 |
| `25 * 42` | 電卓 |
| `https://example.com` | URL を開く |
| `? 猫` / `g 猫` | 既定のブラウザで Web 検索 |
| `*.cpp in Projects` | 拡張子 + ディレクトリ フィルタ |
| `type:image name:logo` | 種類 + ファイル名 |
| `content:widget` / `intext foo` | インデックス済みファイル コンテンツの検索 |
| `> notepad` | コマンド / 起動スタイル |
| `scope:home メモ` | 設定からの名前付きディレクトリ グループ |
| `天気` / `天気 東京` | ミニ カード: ローカルまたは都市の予報 |
| `speedtest` | ライブ ping, ダウンロード, アップロード (`speedtest again` で再実行) |
| `時間` `ディスク` `ディスク使用` `ram` `cpu` | 時計, ドライブ, メモリ, プロセッサ |
| `プロセス chrome` / `top` | プロセス CPU, RAM, スレッドのライブ表示 (`kill <pid|name>` で終了) |
| `スクリーンショット` / `スクリーンショット window` / `スクリーンショット region` | フルスクリーン, ウィンドウ, または領域を `Pictures/Wilfred` に保存 |
| `emoji smile` / `symbol euro` | 絵文字と記号ピッカー (Enter でコピー) |
| `100 usd を eur に` / `fx 25 gbp jpy` | 通貨換算 |
| `1 大さじ を g に` / `1 カップ 小麦粉 を g に` | 料理 体積⇔質量 (水をデフォルト, オプションで食材指定) |
| `#ff5500` / `rgb(255, 85, 0)` / `色 coral` | 色変換 (hex / rgb / hsl) |
| `15時 est を pst に` / `now in tokyo` / `tz london` | タイムゾーン |
| `今日 + 7 日` / `2024-01-31 + 1 月` | 日付計算 |
| `uuid` / `base64 こんにちは` / `sha256 abc` / `lorem 12` / `json {"a":1}` | 開発ユーティリティ (Enter でコピー) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | 数値基数 + ビット ツール |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | 正規表現テスタ, URL コーデック, JWT デコード |
| `timer 10m` / `pomodoro` / `stopwatch start` | カウントダウン, Pomodoro プリセット, ストップウォッチ |
| `note 牛乳を買う` / `notes` / `todo 送る` / `todos` | クイック ノート + ToDo (`todo done 1`) |
| `media play` / `media next` / `media mute` | ネイティブ メディア キー + 音量 (Win メディア キー; mac HID + Music/Spotify/VLC; Linux ネイティブ MPRIS D-Bus + wpctl/pactl/amixer, playerctl オプション) |
| `ping example.com` / `dns example.com` / `myip` | Ping, DNS ルックアップ, パブリック IP |
| `large 10` / `dupes` | インデックスから最大ファイル + 重複候補 |
| `transcribe 話.mp3` / `dictate 10` | mp3/mp4 音声の音声認識, マイク口述 (`transcription.engine`: whisper CLI または組み込み spoke エンジン) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | ファイル変換 (ネイティブ PCM + BMP/PNG/PPM/TGA, ffmpeg で圧縮形式) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | 背景除去で透明 PNG 化 (自動コーナーまたは明示的色) |
| `ウィンドウ chrome` / `最小化 spotify` | ウィンドウの列挙, 切り替え, 最小化, 最大化, 閉じる, スナップ |
| `layout save work` / `layout work` | ウィンドウ レイアウトの保存と復元 |
| `define resilient` / `thesaurus happy` | オフライン定義 + 類義語 (Enter でコピー) |
| `pins` / `pin firefox` / `unpin firefox` | ピン留めお気に入り (`ranking.pinned` でブースト) |
| `event add チーム同期 | 明日 10時` | カレンダー イベント作成 (`.ics` を書き込み) |
| `contact add Jane Doe jane@x.com 555-0100` | 連絡先作成 (`.vcf` を書き込み) |
| `rename ./photos photo-{n}.jpg` | 一括リネーム (`{n}` `{name}` `{ext}` 使用) |
| `template python foo in ./dir` | テンプレートから新規ファイル (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | タイプ別フィルタリングされたクリップボード履歴 (text/url/email/path/code/ip/image) |
| `workflow review` / `run review` | 名前付きマルチステップ ワークフロー (`workflows` で一覧) |
| `ql docs こんにちは` / `ticket:ABC-123` | パラメータ化クイックリンク (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | システム コマンド |
| `wifi` / `wifi off` / `bluetooth toggle` | Wi-Fi + Bluetooth ステータスとオン/オフ (Enter で適用) |
| `volume 50` / `volume mute` / `brightness 70` | 音量レベル/ミュート + 画面輝度 (Enter で適用) |
| `settings wifi` / `settings sound` / `settings` | OS 設定ページを開く (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | 初回実行ステータス + wilfred.yml の編集/検証 (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | 起動時チェックでの新バージョン状況 (Enter でリンクコピー, `wilfred update` でインストール) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | その場で設定を更新/読み取り, 検証済み (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | プラグイン リスト + 信頼承認 (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout work` | 開いているウィンドウをタイリング, または保存済みレイアウトを適用 (`wilfred tile`, モニタ変更時の `layouts.auto_apply`) |
| `winget firefox` / `brew switch` / `apt vlc` | パッケージ検索 -- Enter でインストール, `Tab` でコマンドコピー (`packages` でマネージャ一覧) |
| `clip` / `clips` | クリップボードと最近のクリップ |
| `!yt 猫` / `gh wilfred` | 検索マクロ (`macros` で一覧) |

## アーキテクチャ

プラットフォーム I/O は `wilfred/platform/native.hpp` の背後にあります。コア モジュール:

* `config` -- YAML パース + スキーマ検証
* `index` -- 文字列インターン プール, コンパクト レコード, 逆/トリグラム インデックス, WAL
* `fs` -- ウォーカー, 分類, ボリューム, ウォッチャー
* `search` -- ファジー, ランキング重み, フィルタ, クエリ キャッシュ
* `query` -- 分類 + インタプリタ
* `math` -- 再帰下降電卓
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `plugin` -- ネイティブ `.dll`/`.so`/`.dylib` ABI と stdio プラグイン, トラスト ストア, レジストリ インデックス
* `sync` -- ローカル バックアップ アーカイブとオプションのリモート push/pull
* `import` -- クロス OS ランチャー設定インポーター (ホットキー, 検索, スニペット)
* `providers` -- 追加検索バックエンドのレジストリ (セマンティック, ブラウザ ライブラリ, カレンダー/連絡先/ノート, リモート, パッケージ)

## テストとベンチマーク

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # または build/Release/wilfred_tests.exe
./build/wilfred_bench
```

テストはインデックス CRUD, 永続性/リカバリ, ファジー/ランキング/フィルタ, 数学, クエリ分類, 設定検証, 履歴, パス, IPC, アプリケーション検出をカバーします。

## パフォーマンス

Wilfred はサードパーティ製ライブラリをベンダー同梱しない単一の C++20 バイナリです -- すべて (JSON, YAML, mmap ラッパー, CRC32) がツリー内で実装されているため, 追加のインストールは不要です (Linux オーバーレイはシステムの X11/WebKit ライブラリを使用し, Wayland ネイティブ アンカー用に gtk-layer-shell, X11 キャンバス フォールバックあり)。速度はインデックス設計から来ます (詳細は `docs/architecture.md` を参照):

* 文字列インターン プールを持つコンパクトなメモリマップ レコード ストア (RAM に重複パス/名前文字列なし)
* トリグラム + トークン インデックスにより, クエリがフル スキャンではなく投稿リストにヒット
* 定期的なスナップショットとクラッシュ リカバリを備えた Write-Ahead Log -- 書き込みは追記のみ, 読み取りは持続性でブロックされない
* 再スキャンの代わりに増分, デバウンスされたファイルシステム ウォッチャー更新
* プラグインでのクエリごとのタイムアウト付きの境界付き結果セット (トップ 40)

`wilfred_bench` (`benches/bench_main.cpp`) は合成 1K-100K ファイル名データセット (10 回計測イテレーション + 1 ウォームアップ, 100K 挿入では 5 回) でホット パスを測定し, 最小/中央値/平均/P95/最大ミリ秒を報告します:

| ベンチマーク | 測定内容 |
|---|---|
| String pool: intern 100K | インターニング スループット (インデックス メモリ効率) |
| Tokenizer: tokenize 100K | ファイル名トークン化 |
| Index insert: 1K / 10K / 100K | 一括インデックス構築 |
| Index query: 10K / 100K records | ポスティング リスト ルックアップ |
| Fuzzy search: 10K targets | Damerau 対応スコアリング スループット |
| Ranking: 10K scored hits | 重みスコアリング + トップ 40 選択 |
| Filter chain: 10K records | 拡張子/種類フィルタ評価 |
| Math evaluation: 1K expressions | 計算機パース + 評価 |
| Snapshot save/load: 10K | 永続化ラウンドトリップ |
| Memory reporting: 100K index | インデックス フットプリント会計 |

実際のハードウェアで実行してから数値を引用してください -- 結果は CPU, ディスク, データセット形状に依存します:

```bash
cmake --build build --config Release --target wilfred_bench
./build/wilfred_bench          # または build/Release/wilfred_bench.exe
```

## パッケージング

`v*.*.*` タグをプッシュすると `.github/workflows/release.yml` が実行され, Release バイナリをビルドし, プラットフォームごとに 1 つのアーカイブを GitHub Release に添付します (詳細は `docs/building.md#packaging-what-ci-does` を参照):

| ジョブ | アーティファクト |
|---|---|
| `windows-x64` | ポータブル `.zip`, 署名済み `.msi` (WiX v3, ユーザー単位, UAC 不要), Chocolatey `.nupkg` (埋め込みポータブル, インストール時にダウンロードなし) |
| `linux-x64` | ポータブル `.tar.gz` |
| `freebsd-x64` | ポータブル `.tar.gz` (FreeBSD リリース ジョブ; 他の BSD はソースからビルド -- `docs/bsd.md` 参照) |
| `macos-arm64` | ポータブル `.tar.gz` |
| `android` | `.apk` + `.aab` -- キーストア シークレットが存在する場合は署名済み, それ以外は `-unsigned` サフィックス (詳細は `docs/android.md`) |
| `ios` | 未署名シミュレータ `.zip` (詳細は `docs/ios.md`) |

デスクトップ アーカイブには `ui/overlay/`, `README.md`, `config/wilfred.default.yml` と共にバイナリが含まれます。Windows バイナリには常に VERSIONINFO 経由で `Wilfred Open Contributors` のパブリッシャー スタンプが付きます; Authenticode 信頼は, 設定時に PFX または Azure Trusted Signing シークレットから得られます (詳細は `docs/signing.md`, `docs/installer.md` を参照)。

## 本番環境へのデプロイ

* **デーモンを実行し, ワンショットを実行しない。** `wilfred daemon` (素の `wilfred`) は長寿命プロセス: インデクサ + ウォッチャー + ホットキー + オーバーレイ + IPC サーバー。ログイン アイテム / 自動起動エントリとして実行 (`wilfred autostart enable` でこのバイナリを Windows/macOS/Linux/BSD に登録); トレイ アイコン (Windows) で終了。CLI サブコマンド (`search`, `launch`, `status`, ...) は, 利用可能な場合, ローカル IPC ソケット経由で実行中のデーモンと通信します (詳細は `docs/cli.md`, `docs/ipc-and-api.md` を参照)。
* **設定はコード。** 初回実行で検証済みの `wilfred.yml` が作成されます (上記の場所の表を参照); 無効なキーは, 人間が読めるエラーと `Did you mean` ヒントで失敗し, サイレントなデフォルトはありません。フリート展開では, ベースラインの `wilfred.yml` (ルート, 除外, 重み, エイリアス -- 完全なスキーマは `docs/configuration.md` と `config/wilfred.default.yml`) を配布し, ユーザーがローカルでオーバーライドできるようにします。
* **データと DR.** インデックス, 履歴, クリップ, ノート, ログは OS ごとのデータ ディレクトリにあります (上記の表を参照)。`backup` / `restore` でマシンの状態を移動し, `sync-push` / `sync-pull` で複製します (詳細は `docs/sync-and-backup.md` を参照)。必要になる前にリストアをテストしてください。
* **監視する。** `wilfred status` でインデックスの健全性を報告; `logging.*` キーでログの詳細度/保持を制御; オプションのローカル HTTP API (`api.*`, 認証付き) は `/search`, `/status`, `/index`, `/launch`, バックアップ/同期エンドポイントをダッシュボードと自動化のために公開します。
* **プライバシー ポリシー.** ローカル ファースト: アカウントなし, テレメトリなし, 有効にしない限りネットワーク呼び出しなし (天気/AI/同期). ポリシーで必要な場合は `history.*` で履歴を無効化または制限; AI プロバイダにはユーザー独自の API キーが必要 (`ai.*`, デフォルトはオフ)。
* **モバイルは実験的 (v23.0.5 現在).** Android (`.apk`) と iOS (シミュレータ `.app`) ビルドはインストール/実行可能ですが, さらなるテストと開発を待つため, 日常利用の準備はできていません -- まずデスクトップを展開してください (詳細は `docs/android.md`, `docs/ios.md` を参照)。
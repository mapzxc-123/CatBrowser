# CatBrowser

ネイティブ API と OS 標準の WebView を直接制御する、超軽量のブラウザゲーム用ランチャーです。
Electron / Tauri / Chromium の自前ビルドは使用しません。

**バージョン 1.4** では、URL / 検索のモード切替、DuckDuckGo / Google / Bing の切替、
戻る / 進む / リロード / 全画面ボタン、ユーザー辞書、ダウンロード先設定、
検索履歴、テーマ切替を追加しました。1.3 のオフライン翻訳とアイドル時の完全停止は維持しています。
過去の exe は `artifacts/windows/CatBrowser-1.3.exe` / `CatBrowser-1.2.exe`
およびタグ `v1.3` / `v1.2` に残しています。

> **現在の優先順位: Windows のみ。** macOS / Linux のソースは将来用に残していますが、
> 既定ではビルド対象外です（後回し）。Windows の完成・検証を優先しています。

## 機能

### 共通

- 上部 32 px の操作バー + WebView 領域だけの最小 UI（タブ / ブックマーク / ツールバーなし）
- 何も操作していないとき（非アクティブ・最小化）は WebView を非表示にして描画処理を停止
- メッセージループは `GetMessageW` のブロック待ち。アイドル時にタイマーやポーラーは使いません
- バックグラウンド通信 / 同期 / 更新 / メトリクスは WebView2 起動フラグで無効化

### URL / 検索入力

- `URL` / `検索` ボタンで入力欄の意味を切り替え
  - `URL` モード: `http://` / `https://` のみ。スキーム省略時は `https://` を補完
  - `検索` モード: 入力語を percent-encode して選択中の検索エンジンに渡す
- スペース・引用符・`javascript:` / `file:` / `data:` などは URL モードで入力段階から拒否
- 検索エンジンボタン（`DDG` / `Google` / `Bing`）で cycling。設定は `CatBrowser.ini` に保存
- `↑` / `↓` で直近の入力（URL・検索語）を補完。`history.txt` に最大 100 件保存
- `Enter` : 遷移 / 検索＋即座に WebView へフォーカス移動
- コマンドライン引数で URL を渡すことも可能: `CatBrowser https://example.com`

### ナビゲーション / 表示

- `←` / `→` : 戻る / 進む（履歴がない間はボタンを無効化）
- `再` ボタンまたは `F5` : キャッシュを無視してリロード
- `全` ボタンまたは `F11` : URL バーを含めたボーダレス全画面の切り替え
- `夜` / `昼` ボタン: Win32 UI と WebView の推奨カラースキームをダーク / ライトで切替。設定を保存

### 翻訳 / ダウンロード

- `訳` ボタン: 表示中のページ全体を**内蔵辞書で完全オフライン翻訳**。
  押したときだけ 1 回スクリプトを実行し、監視・タイマー・通信は一切ありません
- 方向切替ボタン（`→JA` / `→EN`）で英日 / 日英を切り替え
- 再翻訳時は前回の置換を元に戻してから実行するため、二重翻訳されません
- `dict.txt` に `english<TAB>japanese` または `english=japanese` を 1 行ずつ書くと、
  起動時にユーザー辞書として読み込みます（`#` でコメント）。通信は行いません
- ダウンロード先は `CatBrowser.ini` の `downloaddir=` で指定。未設定なら WebView2 の既定フォルダー

## 設定ファイル

exe と同じフォルダーに置きます。存在しなければ既定値で動作します。

`CatBrowser.ini`（UTF-8）:

```ini
engine=ddg
theme=dark
downloaddir=C:\Users\user\Downloads\CatBrowser
```

- `engine`: `ddg` / `google` / `bing`
- `theme`: `dark` / `light`
- `downloaddir`: 空欄なら既定のダウンロードフォルダー

`history.txt`（UTF-8、1 行 1 件、新しい順、最大 100 件）:

```text
https://example.com/
cat browser launcher
```

`dict.txt`（UTF-8、1 行 1 組）:

```text
# english<TAB>japanese
high score	ハイスコア
insert coin=コインを入れる
```

## ディレクトリ構成

```
CMakeLists.txt
.github/workflows/build.yml
src/
  main.cpp                 エントリポイント (wWinMain / main)
  App.h                    プラットフォーム共通インターフェース
  common/
    UrlUtils.*             http(s) のみの URL 正規化
    Search.*               検索エンジン切替と URL 生成
    Translation.*          オフライン EN<->JA 辞書と一回限りの翻訳スクリプト
    Text.h                 UTF-8 <-> UTF-16 (Win32)
    Log.*
  win32/
    PlatformWindow.*       Win32 ウィンドウ / 操作バー / 全画面 / 履歴補完
    LocalData.*            ini / history.txt / dict.txt の読み書き
    WebEngine.*            WebView2 起動・設定・ナビ・ダウンロード・テーマ
    Version.rc.in          exe のバージョン情報 (1.4)
    App_win.cpp            優先度設定と配線
    GpuExports.cpp         NVIDIA / AMD の GPU 強制エクスポート
  mac/
    PlatformWindow.mm      NSWindow / NSTextField / NSButton
    WebEngine.mm           WKWebView / 非永続データストア
    App_mac.mm             NSProcessInfo アクティビティ / キーモニタ
    Info.plist.in
  linux/
    PlatformWindow.*       GTK3 ウィンドウ / キースヌーパ
    WebEngine.*            WebKitGTK / エフェメラルコンテキスト
    App_linux.cpp          setpriority() と配線
```

## ビルド (Windows)

Visual Studio 2022 の「C++ によるデスクトップ開発」と CMake が必要です。

```powershell
# WebView2 SDK を一度だけ取得（このリポジトリの packages/ にも取得済み）
Invoke-WebRequest https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile nuget.exe
.\nuget.exe install Microsoft.Web.WebView2 -ExcludeVersion -OutputDirectory packages

# CMakePresets を使う場合（推奨）
cmake --preset windows-release
cmake --build --preset windows-release
# build\windows\Release\CatBrowser.exe

# または直接
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
# build\Release\CatBrowser.exe
```

VS Code は CMake Tools 拡張で `windows-release` プリセットを選択すればそのままビルドできます
（`.vscode/` にも設定・タスクを用意済み）。

WebView2Loader は静的リンクされるため、実行ファイル本体は単一の `.exe` です
（実行時に WebView2 Evergreen Runtime が必要。Windows 11 は標準搭載）。
設定・履歴・辞書は exe の隣に別ファイルとして保存されます。

### macOS / Linux を再開するとき

既定では `CATBROWSER_WINDOWS_ONLY=ON` のため、非 Windows ホストではターゲットが作られません。
再開する場合は明示的に OFF を指定します。なお 1.4 の検索 / 設定機能は Win32 実装です。

```sh
cmake -S . -B build -DCATBROWSER_WINDOWS_ONLY=OFF -DCMAKE_BUILD_TYPE=Release
```

## CI

push / pull_request では **Windows ジョブのみ**が実行され、
`CatBrowser-Windows-x64` Artifact（`CatBrowser.exe`）がアップロードされます。
WebView2 SDK は NuGet から自動取得します。

macOS / Linux ジョブはソースごと残していますが、`workflow_dispatch` で
`build_deferred_platforms` を ON にしたときだけ実行されます（後回し）。

## パフォーマンス設定の概要

| OS | 設定 |
|---|---|
| Windows | `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance` エクスポート、`ABOVE_NORMAL_PRIORITY_CLASS`、GPU 加速に絞った Chromium スイッチを `AdditionalBrowserArguments` で注入、ステータスバー / 右クリック / ズーム / デベロッパーツール / Autofill を無効化、非アクティブ/最小化時は WebView を非表示にして描画停止 |
| macOS | `NSActivityUserInitiated \| NSActivityLatencyCritical` で App Nap 無効、`nonPersistentDataStore`、`WKPreferences` の高速描画系キーを有効化、レイヤーバックトビュー |
| Linux | `setpriority(PRIO_PROCESS, 0, -10)`、ephemeral `WebKitWebContext`、ハードウェアアクセラレーション有効化 |

## 現実的な限界

- ホスト側で GC を止めることはできません。ゲームエンジン／JS エンジン自身の GC、
  ドライバのスケジューリング、ブラウザのプロセスモデルに起因するフレーム落ちは
  このランチャーだけでは完全にゼロにはできません。
- WebView2 のブラウザフラグは Microsoft が長期サポートを保証しない開発用フラグです。
  Runtime のバージョンによっては無視されます。
- `--disable-frame-rate-limit` や背景スロットリング無効化は「重い」原因になるため
  意図的に外しています。GPU 加速は `ignore-gpu-blocklist` / `use-angle=d3d11` で維持します。
- 翻訳辞書はゲーム UI の定型句が中心です。`dict.txt` で補えるのはテキスト置換だけなので、
  文脈を考慮した機械翻訳ではありません。通信も初期化も行わないため待機時は完全に無負荷です。
- 履歴はローカルファイルです。共有 PC では `history.txt` を削除すれば消せます。
- WebView2 の `PreferredColorScheme` はページ側が独自に配色を固定している場合は効かないことがあります。
- macOS / Linux の 1.4 対応は後回しです。現状の Windows バイナリが最新です。

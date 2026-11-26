# CatBrowser

検索機能を一切持たない、URL 入力だけの超軽量ネイティブランチャーです。
Electron / Tauri / Chromium の自前ビルドは使用せず、各 OS のネイティブ API と
OS 標準の WebView エンジンを直接制御します。

> **現在の優先順位: Windows のみ。** macOS / Linux のソースは将来用に残していますが、
> 既定ではビルド対象外です（後回し）。Windows の完成・検証を優先しています。

## 機能

- 上部 URL バー（30 px）＋ `GO` ボタン＋ WebView 領域のみの最小 UI
- 入力は `http://` / `https://` のみ。スキーム省略時は `https://` を補完します
- スペース・引用符・`javascript:` / `file:` / `data:` などのスキームは入力段階で拒否。
  検索エンジンへのリダイレクトはコード上存在しません
- `Enter` : 遷移＋即座に WebView へフォーカス移動
- `F5` : キャッシュを無視してリロード
- `F11` : URL バーを含めたボーダレス全画面の切り替え
- コマンドライン引数で URL を渡すことも可能です: `CatBrowser https://example.com`

## ディレクトリ構成

```
CMakeLists.txt
.github/workflows/build.yml
src/
  main.cpp                 エントリポイント (wWinMain / main)
  App.h                    プラットフォーム共通インターフェース
  common/
    UrlUtils.*             http(s) のみの URL 正規化
    Text.h                 UTF-8 <-> UTF-16 (Win32)
    Log.*
  win32/
    PlatformWindow.*       Win32 ウィンドウ / URL バー / 全画面
    WebEngine.*            WebView2 起動・設定・入力
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

WebView2Loader は静的リンクされるため、配布物は単一の `.exe` です
（実行時に WebView2 Evergreen Runtime が必要。Windows 11 は標準搭載）。

### macOS / Linux を再開するとき

既定では `CATBROWSER_WINDOWS_ONLY=ON` のため、非 Windows ホストではターゲットが作られません。
再開する場合は明示的に OFF を指定します。

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
| Windows | `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance` エクスポート、`ABOVE_NORMAL_PRIORITY_CLASS`（システム全体を遅くしない範囲）、GPU 加速に絞った Chromium スイッチを `AdditionalBrowserArguments` で注入、ステータスバー / 右クリック / ズーム / デベロッパーツール / Autofill を無効化 |
| macOS | `NSActivityUserInitiated \| NSActivityLatencyCritical` で App Nap 無効、`nonPersistentDataStore`、`WKPreferences` の `_acceleratedDrawingEnabled` / `_webGLEnabled` / `_canvasUsesAcceleratedDrawing` を KVC で有効化、レイヤーバックトビュー |
| Linux | `setpriority(PRIO_PROCESS, 0, -10)`、ephemeral `WebKitWebContext`、`enable-webgl` / `enable-accelerated-2d-canvas` / `hardware-acceleration-policy=ALWAYS` |

## 現実的な限界

- ホスト側で GC を止めることはできません。ゲームエンジン／JS エンジン自身の GC、
  ドライバのスケジューリング、ブラウザのプロセスモデルに起因するフレーム落ちは
  このランチャーだけでは完全にゼロにはできません。上記は「_delay 要因を減らす_」ための設定です。
- WebView2 のブラウザフラグは Microsoft が長期サポートを保証しない開発用フラグです。
  Runtime のバージョンによっては無視されます。
- `--disable-frame-rate-limit` や背景スロットリング無効化は「重い」原因になるため
  意図的に外しています。GPU 加速は `ignore-gpu-blocklist` / `use-angle=d3d11` で維持します。
- `_acceleratedDrawingEnabled` などの WKPreferences キーは非公開 SPI です。
  macOS の更新で名前が変わった場合は `@try/@catch` により安全に無視されます。
- Linux で nice 値を下げるには `CAP_SYS_NICE` または高い `RLIMIT_NICE` が必要です。
  権限が無い場合は警告を出して既定の優先度で継続します。
- WebKitGTK の `enable-accelerated-2d-canvas` は 2.32 以降非推奨ですが、
  互換性のため明示的に設定しています。

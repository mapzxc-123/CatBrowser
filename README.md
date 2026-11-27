# AstB3ast

ネイティブ API と OS 標準の WebView を直接制御する、超軽量のブラウザゲーム用ランチャーです。
Electron / Tauri / Chromium の自前ビルドは使用しません。

**バージョン 1.7** では読み込みの遅さを改善しました。WebView2 の起動をネイティブ UI の
準備と並行して開始し、ユーザーデータフォルダーを `%LOCALAPPDATA%\AstB3ast\WebView2` に
移動（`%TEMP%` のクリーンアップによる毎回のコールド起動を回避）、初回ナビゲーションで
WPAD 探索が長時間停留する問題を回避するプロキシ解決フラグを追加しました。
GPU 関係の開発用フラグも外し、ページ要求を UI ハンドラーの登録より先に開始します。

**バージョン 1.6** では UI を大幅変更しました。操作バーを 24 px のアイコンのみにし、
検索・履歴をオーバーレイ化、バーを自動非表示にしています。WebView のサイズ変更は
バーの表示状態やウィンドウサイズが実際に変わったときだけ行います。

**バージョン 1.5** では名称を **AstB3ast** に変更しました。設定・履歴・辞書は
1.5 から完全に新規のものとして扱い、旧 `CatBrowser.ini` は読み込みません。
1.5 の exe は `artifacts/windows/AstB3ast-1.5.exe`、1.4 以前は
`artifacts/windows/CatBrowser-1.4.exe` / `CatBrowser-1.3.exe` /
`CatBrowser-1.2.exe` に残しています。タグは `v1.7` / `v1.6` / `v1.5` /
`v1.4` / `v1.3` / `v1.2` です。

> **現在の優先順位: Windows のみ。** macOS / Linux のソースは将来用に残していますが、
> 既定ではビルド対象外です（後回し）。Windows の完成・検証を優先しています。

## 機能

### 共通 / 軽量さ

- 上部 24 px のアイコンバー + WebView 領域だけの最小 UI（タブ / ブックマーク / ツールバーなし）
- バーはマウスが画面上部に近づくと表示され、入力中やオーバーレイ表示中以外は自動的に隠れる
  - WebView2 の子 HWND は別プロセスなので、マウス位置は Raw Input で受け取る（タイマー／ポーリングなし）
- 検索入力欄・履歴リストは**呼び出したときだけ作成するオーバーレイ**に集約
- 常設コントロールは 8 個のアイコンボタンと WebView ホストのみ。テキストラベルは方向表示の `JA` / `EN` だけ
- 何も操作していないとき（非アクティブ・最小化）は WebView を非表示にして描画処理を停止
- メッセージループは `GetMessageW` のブロック待ち。アイドル時にタイマーやポーラーは使いません
- バックグラウンド通信 / 同期 / 更新 / メトリクスは WebView2 起動フラグで無効化

### 検索・履歴オーバーレイ

オーバーレイは次のいずれかで開きます。

- バーの虫眼鏡アイコンをクリック
- `Ctrl+L`（WebView にフォーカスがあるときも有効）
- バーにフォーカスがある状態でASCII文字を入力（その文字が入力欄に入ります）

オーバーレイ内でできること:

- 地球 / 虫眼鏡アイコンで `URL` / `検索` モードを切り替え
  - `URL` モード: `http://` / `https://` のみ。スキーム省略時は `https://` を補完
  - `検索` モード: 入力語を percent-encode して選択中の検索エンジンに渡す
- `D` / `G` / `B` ボタンで DuckDuckGo / Google / Bing を cycling。設定は `AstB3ast.ini` に保存
- 直近の入力をリスト表示。クリックで入力欄に入り、ダブルクリックでそのまま遷移
- `↑` / `↓` で直近の入力（URL・検索語）を補完。`history.txt` に最大 100 件保存
- `Enter` : 遷移 / 検索＋即座に WebView へフォーカス移動してオーバーレイを閉じる
- `Escape` : オーバーレイを閉じて WebView へ戻る
- スペース・引用符・`javascript:` / `file:` / `data:` などは URL モードで入力段階から拒否
- コマンドライン引数で URL を渡すことも可能: `AstB3ast https://example.com`

### ナビゲーション / 表示

バーはアイコンのみです。左から虫眼鏡（検索オーバーレイ）、`←` / `→`（戻る / 進む）、
リロード、全画面、翻訳、方向（`JA` / `EN`）、テーマの順に並びます。

- `←` / `→` : 戻る / 進む（履歴がない間はボタンを無効化）
- リロードアイコンまたは `F5` : キャッシュを無視してリロード
- 全画面アイコンまたは `F11` : URL バーを含めたボーダレス全画面の切り替え
- テーマアイコン: Win32 UI と WebView の推奨カラースキームをダーク / ライトで切替。設定を保存

### 翻訳 / ダウンロード

- 翻訳アイコン: 表示中のページ全体を**内蔵辞書で完全オフライン翻訳**。
  押したときだけ 1 回スクリプトを実行し、監視・タイマー・通信は一切ありません
- 方向切替ボタン（`JA` / `EN`）で英日 / 日英を切り替え
- 再翻訳時は前回の置換を元に戻してから実行するため、二重翻訳されません
- `dict.txt` に `english<TAB>japanese` または `english=japanese` を 1 行ずつ書くと、
  起動時にユーザー辞書として読み込みます（`#` でコメント）。通信は行いません
- ダウンロード先は `AstB3ast.ini` の `downloaddir=` で指定。未設定なら WebView2 の既定フォルダー

## 設定ファイル

exe と同じフォルダーに置きます。存在しなければ既定値で動作します。
1.5 以降は新規設定のみを使用し、旧 `CatBrowser.ini` からの引き継ぎは行いません。

`AstB3ast.ini`（UTF-8）:

```ini
engine=ddg
theme=dark
downloaddir=C:\Users\user\Downloads\AstB3ast
```

- `engine`: `ddg` / `google` / `bing`
- `theme`: `dark` / `light`
- `downloaddir`: 空欄なら既定のダウンロードフォルダー

`history.txt`（UTF-8、1 行 1 件、新しい順、最大 100 件）:

```text
https://example.com/
ast browser launcher
```

`dict.txt`（UTF-8、1 行 1 組）:

```text
# english<TAB>japanese
high score\tハイスコア
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
    PlatformWindow.*       Win32 ウィンドウ / 24px アイコンバー / 検索オーバーレイ / 全画面
    LocalData.*            ini / history.txt / dict.txt の読み書き
    WebEngine.*            WebView2 起動・設定・ナビ・ダウンロード・テーマ
    Version.rc.in          exe のバージョン情報 (CMake のプロジェクトバージョン)
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
# build\windows\Release\AstB3ast.exe

# または直接
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
# build\Release\AstB3ast.exe
```

VS Code は CMake Tools 拡張で `windows-release` プリセットを選択すればそのままビルドできます
（`.vscode/` にも設定・タスクを用意済み）。

WebView2Loader は静的リンクされるため、実行ファイル本体は単一の `.exe` です
（実行時に WebView2 Evergreen Runtime が必要。Windows 11 は標準搭載）。
設定・履歴・辞書は exe の隣に別ファイルとして保存されます。

### macOS / Linux を再開するとき

既定では `ASTB3AST_WINDOWS_ONLY=ON` のため、非 Windows ホストではターゲットが作られません。
再開する場合は明示的に OFF を指定します。なお検索 / 設定機能は Win32 実装です。

```sh
cmake -S . -B build -DASTB3AST_WINDOWS_ONLY=OFF -DCMAKE_BUILD_TYPE=Release
```

## CI

push / pull_request では **Windows ジョブのみ**が実行され、
`AstB3ast-Windows-x64` Artifact（`AstB3ast.exe`）がアップロードされます。
WebView2 SDK は NuGet から自動取得します。

macOS / Linux ジョブはソースごと残していますが、`workflow_dispatch` で
`build_deferred_platforms` を ON にしたときだけ実行されます（後回し）。

## パフォーマンス設定の概要

| OS | 設定 |
|---|---|
| Windows | `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance` エクスポート、`ABOVE_NORMAL_PRIORITY_CLASS`、WebView2 起動をネイティブ準備と並行実行、`%LOCALAPPDATA%` の安定した UDF、不要な背景処理と WPAD 停留を避ける Chromium スイッチを `AdditionalBrowserArguments` で注入、ステータスバー / 右クリック / ズーム / デベロッパーツール / Autofill を無効化、非アクティブ/最小化時は WebView を非表示にして描画停止、24px アイコンバーの自動非表示（Raw Input、タイマーなし）、WebView サイズ変更は状態変化時のみ |
| macOS | `NSActivityUserInitiated \| NSActivityLatencyCritical` で App Nap 無効、`nonPersistentDataStore`、`WKPreferences` の高速描画系キーを有効化、レイヤーバックトビュー |
| Linux | `setpriority(PRIO_PROCESS, 0, -10)`、ephemeral `WebKitWebContext`、ハードウェアアクセラレーション有効化 |

## 現実的な限界

- ホスト側で GC を止めることはできません。ゲームエンジン／JS エンジン自身の GC、
  ドライバのスケジューリング、ブラウザのプロセスモデルに起因するフレーム落ちは
  このランチャーだけでは完全にゼロにはできません。
- WebView2 のブラウザフラグは Microsoft が長期サポートを保証しない開発用フラグです。
  Runtime のバージョンによっては無視されます。
- `--disable-frame-rate-limit` / 背景スロットリング無効化 / GPU 強制フラグは
  「重い」原因になるため意図的に外しています。WebView2 の既定の GPU 設定を使います。
- 翻訳辞書はゲーム UI の定型句が中心です。`dict.txt` で補えるのはテキスト置換だけなので、
  文脈を考慮した機械翻訳ではありません。通信も初期化も行わないため待機時は完全に無負荷です。
- 履歴はローカルファイルです。共有 PC では `history.txt` を削除すれば消せます。
- 1.5 以降は旧 `CatBrowser.ini` を読み込みません。旧設定を使う場合は
  `AstB3ast.ini` に手動で移行してください。
- WebView2 の `PreferredColorScheme` はページ側が独自に配色を固定している場合は効かないことがあります。
- macOS / Linux の対応は後回しです。現状の Windows バイナリが最新です。

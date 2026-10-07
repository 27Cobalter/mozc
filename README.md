# Mozc (27Cobalter カスタム版)

[google/mozc](https://github.com/google/mozc) を Windows 向けに自分用にカスタマイズしたフォークです。
変更は `my-patches` ブランチにあります。
upstream への追従 (rebase / merge) 用の仕様書は [docs/my-patches-spec.md](docs/my-patches-spec.md) にあります (coding agent 向け)。

## カスタム内容

### インライン単語登録 (SKK 風)

変換候補を最後まで送ると、先頭に戻らず、そのまま単語登録モードに入ります。
別ウィンドウの単語登録ダイアログは開きません。

- 最後の候補で「次候補」(スペースなど)、または最後のページで「次ページ」(PageDown など) を押すと登録モードに入る
  - 予測 (Tab) も同じ。Tab で候補が追加されなくなって一周しそうなときに登録モードに入る
  - 入力欄に `[登録:よみ] ` が表示され、その後ろに登録したい単語を入力する
  - 通常どおりひらがな入力・変換・候補選択ができる。途中で確定した文字も `[登録:よみ] 亜い` のように残る
  - Enter は、入力中・変換中の文字を確定済みの文字に加える。何も入力していない状態でもう一度 Enter を押すと、
    確定済みの文字を名詞として辞書に登録し、そのままアプリに確定する
  - 何も入力していない状態で Esc (確定済みの文字がなければ Backspace / Enter も) を押すと登録を取り消し、変換前の読みの状態に戻る
  - スペースだけ (半角・全角・タブ) の文字列は確定のみで辞書には登録しない
- 登録先はユーザー辞書「インライン登録」(なければ自動作成)。登録後に辞書は自動で再読み込みされる
- キー設定のアクション `StartWordRegistration` で、変換中の任意のタイミングから登録モードを開始できる (既定では未割り当て)
- 設定項目はなく、常に有効

登録した語は、ユーザー辞書への追加に加えて、学習履歴にも追加される
(次の変換で先頭付近に出ることを期待した処理。先頭候補になるかは未検証)。

関連コミット: `IsFocusedCandidateLast` (engine)、インライン単語登録 (session)

#### 登録モード中の編集

- 表示は、確定済みの文字が下線なし、入力中の文字が点線、変換中の選択文節がハイライト
- カーソルがある。入力が空のとき、`←` `→` `Home` `End` で確定済みの文字の中を動き、`Backspace` `Delete` で消せる
  (「歩く」を Enter で確定して `Backspace` を押すと「く」だけが消える)
- Ctrl / Alt を押したキーで何も割り当てられていないもの (Ctrl+N など) は、IME が握りつぶしてアプリには渡さない

#### Ctrl+Shift+Enter で単語登録ダイアログを開く

別のアプリからコピペで語を入れたいときのために、登録モード中に Ctrl+Shift+Enter を押すと、
単語登録ダイアログが開く。読み・語 (確定済みの文字と入力中の文字)・辞書 (`インライン登録`) が入力済みで、
入力欄 (`[登録:…]`) は空になる (ダイアログにフォーカスが移ったときに、アプリへ入らないようにするため)。
Windows のみ (環境変数で値を渡す)。

#### インライン登録した語の表示と削除

変換候補のうち、`インライン登録` 辞書の語には、説明欄 (「ひらがな」などが出るところ) に `インライン登録` と出る。
その候補を選んで Ctrl+Del を押すと、`インライン登録` 辞書から削除する (履歴削除と同じキー)。
(upstream の既定のキー設定は、Ctrl+Del が予測の状態にしか割り当てられていないため、変換の状態にも割り当てた。)

#### 試験中の設定 (EXPERIMENTAL)

SKK の挙動に寄せるための試験的な設定です。使ってみて、残す・変える・消すを決めます。
設定画面の「入力補助」にある `[試験中] 単語登録: ...` のチェックボックスで切り替えます (既定は off)。

| 設定 | 内容 |
|:-----|:-----|
| Esc で一度に取り消す | 入力の途中でも、Esc 1 回で登録をすべて取り消し、変換前の読みに戻る |
| 選択中の文節の読みだけを登録する | 文全体ではなく、選択中の文節の読みで登録する。他の文節の文字は、登録した語と一緒に確定される (文節の区切りは、Shift+←/→ で変えられる) |

(以前あった「Enter を分ける」設定は、Enter が 2 段階になる動作を標準にしたので削除した。)

関連コミット: `[EXPERIMENTAL] Add config options for the inline word registration`

### アイコンのモノクロ化

`src/data/images` のアイコン (ico / png / tiff / svg) のオレンジ色を、グレースケールに変換した。
`src/build_tools/grayscale_images.py` (Pillow が必要) でやり直せる。

### ビルド時刻の表示

`--config release_build` でビルドしたとき、About 画面のバージョン表示に
ビルド時刻が付く (例: `(3.34.6239.100) build 2026-10-06 18:34:29`)。

### ローカルビルドごとのバージョン更新

同じバージョンの MSI を上書きインストールしても、バイナリが入れ替わらないことがあります。
`--config local_build` を付けると、ビルドごとにバージョンの 4 番目の数字が増えます
(`~/.mozc_local_build_number` のカウンター。例: `3.34.6239.101`, `.102`, ...)。
付けなければ従来どおりです。

### Qt ビルドで vcpkg の環境を無視

`CMAKE_TOOLCHAIN_FILE` などで vcpkg が有効だと、Qt が vcpkg の zlib / pcre2 / zstd にリンクされ、
`bazelisk build` で `uic.exe: error while loading shared libraries` になる問題の対策です。
`build_tools/build_qt.py` が vcpkg 関連の環境変数と PATH を無視します。
すでに vcpkg が有効な状態でビルドした Qt は、`build_qt.py` を再実行してください。

## ビルド方法

[docs/build_mozc_in_windows.md](docs/build_mozc_in_windows.md) の手順に従います。

```
cd src
python build_tools/update_deps.py
python build_tools/build_qt.py --release --confirm_license
bazelisk build package --config release_build --config local_build
python build_tools/open.py bazel-bin/win32/installer/Mozc64.msi
```

`--config local_build` は開発中のみ。通常のビルドでは外してください。

---

[Mozc - a Japanese Input Method Editor designed for multi-platform](https://github.com/google/mozc)
===================================

Copyright 2010-2026 Google LLC

Mozc is a Japanese Input Method Editor (IME) designed for multi-platform such as
Android OS, Apple macOS, Chromium OS, GNU/Linux and Microsoft Windows.  This
OpenSource project originates from
[Google Japanese Input](http://www.google.com/intl/ja/ime/).

Mozc is not an officially supported Google product.

Build Status
------------

| Linux | Windows | macOS | Android lib |
|:-----:|:-------:|:-----:|:-----------:|
| [![Linux](https://github.com/google/mozc/actions/workflows/linux.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/linux.yaml) | [![Windows](https://github.com/google/mozc/actions/workflows/windows.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/windows.yaml) | [![macOS](https://github.com/google/mozc/actions/workflows/macos.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/macos.yaml) | [![Android lib](https://github.com/google/mozc/actions/workflows/android.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/android.yaml) |


What's Mozc?
------------
For historical reasons, the project name *Mozc* has two different meanings:

1. Internal code name of Google Japanese Input that is still commonly used
   inside Google.
2. Project name to release a subset of Google Japanese Input in the form of
   source code under OSS license without any warranty nor user support.

In this repository, *Mozc* means the second definition unless otherwise noted.

Detailed differences between Google Japanese Input and Mozc are described in [About Branding](docs/about_branding.md).

For policies on vocabulary and conversion results, see
[Vocabulary Policy](VOCABULARY_POLICY.md).

Build Instructions
------------------

* [How to build Mozc for Android](docs/build_mozc_for_android.md): for Android library (`libmozc.so`)
* [How to build Mozc for Linux](docs/build_mozc_for_linux.md): for Linux desktop
* [How to build Mozc for macOS](docs/build_mozc_in_osx.md): for macOS build
* [How to build Mozc for Windows](docs/build_mozc_in_windows.md): for Windows

Release Plan
------------

tl;dr. **There is no stable version.**

As described in [About Branding](docs/about_branding.md) page, Google does
not promise any official QA for OSS Mozc project.  Because of this,
Mozc does not have a concept of *Stable Release*.  Instead we change version
number every time when we introduce non-trivial change.  If you are
interested in packaging Mozc source code, or developing your own products
based on Mozc, feel free to pick up any version.  They should be equally
stable (or equally unstable) in terms of no official QA process.

[Release History](docs/release_history.md) page may have additional
information and useful links about recent changes.

License
-------

All Mozc code written by Google is released under
[The BSD 3-Clause License](http://opensource.org/licenses/BSD-3-Clause).
For third party code under [src/third_party](src/third_party) directory,
see each sub directory to find the copyright notice.  Note also that
outside [src/third_party](src/third_party) following directories contain
third party code.

### [src/data/dictionary_oss/](src/data/dictionary_oss)
Mixed.
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

### [src/data/test/dictionary/](src/data/test/dictionary)
The same as [src/data/dictionary_oss/](src/data/dictionary_oss).
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

### [src/data/test/stress_test/](src/data/test/stress_test)
Public Domain.  See the comment in
[src/data/test/stress_test/sentences.txt](src/data/test/stress_test/sentences.txt)

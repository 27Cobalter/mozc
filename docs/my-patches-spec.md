# my-patches 仕様書 (coding agent 向け)

この文書は `my-patches` ブランチ (フォーク: 27Cobalter/mozc) の変更を、
**仕様ベース**で master に追従させるための記録です。人が読む説明は
[README.md](../README.md) にあります。

## この文書の使い方 (rebase / merge するとき)

1. **コードの衝突解消ではなく、仕様の再適用として扱う。**
   各機能の「仕様」と「不変条件」を満たすことが目的で、コードの形は問わない。
2. upstream (master) に **同等・類似の機能が入っていたら、upstream の仕組みを採用**し、
   本書の仕様との差分だけを薄く足す。自前実装は捨ててよい
   (ただし「食い違いやすい点」に書いた、upstream の既定動作と仕様が食い違う箇所は残す)。
3. 行番号は書かない。関数名・ファイル名で探す。
4. 終わったら末尾の「検証」を実行する。テストが仕様の正本。
   テストが upstream の変更で落ちた場合は、仕様 (本書) に合わせてテストを直す。

機能ごとに: 仕様 / 置き場所 / 食い違いやすい点 を書く。
`[EXPERIMENTAL]` は試験中で、他の仕様と競合したら消してよい。

---

## 1. インライン単語登録

### 仕様

- 変換中 (CONVERSION) に、**最後の候補で「次候補」**、または**最後のページで「次ページ」**を押すと、
  先頭に戻らず登録モードに入る。キー設定のアクション `StartWordRegistration`
  (ConversionState、既定は未割り当て) でも入れる。
- **予測 (Tab) でも同じ**: 予測の候補を最後まで送って、**候補が増えず一周しそうなとき**に登録モードに入る。
  最後の候補での Tab は、Mozc 本来の動作 (サジェストから予測候補を追加する) を先に行う。
  追加で候補が増えなかった (フォーカスが先頭に戻った) ときだけ登録する。
- 登録モードは別ウィンドウを開かず、**入力欄 (preedit) に表示**する。
  `[登録:よみ] ` + 確定済みの文字 + 入力中の文字。候補ウィンドウは通常どおり出る。
- 登録モード中の入力は**通常の IME と同じ**ことができる (入力・変換・候補選択・文節移動)。
- 登録の完了は **Enter**:
  - 入力中/変換中の文字があるとき: Enter は「確定済みの文字に加える」だけ (登録は終わらない)。
  - 入力が空で、確定済みの文字があるとき: Enter で登録完了。ユーザー辞書に**名詞**で追加し、
    文字列をアプリに確定する (result の key は元の読み)。
  - 入力が空で、確定済みの文字もないとき: Enter は取り消し。
- **取り消し**は、登録モードを抜けて、**変換前の読み (COMPOSITION)** に戻る (変換状態には戻さない)。
  入力が空のときの Esc / Backspace (確定済みの文字もないとき)。
- スペースだけ (半角・全角・タブ。`IsBlank`) の語は確定するが、辞書には登録しない。
- 登録先は専用のユーザー辞書 `インライン登録` (なければ作る)。同じ読み・語は重複して足さない。
- 登録後は辞書を再読み込みして**待つ** (`ReloadAndWait`)、学習履歴 (`AddUserHistory`) にも追加する。
  (登録した語が次の変換で先頭になることを期待。**未検証**。)
- IME オフ・`REVERT`・`RESET_CONTEXT` で登録モードは黙って破棄する。
- 読みは既定で変換全体の読み。`inline_register_focused_segment` (試験中) のときだけ選択中の文節。

### 置き場所

| 何 | どこ |
|:--|:--|
| 本体 | `session/session.cc` の `StartWordRegistration` / `SendKeyInWordRegistration` / `EditWordRegistrationText` / `CancelWordRegistration` / `PostProcessWordRegistration` / `FinishWordRegistration` / `DecorateWordRegistrationOutput` / `ResetWordRegistration` |
| 状態 | `session/session.h` の `registration_*` メンバ |
| 最後の候補の判定 | `engine/candidate_list.{h,cc}` の `IsFocusedLast` / `IsFocusedOnLastPage`、`engine/engine_converter*` の `IsFocusedCandidateLast` / `IsFocusedCandidateOnLastPage` |
| 次候補/次ページへの組み込み | `Session::ConvertNext` / `Session::ConvertNextPage` |
| キー設定 | `session/keymap.{h,cc}` (`START_WORD_REGISTRATION` ↔ `StartWordRegistration`)、`gui/config_dialog/keymap_{ja,en}.qtts` |
| 辞書の書き込み | `dictionary/inline_registration.{h,cc}` (`AddInlineRegisteredWord`) |
| 辞書の再読み込みと学習 | `session/session_handler.cc` の `ApplyRegisteredWord` (Session は `const EngineInterface&` しか持てないため、Session が `ConsumeRegisteredWord()` で渡し、Handler が処理する) |

### 食い違いやすい点

- **ネストした ImeContext 方式**。登録モードに入ると、変換中の `ImeContext` を
  `registration_context_` に退避し、`context_` はそのコピーを PRECOMPOSITION にしたもの。
  通常の入力処理 (`SendKeyInternal`) をそのまま使うための構造。upstream が ImeContext の
  コピーや `SetStateToPredompositionAndCancel` を変えたら、ここを合わせる。
- ネスト側の確定 (result) は `registration_value_` / `registration_after_` に**横取り**し、
  ホストへは返さない。完了時にだけ result を返す。
- `Session::TestSendKey` は `SendKey` と同じキーを「処理した (consumed)」と返すこと。
  登録モードでは `TestSendKeyInternal` の結果に上書きする (素通しだとアプリに漏れる)。
- 変換中の preedit は `EngineConverter::FillOutput` から取る (`FillPreedit` は変換前の
  composition しか出さない)。文節の読み (HIGHLIGHT の `key`) もここから取る。
- 取り消しで `ConvertCancel` を使うのは、変換前の読みに戻す仕様のため。
- `[登録:…]` の表示は preedit の先頭に足しているだけなので、候補ウィンドウの
  `position` は `ShiftCandidateWindowPosition` でずらす。
- 通常の「候補を一周して先頭に戻る」動作は、次候補と次ページだけ変えている。
  **前候補・前ページは一周のまま** (SKK との差。直さない仕様)。
- 予測では `Session::ConvertNext` が、`CandidateNext` を呼んだ**後**に候補ウィンドウの
  `focused_index == 0` を見て一周を検出する (先に `IsFocusedCandidateLast` で最後だったときだけ)。
  予測の追加は `EngineConverter::CandidateNext` の中で起きるため、事前には判定できない。
  `data/test/session/scenario/predict_and_convert.txt` (Tab 4 回目で追加) が通ることを保つ。
- セッションのテストの「一周しない」前提の箇所 (`Issue1805239`、
  `MultiSegmentSelectionFocusRightAndLeft`、`data/test/session/scenario/conversion.txt`) は、
  登録モードに入ることを期待するように変えてある。upstream のテストが増えて
  一周を期待して落ちたら、同じように直す。

---

## 2. 登録モード中の編集

### 仕様

- 表示は 3 種類。確定済みの文字は**下線なし** (`NONE`)、入力中 (composition) は
  **点線** (`UNDERLINE`)、変換中の選択文節は**ハイライト** (`HIGHLIGHT`)。
  Windows の TSF では、`UNDERLINE` が点線、`HIGHLIGHT` が太い実線、`NONE` は装飾なし。
  `[登録:よみ] ` も `NONE`。
- **カーソルを持つ**。確定済みの文字は「カーソルの前」と「カーソルの後」に分かれ、
  入力中の文字はその間に入る (preedit の cursor もその位置)。
- 入力が空のとき、次のキーで確定済みの文字を編集する (装飾キーなしのとき):
  `←` `→` (1 文字移動)、`Home` `End`、`BS` (カーソルの前の 1 文字を消す)、`Del` (後の 1 文字)。
  - 例: 「歩く」を Enter で確定 → `BS` で「く」が**1 回で**消える (2 回押す必要はない)。
  - 確定済みの文字も入力も空のときの `BS` は、登録の取り消し。
- 入力中・変換中のキーは通常の IME が処理する (入力中のカーソル移動・BS・Del も通常どおり)。
- **Ctrl / Alt が付いたキーで何も割り当てられていないもの** (例: Ctrl+N) は、
  IME が握りつぶす。アプリには渡さない (`TestSendKey` も consumed で返す)。
  修飾キーだけのイベントは対象外。

### 置き場所

`session/session.cc` の `EditWordRegistrationText`、`SendKeyInWordRegistration`、
`TestSendKey` (ラッパー) / `TestSendKeyInternal`、無名名前空間の `IsCtrlAltKey` /
`IsWordRegistrationTextKey` / `PopBackChar` / `PopFrontChar`。

### 食い違いやすい点

- 入力が空 (PRECOMPOSITION) のときだけ確定済みの文字を編集する。入力中は通常の
  composition が矢印キーを処理するので、確定済みの文字の中へは入れない (仕様)。
- 昔の `inline_register_two_step_enter` (Enter を 2 段階にする試験設定) は、
  上の Enter の仕様が既定になったので**削除済み** (`config.proto` の 9000 は reserved)。
  upstream 由来の差分で復活させない。

---

## 3. 単語登録ダイアログへの受け渡し

### 仕様

- 登録モード中に **Ctrl+Shift+Enter** で、単語登録ダイアログ (`word_register_dialog`) を開く。
  別アプリからのコピペで語を入れるため。
- 開くとき、ダイアログに**読み・語・辞書**を事前入力する:
  読み = 登録モードの読み、語 = 確定済みの文字 + 入力中の文字、辞書 = `インライン登録`
  (辞書がなければダイアログ上で作る。保存はダイアログの OK のとき)。
- 開くとき、**preedit は空**で返す (result も付けない)。ダイアログにフォーカスが移ると
  アプリが composition を確定してしまい、`[登録:…]` がアプリに入るため。
  セッションは通常の PRECOMPOSITION に戻る (登録モードは破棄)。
- Windows では、受け渡しが元々未配線だったので配線した。Unicode API で環境変数を設定し、
  ツールを起動したら消す。Mac / ibus は未配線 (`Output` のフィールドは付くが使わない)。

### 置き場所

| 何 | どこ |
|:--|:--|
| 起動と値 | `session/session.cc` の `OpenWordRegisterDialog`、`protocol/commands.proto` の `Output.word_register_default` (field 9000)、`launch_tool_mode = WORD_REGISTER_DIALOG` |
| 環境変数の設定 (Windows) | `win32/base/keyevent_handler.cc` の `MaybeSpawnTool` (`SetEnvironmentUtf8`) |
| 環境変数名 | `base/const.h` の `kWordRegisterEnvironment{Name,ReadingName,DictionaryName}` |
| ダイアログ側 | `gui/word_register_dialog/word_register_dialog.cc` (辞書名の環境変数を読み、コンボボックスを選ぶ。なければ追加) |

### 食い違いやすい点

- `Output` の `word_register_default` は upstream にないフィールド (番号 9000)。
  upstream が同じ番号を使い始めたら別の番号に変える (保存されるデータではないので互換性は不要)。
- 環境変数はホストアプリのプロセスに設定される。**起動した後で必ず消す**。
- Ctrl+Shift+Enter は登録モード中だけ特別扱い (keymap には載せていない)。
- **クリップボードの扱い**: 通常 (キー設定やメニュー) から開いたダイアログは、従来どおり
  クリップボードを語に入れる。登録モードから開いたときは、語が空でも読みの環境変数があれば
  それを使い、**クリップボードは見ない** (`word_register_dialog.cc` のコンストラクタ)。
  読みの環境変数の有無が「登録モードから開いた」目印。

---

## 4. インライン登録語の表示と削除

### 仕様

- 変換候補のうち、ユーザー辞書 `インライン登録` にある語 (読み・語が一致) には、
  候補の説明欄 (ひらがな・カタカナなどが出るところ) に **`インライン登録`** を出す。
  既に説明があれば、その後ろに半角スペースで足す。
- その候補にフォーカスがあるとき、フッターに `Ctrl+Delでインライン登録から削除` を出す。
- **Ctrl+Del** (アクション `DeleteSelectedCandidate` = 履歴削除と同じ) で、その語を
  `インライン登録` 辞書から削除する。削除後は辞書を再読み込みする (`ReloadAndWait`)。
- `インライン登録` 辞書にダイアログから足した語も、同じ辞書なので同じ扱い。

### 置き場所

| 何 | どこ |
|:--|:--|
| 目印と削除 | `rewriter/inline_registration_rewriter.{h,cc}` (`Rewrite`: 説明を足す / `ClearHistoryEntry`: 辞書から削除 / `Reload`: 語の集合を読み直す)、`rewriter/rewriter.cc` で `UsageRewriter` の後・`VersionRewriter` の前に登録 |
| 辞書の読み書き | `dictionary/inline_registration.{h,cc}` (`LoadInlineRegisteredWords` / `RemoveInlineRegisteredWord`) と定数 `kInlineRegistrationDictionaryName` / `kInlineRegistrationDescription` |
| 削除可能の印 (UI) | `engine/engine_output.cc` の `FillAnnotation` (`deletable`) とフッターの文言 |
| 再読み込み | `Session::DeleteCandidateFromHistory` が `user_dictionary_changed_` を立て、`session_handler.cc` の `ApplyRegisteredWord` が処理 |

### 食い違いやすい点

- upstream の既定キー設定 (`data/keymap/*.tsv`) は、Ctrl+Delete を**予測 (Prediction) 状態にしか**
  割り当てていない。変換 (Conversion) 状態でも効くように、`Conversion Ctrl Delete DeleteSelectedCandidate`
  を 5 つの tsv (atok / chromeos / kotoeri / mobile / ms-ime) に足してある。upstream の tsv が
  更新されたら、この行が残っていることを確認する (`keymap_test` の
  `DeleteSelectedCandidateInConversion`)。カスタムのキー設定を使っている場合は自分で足す。
- `converter::Attribute` の 32 ビットは**すべて使われている**ため、印は属性ではなく
  **`description` の末尾の文字列**で判定している。upstream が説明欄の組み立てを変えたら、
  この判定 (`absl::EndsWith(description, kInlineRegistrationDescription)`) が通るようにする。
  3 か所 (rewriter / `FillAnnotation` / フッター) で同じ判定を使う。
- 削除は `Converter::DeleteCandidateFromHistory` → `MergerRewriter::ClearHistoryEntry` →
  `InlineRegistrationRewriter::ClearHistoryEntry` の経路。この経路が変わったら、
  Ctrl+Del が履歴削除と同じキーで届くことを保つ。
- `Reload()` は学習履歴も読み直すので、`ApplyRegisteredWord` は先に `Sync()` している。
  外さない。

---

## 5. [EXPERIMENTAL] 試験中の設定

設定画面 (入力補助) のチェックボックス。既定は off。他の仕様と競合したら削除してよい。

| `config.proto` | 内容 |
|:--|:--|
| `inline_register_cancel_at_once` (9001) | Esc 1 回で、入力途中でも登録をすべて取り消す (変換前の読みに戻る) |
| `inline_register_focused_segment` (9002) | 文全体ではなく選択中の文節の読みで登録。他の文節は登録した語と一緒に確定 |

ラベルは日本語の原文を直接書いている (`.qm` は生成物なので、翻訳を足していない)。

---

## 6. ビルド・配布まわり

- **ビルド時刻の表示**: `--config release_build` で `--stamp` と
  `--workspace_status_command=build_tools/workspace_status.bat` (`src/.bazelrc`)。
  `gui/about_dialog` が `MOZC_BUILD_TIME` を About に出す。
- **`--config local_build`**: ビルドごとにバージョンの 4 番目 (REVISION) を増やす
  (`~/.mozc_local_build_number`)。同じバージョンの MSI は上書きしても入れ替わらないため。
  `src/base/BUILD.bazel` の `mozc_version_base_txt` / `mozc_version_txt`。
  開発中だけ使い、リリースでは付けない。
- **Qt のビルドで vcpkg の環境変数を無視**: `build_tools/build_qt.py`
  (`get_clean_vs_env_vars`) と `docs/build_mozc_in_windows.md` の NOTE。
- **アイコンをモノクロ化**: `data/images` の ico / png / tiff / svg の色を輝度のグレーに
  変換済み (透明度は保持)。`build_tools/grayscale_images.py` (Pillow が必要) で
  やり直せる。冪等なので、upstream が画像を更新 / 追加したら再実行する。
  `mac/product_icon.icns` は生成物なので対象外。

---

## 検証

```
cd src
bazelisk test //session/... //engine/... //rewriter:rewriter_test //dictionary/... --config release_build
bazelisk build package --config release_build --config local_build
```

- 登録モードの仕様の正本は `session/session_test.cc` の `InlineWordRegistrationTest`。
- 実機で見ること (単体テストでは確認できない):
  1. 候補を最後まで送ると `[登録:…]` が入力欄に出る。確定済みは下線なし、入力中は点線、変換中はハイライト。
  2. `歩く` → Enter → BS で「く」が 1 回で消える。←→ Home End Del が効く。
  3. Ctrl+N などがアプリに届かない。
  4. Ctrl+Shift+Enter でダイアログが開き、読み・語・辞書が入っていて、アプリに `[登録:…]` が残らない。
  5. 登録した語が候補に `インライン登録` と出て、Ctrl+Del で消える。
  6. 登録した語が次の変換で先頭に来るか (未検証)。

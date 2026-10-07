# TapeBloom 0.1.0 — Recording Tape Instrument

木製オルガン風の画面で、自分の声・ギターなどを録音し、持続音の安定区間を
ループして鍵盤で弾くMac向け音源です。AUプラグインと単体アプリを同じソースから生成します。

**開発初版 0.1.0 のソースです。**
音源コアの自動テストとJUCE 8.0.6に対するC++構文チェックを実施済みです。
2026-10-07、ユーザーのMacBookでビルド成功と単体アプリの起動を確認しました。
AUのLogic Pro実機動作、総レイテンシー、各機能の聴感品質は検証中です。
公開リポジトリ: https://github.com/fattime73-coder/TapeBloom

## 実装内容

- 入力機器から最大30秒の録音。停止後、バックグラウンドで単音の音程を解析
- 安定した音程区間の選択、正方向ゼロクロス近傍の接合点探索、最大15msのクロスフェード
- 自動基準音設定。ROOT MIDIで微調整可能
- 波形のループ両端をドラッグして手動修正
- 32ボイス、ベロシティ、サステインペダル、±2半音ピッチベンド
- 各音のADSR、全体のローパスフィルターとレゾナンス
- WOW（0.55Hz）／FLUTTER（7.3Hzと11.1Hz）の速度変調
- テープ風DRIVE、演奏中のみ加わるNOISE
- LOW（180Hzシェルフ）／MID（1kHzピーク）／HIGH（5kHzシェルフ）のEQ
- WAV／AIFF／FLACの読み込みとドラッグ＆ドロップ
- `.tapebloom`形式で音声込みの設定保存、ホストプロジェクトへの状態保存
- テープリールのアニメーション、入力／出力メーター、鍵盤、PANIC

画面は提示したイメージに基づくネイティブ描画です。生成画像をそのまま貼り付けた画面ではありません。

## Macでビルド

必要なもの：macOS 11以降、Xcode Command Line ToolsまたはXcode、CMake 3.22以降、Git、初回のインターネット接続。

1. ZIPを解凍し、TapeBloomフォルダを任意の場所に置きます。
2. ターミナルで必要に応じて `xcode-select --install` を実行します。
3. CMakeがなければ https://cmake.org/download/ から導入します。
   Homebrewを使っている場合は `brew install cmake` でも導入できます。
4. TapeBloomフォルダ内で次を実行します。

```bash
bash scripts/build-mac.sh
```

`Build-TapeBloom.command`を開いても同じ処理です。
JUCE 8.0.6を取得し、Apple SiliconとIntelのUniversal Binaryを構成します。

生成予定のファイル：

```text
build/TapeBloom_artefacts/Release/Standalone/TapeBloom.app
build/TapeBloom_artefacts/Release/AU/TapeBloom.component
```

続いてインストールします。Logic Proは先に終了してください。

```bash
bash scripts/install-mac.sh
```

アプリは `~/Applications`、AUは `~/Library/Audio/Plug-Ins/Components` に入ります。
同名の既存ファイルは日時付きバックアップに移動します。
インストール用スクリプトは個人利用のためのアドホック署名を付けます。
Developer ID署名・Apple公証付き配布物を生成する設定はまだありません。

## 単体アプリで最初の録音

1. TapeBloom.appを起動し、マイクへのアクセスを許可します。
2. ウィンドウのOptions → Audio/MIDI Settingsで入力と出力機器を選び、MIDI鍵盤を有効にします。
3. JUCE標準の入力ミュート表示が出る場合、入力ミュートを解除します。
   入力がミュートされていると録音データも無音になります。
4. INPUTメーターが反応することを確認します。録音入力は直接モニター出力しません。
5. RECORDを押し、単音を2〜5秒伸ばしてSTOPを押します。
6. 解析が終わったらAUDITION、画面の鍵盤、または外部MIDI鍵盤で演奏します。
7. SAVEで音声と設定を一緒に保存します。

ギターなら、まずビブラートをかけずに弾いて減衰を待つと安定区間が見つかりやすくなります。
ノイズや和音だけの録音など、確かな基準音が見つからない場合はエラー表示になり、既存音色は維持されます。
「音程を均一にする補正」ではなく、「音程が安定している場所を選ぶ」方式です。

## Logic ProのAU版

録音用オーディオ入力とMIDI演奏を両立するため、AUはMusic Effect（`aumf`）として構成しています。
通常のオーディオエフェクト挿入ではなく、ソフトウェア音源スロットの
**AU MIDI制御エフェクト → TapeBloom Audio → TapeBloom**での利用を想定しています。

入力はホストから供給される音声です。AUが独自にMacのマイクを開く構成ではありません。
Logicのサイドチェーンで録音元の入力／バスを選び、INPUTメーターが反応してから録音します。
このバス経路とメニュー表示はMac実機での確認が必要です。
入力が届かない場合は、単体版で保存した`.tapebloom`をLOADするか、録音したWAVをIMPORTしてください。

AUの検証コマンド：

```bash
auval -v aumf TpBl TbAu
```

## 確認済みの初版の問題

- ノート開始時は録音データの先頭から再生します。録音冒頭に無音があると、その分だけ発音が遅れます。ループ先頭からの即時発音／自動無音トリムは未実装です。
- 数値表示の桁数、非ASCII文字の文字化け、STOPボタンと入力メーターの重なりを確認しています。
- 公開しているのはこの初版です。これらの問題が修正済みであることを意味しません。

## 初版の制約

- 録音／読み込みはモノラルにまとめます。出力は同じ音を左右に送るステレオです。
- 1つの録音を鍵盤全体へ配置する方式です。マルチサンプルや鍵盤別テープはありません。
- ピッチ変更はテープ速度方式です。高い音ほどアタックやループの再生も速くなります。
- 現状の補間は線形です。極端な高音域の折り返しノイズ対策は今後の改善項目です。
- WOW／FLUTTERは全ボイス共通。独立したテープ個体差のモデルではありません。
- パラメーター平滑化は音量のみ。急なフィルター/EQ変更や最大発音数超過時の音切り替えは実機で聴感確認が必要です。
- 解析は単音を対象とし、低いベース音・強いビブラート・複雑な倍音では検出が不確かになる場合があります。
  音程探索の目安は55〜1400Hzです。
- 画面サイズは1100×740固定、UIは英語です。画像案のMODホイールは未実装です。
- 30秒を超える素材は取り込めません。音声込みの状態はDAWプロジェクト容量を増やします。

## テスト

JUCEなしで音源コアをテストできます。

```bash
cmake -S . -B build-core -DTAPEBLOOM_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

検証対象とMac実機チェック表は `TESTING.md` に記載しています。

## GitHubへの公開準備

このフォルダをリポジトリのルートとして利用します。
`.github/workflows/build.yml`はpush、PR、手動実行でコアテストとmacOSビルド・auvalを実行する設定です。
通過した場合、ActionsのArtifactsに未公証のMac版ZIPが保存されます。
Release公開は自動で行いません。まず実機テストに通ることを確認してください。

録音・プリセット・buildフォルダは`.gitignore`で除外しています。
依存ライブラリの利用条件は `THIRD_PARTY.md` を参照してください。

# Lift Pilot

C11 + libm の縦運動シミュレーター。物理の正典は `src/flight.c`。
端末版・CSV/SVG/GIF と、同じ C 計算を WASM で使うブラウザー版を持つ。

- 点質量モデル・静止大気・一定密度。ゲーム・実験室・物語は迎角の一次遅れのモデル(`src/flight.c`)。
  ピッチ角速度と縦の静安定・昇降舵を持つ剛体モデルは別の `src/pitch.c`(M2 #2)で、ブラウザーの pitch laboratory(`src/pitch_lab.c`・`web/pitch-lab.mjs`)と `./lift-pilot --pitch` だけが使う。
  `flight_step` を変えずに置いているのは、既存の CSV/SVG/GIF・物語の JSON をバイト一致で保つため。
- The Last Ferry(物語の飛行)は `src/ferry.c`。場面は区間(hop)の始まり・終わりで開き、電池・滑走路の位置と長さ・
  空気の動き(一様な風と上下の流れを RK4 の後に位置へ足す)・離陸高度・機体のモーターだけを変える。`src/flight.c` は変えない。
  台詞は `web/app.mjs`。ビート表は `docs/narrative/beats.json`、bench の出力(play-order・ablation・tension)は `make story` が書く(手で書かない)。
- JavaScript は描画と入力だけ。積分・揚抗力・着地判定・物語の数値は C で計算する。
- 原画の背景は装飾。実測の飛行経路には軸・単位・凡例を付ける。
- 乱数・保存・サーバー API は無い。既存の出力と物理モデルを保持する。
- 検証は `make test` / `make bench` / `make story-check` / `make plots` / `make web` / `make smoke`。
- Python は 3.14、`make -j` は最大4。
- 理論の手引きは `docs/theory/`。`make theory-figs` が図と実測値の表を `tests/theory_figs.c` から再生成し、`make theory-check` が
  対応表(文 → 頁 → 図 → 関数 → テスト)の欠け・リンク切れ・消えた関数名・未掲載の公開関数で落ちる。法則・規則を足したら同じ PR で頁と図と表を足す。
  README の「29 statements」は表の行数と合わせる。
- `make test` は bench の行をバイト一致で照合するので、`make story`・`make test` は Dockerfile と同じ Debian 13 の
  ツールチェーンで行う(この PC に C コンパイラは無い。`docker build .` か Debian 13 + build-essential のコンテナ)。
- CI は PR の軽量テストのみ(`docker build .` の中の `make test` と `make plots`)。WASM ビルド・撮影・E2E はローカル専用。
- 遊べる門の hook は `window.__tmGate`(`web/app.mjs`)。文字は 14 CSS px 以上、タップ対象は 44 CSS px 以上を保つ。
- 公開 repo。コミット・PR に生成 AI の署名、私的な情報を入れない。
- 実画面は `docs/images/`、原画と生成指示・採否は `assets/art/` と `docs/art-direction.md`。

## M4 の独自原画の例外

Codex built-in image_gen の独自の滑空機・飛行場原画を許可する。
ブラウザーの機体スプライトと背景に実描画する。既存作品の意匠・第三者の画像・
外部音声・外部フォントは持ち込まない。原画を含め配布10,000,000 B以下を守る。

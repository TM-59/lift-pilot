# Lift Pilot

C11 + libm の縦運動シミュレーター。物理の正典は `src/flight.c`。
端末版・CSV/SVG/GIF と、同じ C 計算を WASM で使うブラウザー版を持つ。

- 点質量モデル・静止大気・一定密度。迎角の一次遅れはあるがピッチ角速度は未実装。
- JavaScript は描画と入力だけ。積分・揚抗力・着地判定は C で計算する。
- 原画の背景は装飾。実測の飛行経路には軸・単位・凡例を付ける。
- 乱数・保存・サーバー API は無い。既存の出力と物理モデルを保持する。
- 検証は `make test` / `make bench` / `make plots` / `make web` / `make smoke`。
- Python は 3.14、`make -j` は最大4。Docker は使わない。
- CI は PR の軽量 C テストのみ。WASM ビルド・撮影・E2E はローカル専用。
- 公開 repo。コミット・PR に生成 AI の署名、私的な情報を入れない。
- 実画面は `docs/images/`、原画と生成指示・採否は `assets/art/` と `docs/art-direction.md`。

## M4 の独自原画の例外

Codex built-in image_gen の独自の滑空機・飛行場原画を許可する。
ブラウザーの機体スプライトと背景に実描画する。既存作品の意匠・第三者の画像・
外部音声・外部フォントは持ち込まない。原画を含め配布10,000,000 B以下を守る。

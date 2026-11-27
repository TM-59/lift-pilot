# BGM(M63 #29、親 TM-59/project-context#598)

方針(project-context 20-stacks.md「ゲームの BGM」): 基調はバラード。戦闘の役と物語は別の曲で、場面の切り替えで替える。歌は文字を読まない場面だけ。

## 鳴らす経路

ブラウザー版(C の飛行を Emscripten で動かす頁)だけに Web Audio の再生を足した。物理は C のまま、JS は画面から曲を選ぶだけ。
端末版・CSV/SVG/GIF は無音のまま。曲は最初の操作の後に `bgm/list.json` と `bgm/<曲>.ogg` を読む(初期ロードに入れない)。読めない・復号できない曲は無音のまま遊びを止めない。

## 場面 → 曲

| 画面 | 曲 | 強さ |
|---|---|---|
| 入口(welcome)・手動の進入・実演 2 つ(Trimmed landing・Speed ↔ height)・pitch laboratory | valley(基調のバラード) | 1 |
| The Last Ferry の区間(hop)を飛ぶ間 | hop(戦闘の役、pulse) | 1(区間の一時停止は 0.4) |
| 区間の場面(hangar・weir・orchard) | story | 0.7 |
| 重い場面(buyer・mill・doors) | story | 0.4 |
| The breaker's yard の場面(結末、coast)と 5 区間を終えた後 | yard | 1 |

戦う相手・主戦は無い。電池と滑空比で次の滑走路へ届かせる区間を戦闘の役にした。手動の進入と実演は押すまで止まっているのが普通なので一時停止で下げない。場面は選択を押すまで止まる(時間で流れない)。判定は `web/bgm.mjs` の `cue()`(物語か・場面・重さ・結末か・一時停止・終わったかだけを読む)。

- 切り替えは 2.5 秒の交差フェード。一時停止は 0.4 倍。
- **全部器楽**: どの場面も文字か計器を読むので、歌入りは作らない。
- 消音: 見出しの ♪(44px)と `M`(どちらも新設。M は他の操作に使われていない)。保存はしない(この repo は保存機能を持たない)。消音は出口(master)でも切るので、交差フェードで消えていく前の曲も同時に黙る(TM-59/project-context#668 の穴を最初から作らない)。
  横持ちのスマホ(高さ 520px 以下)は既存の型で見出しを隠すので ♪ も隠れる(縦持ちか M で切り替える)。

## 曲(採用・門の値)

| 場面(曲) | 鳴る画面 | 門 | 採用 | 通過 | ループ | LUFS / 山 | 継ぎ目の音色 | 落ち込み / 静かな窓 | 大きさ |
|---|---|---|---|---|---|---|---|---|---|
| valley | 基調(入口・手動の進入・実演・pitch laboratory) | soft | valley-c0-s934 | 10/12 | 25.455 秒 | -15.9 / -1.7 dBFS | 0.6 | 0.5 秒 / 0.04 | 556,596 B |
| hop | 戦闘の役(The Last Ferry の 5 区間を飛ぶ間) | pulse 113.2 BPM(計画 112) | hop-c2-s936 | 8/18 | 34.32 秒 | -16.0 / -2.9 dBFS | 0.32 | 0.5 秒 / 0.029 | 790,551 B |
| story | 物語(区間の場面。重い場面 buyer・mill・doors と一時停止は 0.4 倍) | soft | story-c1-s936 | 7/12 | 16.552 秒 | -16.0 / -1.5 dBFS | 0.48 | 1.5 秒 / 0.091 | 354,742 B |
| yard | 結末(The breaker's yard の場面と終えた後) | soft | yard-c0-s932 | 7/12 | 24.0 秒 | -16.0 / -2.6 dBFS | 0.44 | 0.0 秒 / 0.0 | 551,316 B |

計 2,253,205 B(4 曲)。通過 32/54。全候補の値は `docs/bgm/gate.md`、生の値は `docs/bgm/report.json`。

## 作り方

1. 計画: `docs/bgm/plan.json`(場面 × caption × seed 6、seed 931〜936。pulse の hop だけ caption 3 つで、どれも頭から拍が続く指示。他の作品と同じ caption・seed を使わない)。caption は音の特徴だけで、曲名・作者・作品名を書かない。
2. 生成: ACE-Step 1.5 turbo(MIT、steps 8)を Colab の L4 で 1 回の確保(54 本、端末からの 1 回 316.5 秒、計算枠 587.39 → 587.33)。
   `cd ~/dev/local-llm-ops && .venv/bin/python -m tools.colab bgm --repo <この repo> --out <dir> --backend colab --gpu L4`
   Colab が使えない時の落ちる先は `scripts/bgm_gen.py`(手元の CPU、local-llm-ops の music venv)。
3. 門と書き出し: `~/.local/share/local-llm-ops/genvenv/music/bin/python scripts/bgm_bake.py --takes <dir>`(ffmpeg が要る)。
   門は gravity-well #35・spot-gap #33・td-games #38 と同じ値で緩めていない: soft(バラード・物語・結末)は local-llm-ops `tools/genmodels/mood.py` の soft の門、pulse(戦闘の役)は PULSE_GATE + テンポ ±5%、全部に -16 LUFS ±1.5・山 -1 dBFS・継ぎ目の音色の跳び 2 倍以下・12 dB の落ち込み 2 秒以下、pulse は静かな窓の割合 0.10 以下、soft のループは 16 秒以上。
   `scripts/bgm_bake.py`・`scripts/bgm_gen.py` は gravity-well #35 の物と同じ(Ruff ALL・mypy strict を通る。soundfile・matplotlib を使わず ffmpeg で読み書き・作図)。mypy が numpy の型を読むので pyproject.toml の dev に numpy を足した。
4. 出所と利用条件: 曲は ACE-Step 1.5(MIT、出力に第三者の権利の主張なし)で手元の計画から生成した独自の音源。第三者の音源・歌詞・旋律は使っていない。外部の生成 AI・API key は使っていない。

## 波形とスペクトル(自分で見た所見)

画像は 2 周つないだ物で、真ん中が継ぎ目(上が波形、下が対数の周波数のスペクトル)。

- `valley`: 撥弦と鍵盤の粒が全体に続き、和音の帯が切れない。句の終わりに長い減衰があり、継ぎ目(0.6)の段差は見えない。
- `hop`: c2(頭から拍が続く指示)の候補。頭から終わりまで等間隔の拍と低い帯が続き、ループの終わりに 0.5 秒ほど薄い所があるだけ(落ち込み 0.5 秒・静かな窓 0.029)。継ぎ目の音色の跳び 0.32。
- `story`: 句ごとに鍵盤と弦の音がまとまって鳴り、句の間は静か(最長 1.5 秒、静かな窓 0.091)。台詞の邪魔をしない薄さ。
- `yard`: 旋律と弦の帯が全体に続き、途中に短い静けさ。山 -2.6 dBFS。継ぎ目の段差は見えない(0.44)。

人の試聴は門にしない(客観の門を通した物だけを採る)。

## 試験

- `node scripts/bgm-smoke.cjs`(ローカル専用、port `BGM_PORT` 既定 22091、記録 `docs/bgm/smoke.json`):
  最初の操作の前は何も読まない(0 件)・valley(出口 -17.5 dBFS)・hangar の場面で story(-30.5、0.7 倍)・交差フェード中の M で 0.8 秒後 -120 dBFS・hop(-24.7)・一時停止で利得 0.45 → 0.18(-40.3)・♪ 44×44 で -120 → -45.9・重い場面と The breaker's yard の cue・dist 3,257,212 B(10 MB の門の内。BGM 4 曲 2,253,205 B を含む。main は 994,713 B)。
- 既存の `make test`(飛行・ピッチ・GIF・観測・ferry、`story_bench --check` 8 行一致)と `make smoke` は通る(branch 161 項目・main 160 項目を同じ時間帯に、どちらも約 1,600 秒)。撮り直した画像と docs/validation.json は戻した。
  1 回目の branch の smoke は他の smoke 2 本と並んだ時、2 回目の物語の飛行で `#throttle` の fill が「not enabled」で 30 秒待って落ちた。main と branch を並べて回した 2 回目はどちらも通った(負荷の高い時の既存の揺れと判断。音の処理は画面の操作を変えない)。
- 遊べる門(`bin/playability-gate.py`、main と branch を同じ時間帯 2026-11-27 10:33): main 6/7・branch 6/7(② 横の未到達は main と同じ既知の型)。

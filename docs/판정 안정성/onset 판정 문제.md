# G5 연속 스트로크 onset 판정 실험

## 목적

운지 연습 모드에서 연속으로 들어오는 G5 파워코드 스트로크가 첫 24개 타깃까지
안정적으로 진행되는지 확인한다.

- `G5`는 실제 G5 음역이 아니라 파워코드 이름이다.
- 현재 G5 운지는 6번 줄 3프렛(G2), 5번 줄 5프렛(D3)으로 낮은 음역이다.
- 실제 연주에서는 타깃 두 줄 외 네 줄의 커팅 소리가 함께 들어올 수 있다.

## 테스트 구성

- 입력 파일: `assets/testSound/G5stroke*.mp3`
- 차트: `assets/charts/PAK - Night.json`의 처음 24개 G5 코드 타깃
- 테스트 실행 파일: `test/performanceTest/G5fingeringTest.cpp`
- 입력 방식: MP3를 128-frame 단위로 공급해 기존 audio queue와 judge thread 경로를 사용한다.
- 결과 지표
  - `detectedOnsets`: `aubio_onset_do()`가 검출한 onset 수
  - `startedFingeringJudgments`: 실제로 타깃 판정을 시작한 수
  - `chord pass/fail`: 시작한 판정의 코드 통과/실패 수
  - `targets correct`: Target 24까지 순서대로 진행한 수

이 테스트는 장치 지연은 포함하지 않지만, callback-to-judge queue와 onset/chord 판정 경로는
실제 플러그인과 같다.

## 관찰한 구조

현재 onset 검출과 운지 판정은 다음 순서로 동작한다.

```text
aubio_onset_do()
  ↓
detectedOnsets 증가
  ↓
pendingJudgments가 비어 있으면 새 타깃 판정 시작
  ↓
G5 chord는 160ms(CHORD_SETTLE_MS) 동안 샘플 수집
  ↓
fundamental 판정 성공 시 다음 타깃으로 진행
```

`new_aubio_onset("default", ...)`의 `default` descriptor는 aubio에서 HFC(High
Frequency Content)다. 초기 기준선의 peak-picking threshold는 `0.2f`였고, 현재 후보
조건은 adaptive whitening ON과 threshold `0.04f`다.

## 실험 결과

| 입력 / 설정                            | 타깃 통과 | detected | started | chord pass / fail | 해석                                                  |
| -------------------------------------- | --------: | -------: | ------: | ----------------: | ----------------------------------------------------- |
| `G5stroke.mp3`                         |     14/24 |       17 |      17 |            14 / 3 | 기본 녹음에서 15번 이후로 진행하지 못함               |
| `G5stroke_gainUp.mp3`                  |     15/24 |       18 |      18 |            15 / 3 | 입력 게인만 올려서는 큰 개선 없음                     |
| gain-up + fundamental peak ±1 bin      |     16/24 |       18 |      18 |            16 / 2 | 코드 판정 1건 개선, onset 수는 동일                   |
| `G5stroke_cuttingRemove.mp3` + ±1 bin  |      9/24 |       14 |      13 |             9 / 4 | 커팅을 제거하면 onset이 더 감소                       |
| gain-up + HFC(default) + threshold 0.2 |     16/24 |       19 |      18 |            16 / 2 | threshold 하향은 대기 중 중복 onset 1개만 추가        |
| gain-up + energy + threshold 0.2       |     11/24 |       25 |      14 |            11 / 3 | raw onset은 증가했지만 과검출과 대기 구간 손실로 악화 |

### 판정 대기 중 onset 보존 실험

`pendingJudgments`가 존재하는 동안에도 onset 후보와 이후 160ms 샘플을 보존하고,
이전 타깃 판정이 끝난 뒤 순서대로 연결하는 구현을 한 번 적용했다.

결과는 `16/24`, `detected 19`, `started 19`, `chord pass/fail 16/3`이었다.

- 이전에는 `detected 19 / started 18`이었고, 후보 보존으로 1개를 더 판정했다.
- 추가 처리된 onset은 `1961.5ms`였으며, `88.5ms` 뒤 `2050.0ms`에 실제 Target 2
  통과 onset이 다시 들어왔다.
- 즉 추가 후보는 다음 스트로크가 아니라 하나의 스트로크에서 나온 중복 onset이었다.
- 타깃 진행 수는 늘지 않았고 chord fail만 1개 증가했다.

따라서 이 변경은 되돌렸다. 현재는 판정 대기 중 onset을 다시 버린다.

### 반복성

gain-up + HFC(default) + threshold 0.2를 3회 실행했을 때 결과와 onset 시각이 모두
동일했다.

```text
16/24 targets correct
detected 19 / started 18 / chord pass 16 / chord fail 2
```

테스트 결과는 재생 타이밍, queue, judge thread의 우연한 변동이 아니라 입력 신호와
현재 detector 설정에 의해 결정된다.

### HFC onset 시각에서 보인 공백

대표 실행에서 다음 공백이 반복됐다.

```text
4200.9ms → 4922.0ms : 721.1ms
5628.1ms → 6150.3ms : 522.2ms
7591.7ms 이후       : 후속 onset 없음
```

일반적인 검출 간격은 약 160~190ms 또는 346~373ms였다. 따라서 위 구간은 실제
스트로크가 detector에서 누락됐을 가능성이 높은 구간이다. 단, 녹음에 별도 정답 onset
마커가 없으므로 정확한 누락 스트로크 개수는 아직 확정하지 않는다.

## 결론

### 1. raw onset과 judgment 손실을 분리해야 한다

raw onset 시각을 CSV와 직접 매칭한 현재 후보 조건(whitening ON, threshold `0.04`)의
결과는 아래와 같다.

```text
raw onset 25개
  ├─ 의도한 스트로크 raw TP 23개
  ├─ 의도한 스트로크 raw FN 1개: 7207ms up weak
  └─ x,x ignored 2개: 1861.9ms, 1961.6ms

started judgment 23개
  ├─ 의도한 스트로크 TP 22개
  └─ x,x ignored 1개: 1861.9ms
```

따라서 남은 문제는 두 종류다.

- **detector 문제**: `7207ms` up weak은 raw onset 자체가 없다.
- **구조 문제**: `5985.9ms` raw onset은 `5963ms` down strong 라벨의 raw TP지만,
  기존 pending chord judgment가 있어 버려진다.

기존 started judgment 기준 FN 2개를 모두 detector 누락으로 해석하면 안 된다.

### 2. 커팅은 제거할 노이즈가 아니다

커팅을 천으로 제거한 녹음은 onset이 18회에서 14회로 감소했다. 커팅과 피킹의
attack/transient는 낮은 G2/D3 파워코드에서 HFC onset을 만드는 유용한 단서다.

따라서 목표는 커팅을 제거하는 것이 아니라, 자연스러운 커팅을 포함한 연주에서 onset을
안정적으로 검출하는 것이다.

### 3. energy 단독 방식은 현재 입력에서 적합하지 않다

energy는 HFC보다 많은 raw onset(25회)을 검출했지만, 한 스트로크 안의 에너지 변화도
여러 번 잡아 과검출했다. 160ms 판정 대기와 결합되면서 실제 시작 판정 수는 14회로
감소했고, 최종 결과도 11/24로 나빠졌다.

현재 입력에서는 HFC가 energy보다 좋은 기준선이다.

### 4. 판정 대기 중 후보 보존과 판단 중첩은 구분한다

이전 후보 보존 실험에서 추가 처리된 `1961.5ms` onset은 현재 `x,x` 라벨로 분류된
의도하지 않은 소리였다. 따라서 이를 다음 target으로 순차 연결하는 방식은 적합하지 않다.

반면 `5963ms` raw TP는 유효한 스트로크다. 다음 해결책은 기존 160ms settle을 줄이는 것이
아니라, 새 raw onset마다 별도 160ms chord sample window를 열어 **판단을 겹쳐 수행**하는
방식이다.

## 다음 실험 우선순위

### 1. HFC + adaptive spectral whitening (완료: threshold 0.04 후보)

`InitializeFingeringTest()`에서 onset detector 생성 직후 아래 설정을 적용했다.

```cpp
aubio_onset_set_awhitening(g_state.onsetDetector, 1);
```

`G5stroke_gainUp.mp3`와 24개 라벨을 기준으로 HFC를 고정했다. whitening OFF와
whitening ON threshold `0.20`은 각각 3회 반복해 결과가 동일했다. 이후 whitening ON에서
threshold를 `0.15`부터 `0.03`까지 낮추며 한 변수씩 비교했다.

| 설정                          | raw / started |  TP |  FP |  FN | Recall | Precision |        F1 |
| ----------------------------- | ------------: | --: | --: | --: | -----: | --------: | --------: |
| whitening OFF, threshold 0.20 |       19 / 18 |  17 |   1 |   7 |  70.8% |     94.4% |     81.0% |
| whitening ON, threshold 0.20  |         4 / 4 |   4 |   0 |  20 |  16.7% |    100.0% |     28.6% |
| whitening ON, threshold 0.15  |       22 / 21 |  20 |   1 |   4 |  83.3% |     95.2% |     88.9% |
| whitening ON, threshold 0.10  |       23 / 21 |  20 |   1 |   4 |  83.3% |     95.2% |     88.9% |
| whitening ON, threshold 0.05  |       24 / 22 |  21 |   1 |   3 |  87.5% |     95.5% |     91.3% |
| whitening ON, threshold 0.04  |       25 / 23 |  22 |   1 |   2 |  91.7% |     95.7% | **93.6%** |
| whitening ON, threshold 0.03  |       25 / 23 |  22 |   1 |   2 |  91.7% |     95.7% | **93.6%** |

`0.20`에서 whitening은 onset을 과도하게 억제했다. descriptor 분포가 whitening으로
변했지만 OFF 기준 threshold를 그대로 사용한 조합이 원인이다. threshold를 낮추면서
onset 수와 TP가 회복됐고, FP는 1개로 유지됐다.

`0.04`에서 `4715ms` up weak이 추가로 TP가 되어 FN이 3개에서 2개로 줄었다. 남은 FN은
`5963ms` down strong, `7207ms` up weak이다. `0.03`은 모든 지표가 `0.04`와 같으므로
성능 증가가 멈춘 포화 구간으로 판단했다.

따라서 현재 입력의 후보 설정은 **adaptive whitening ON + threshold 0.04**다. `0.03`도
같은 결과지만, 동일 성능에서는 더 높은 threshold가 다른 입력에서 불필요한 FP를 억제할
여지가 있어 `0.04`를 선택한다. 수정된 `x,x` 라벨과 raw onset 매칭 기준에서 raw onset
25개 중 `1961.6ms x,x`와 `5985.9ms down strong`이 chord settle 대기 중 버려진다.

### 2. chord settle 길이 비교 (완료: 160ms 유지)

`5963ms` 주변 라벨 간격이 빽빽한 구간에서 chord sample 수집 시간이 onset 손실 또는
코드 판정 실패에 미치는 영향을 확인했다. `G5stroke_gainUp.mp3`를 128-frame 단위로
공급하는 기존 `G5fingeringTest` 경로를 사용하고, HFC(`default`)와 threshold `0.20`을
고정했다. 각 조건은 독립 Debug DLL로 빌드했다.

| chord settle | onset TP / FP / FN | 타깃 통과 | chord pass / fail |
| -----------: | -----------------: | --------: | ----------------: |
|         80ms |         17 / 0 / 7 |      8/24 |            8 / 11 |
|        100ms |         16 / 0 / 8 |     13/24 |            13 / 5 |
|        120ms |         17 / 0 / 7 |     15/24 |            15 / 3 |
|    **160ms** |     **17 / 0 / 7** | **16/24** |        **16 / 2** |

onset은 CSV 라벨과 비교할 수 있어 TP/FP/FN으로 표기했다. chord는 onset으로 시작된
판정마다 G5 fundamental 통과 여부만 기록하므로, 별도 음성 chord 정답 라벨이 없는 현재
테스트에서는 FP를 정의할 수 없다. 따라서 chord 지표는 pass/fail을 유지한다.

- 80ms에서는 `1961.5ms`의 중복 onset까지 새 판정으로 시작한다. 그러나 chord sample
  수집량이 부족해 fail이 11건으로 증가했다.
- 100ms는 `1961.5ms` 중복 onset이 다음 `2050.0ms` onset을 선점해, `2030ms` down strong
  라벨까지 놓쳤다.
- 120ms는 160ms에 근접하지만 chord pass가 1건 적다.
- 당시에는 started judgment만 비교했기 때문에 `5963ms` down strong을 detector FN으로
  기록했다. raw onset 매칭 결과 `5985.9ms` raw TP가 존재하며, 기존 pending judgment가
  있어 버려진 구조적 손실임이 확인됐다.

따라서 이 입력에서는 **CHORD_SETTLE_MS를 160ms로 유지**한다. 더 짧은 수집 창은 다음
onset을 더 빨리 받을 수 있어도 chord fundamental 판정 품질 저하가 더 크다. `5963ms` 개선은
settle 단축이 아니라, settle window를 유지한 병렬 judgment 처리에서 다룬다.

### 3. 의도하지 않은 초반 소리 제거 입력 (완료)

`G5test_removeUnexpectSound.mp3`는 원본의 `1861.9ms`, `1961.6ms` 의도하지 않은 소리를
삭제한 파일이다. 라벨에서도 두 `x,x` 항목을 제거하고, 의도한 스트로크 24개와 마지막
`7895ms x,x` 항목만 유지했다. 조건은 whitening ON, threshold `0.04`, 병렬 judgment다.

2회 실행 결과가 동일했다.

| 입력 | raw / started | raw TP / FP / FN | judgment TP / FP / FN | pending dropped | chord pass / fail |
|---|---:|---:|---:|---:|---:|
| 원본 gain-up | 25 / 25 | 23 / 0 / 1 | 23 / 0 / 1 | 0 | 21 / 4 |
| 초반 소리 제거 | 23 / 23 | 23 / 0 / 1 | 23 / 0 / 1 | 0 | 19 / 4 |

- 삭제한 두 소리는 새 파일에서 raw onset으로 검출되지 않았다.
- 의도한 스트로크의 raw TP와 judgment TP는 모두 23개로 유지됐다.
- 유일한 detector FN은 계속 `7207ms up weak`이다.
- `5963ms down strong`은 새 파일에서도 raw TP와 judgment TP이므로, pending 구조적 유실은
  제거된 상태다.

다만 첫 의도 스트로크는 원본과 다르게 chord fail이 됐다.

```text
원본: 2050.3ms → 2030ms down strong, Correct
편집 파일: 2021.4ms → 2030ms down strong, Incorrect
```

편집 파일의 첫 onset 시각은 라벨보다 8.6ms 빠르고, 2회 모두 같은 chord fail이 발생했다.
따라서 삭제 과정의 편집 경계가 첫 스트로크 주변 waveform 또는 160ms chord FFT window에
영향을 준 것으로 판단한다. 이 파일은 onset 구조 검증에는 유효하지만, chord pass/fail을
자연스러운 원본과 동등하게 비교하는 입력으로는 사용하지 않는다.

### 4. settle 유지 + judgment 겹침 (완료: onset 유실 제거)

`CHORD_SETTLE_MS = 160ms`는 유지한다. raw onset이 들어오면 기존 pending judgment가 있어도
새 target용 pending judgment를 만들고, 각 judgment가 독립적으로 이후 160ms sample을
수집하도록 한다.

- `5985.9ms` raw TP는 더 이상 버려지지 않고 `5963ms` down strong의 chord 판정까지
  수행된다.
- raw onset과 started judgment가 모두 25개(원본), 23개(초반 소리 제거 파일)가 되어 pending
  onset 유실이 제거됐다.
- `5963ms`의 chord 결과는 fail이므로, 해결된 것은 onset 전달 구조이며 chord fundamental
  판정 품질은 별도 문제로 남아 있다.

### 5. 7207ms weak-up ODF 상태 확인

`7207ms` up weak은 유일한 raw FN이다. 이 구간 전후의 hop별 onset detection function(ODF),
thresholded descriptor, peak-picking threshold를 기록한다.

- 목표: ODF peak가 threshold `0.04` 아래인지, peak 자체가 형성되지 않았는지 구분한다.
- 비교 구간: 인접한 검출 성공 weak-up과 `7207ms`를 같은 시간 창으로 비교한다.
- 결과에 따라 threshold 재조정 또는 보조 detector 필요 여부를 결정한다.

### 6. SpecFlux와 Complex의 보완성 시험

HFC만으로 놓치는 `7207ms` weak-up을 대상으로 SpecFlux와 Complex descriptor를 같은 라벨과
raw onset 기준으로 비교한다.

- 각 descriptor의 raw TP/FP/FN과 `7207ms` 검출 여부를 기록한다.
- HFC의 raw TP를 유지하면서 다른 descriptor만 잡는 onset이 있는지 확인한다.
- 보완성이 확인된 경우에만 HFC를 기준 detector로 두고 보조 후보 결합 방식을 검토한다.

### 7. onset 분석 창 크기 비교

현재 onset buffer는 1024 sample(44.1kHz에서 약 23ms), hop은 128 sample이다.
buffer를 512 sample(약 12ms)으로 줄여 빠른 스트로크 attack이 이전 스트로크와 덜 섞이는지
확인한다. whitening 실험 이후에 한 변수만 바꾸어 비교한다.

### 8. 필요 시 target-aware 보조 detector 설계

energy 단독 검출은 채택하지 않는다. 이후에도 HFC가 실제 스트로크를 누락하면,
저·중역의 짧은 RMS 상승을 보조 후보로 사용하고, 이후 G5 fundamental 판정이 통과할 때만
타깃 진행을 허용하는 방식을 검토한다.

이 단계는 custom detector와 상태 처리 변경이 필요하므로 whitening/buffer 실험 후에
진행한다.

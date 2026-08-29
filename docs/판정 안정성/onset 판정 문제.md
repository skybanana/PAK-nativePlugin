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
Frequency Content)다. 현재 peak-picking threshold는 `0.2f`다.

## 실험 결과

| 입력 / 설정 | 타깃 통과 | detected | started | chord pass / fail | 해석 |
|---|---:|---:|---:|---:|---|
| `G5stroke.mp3` | 14/24 | 17 | 17 | 14 / 3 | 기본 녹음에서 15번 이후로 진행하지 못함 |
| `G5stroke_gainUp.mp3` | 15/24 | 18 | 18 | 15 / 3 | 입력 게인만 올려서는 큰 개선 없음 |
| gain-up + fundamental peak ±1 bin | 16/24 | 18 | 18 | 16 / 2 | 코드 판정 1건 개선, onset 수는 동일 |
| `G5stroke_cuttingRemove.mp3` + ±1 bin | 9/24 | 14 | 13 | 9 / 4 | 커팅을 제거하면 onset이 더 감소 |
| gain-up + HFC(default) + threshold 0.2 | 16/24 | 19 | 18 | 16 / 2 | threshold 하향은 대기 중 중복 onset 1개만 추가 |
| gain-up + energy + threshold 0.2 | 11/24 | 25 | 14 | 11 / 3 | raw onset은 증가했지만 과검출과 대기 구간 손실로 악화 |

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

### 1. 주 병목은 chord 판정보다 onset 검출이다

24개 타깃에 도달하려면 최소 24개의 유효 onset이 필요하다. HFC 기준으로는
`detectedOnsets`가 19회이므로, chord 판정을 모두 통과시켜도 Target 24에 도달할 수 없다.

fundamental peak 허용 범위를 exact bin에서 ±1 bin으로 완화한 것은 chord fail을 줄이는
보조 개선이었다. onset 누락 자체는 해결하지 못했다.

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

### 4. 판정 대기 중 onset 보존은 현재의 주 해결책이 아니다

HFC에서 버려진 onset은 1개였고, 그 1개는 중복 onset이었다. 후보 보존만으로는 실제
누락 스트로크를 복구하지 못한다.

## 다음 실험 우선순위

### 1. HFC + adaptive spectral whitening (완료: 비채택)

`InitializeFingeringTest()`에서 onset detector 생성 직후 아래 설정을 적용했다.

```cpp
aubio_onset_set_awhitening(g_state.onsetDetector, 1);
```

`G5stroke_gainUp.mp3`와 24개 라벨을 기준으로, HFC와 threshold를 고정해 3회 반복했다.
모든 실행 결과가 동일했으므로 queue 또는 judge thread의 변동이 아니다.

| 설정 | TP | FP | FN | Recall | F1 | raw onset / started |
|---|---:|---:|---:|---:|---:|---:|
| whitening OFF | 17 | 1 | 7 | 70.8% | 81.0% | 19 / 18 |
| whitening ON | 4 | 0 | 20 | 16.7% | 28.6% | 4 / 4 |

- whitening ON에서 검출된 TP는 `3839.0ms` down weak, `4200.0ms` down strong,
  `4921.9ms` down strong, `6499.9ms` up strong뿐이다.
- weak up은 9개 모두 FN이었고, strong도 15개 중 12개가 FN이었다.
- chord pass/fail은 `4 / 0`이지만, onset 자체가 19개에서 4개로 감소한 결과다.

따라서 adaptive spectral whitening은 이 입력과 HFC threshold `0.2` 조합에서 onset을
과도하게 억제한다. FP 1개가 사라졌지만 FN이 7개에서 20개로 증가했으므로 채택하지
않는다.

### 2. onset 분석 창 크기 비교

현재 onset buffer는 1024 sample(44.1kHz에서 약 23ms), hop은 128 sample이다.
buffer를 512 sample(약 12ms)으로 줄여 빠른 스트로크 attack이 이전 스트로크와 덜 섞이는지
확인한다. whitening 실험 이후에 한 변수만 바꾸어 비교한다.

### 3. 필요 시 target-aware 보조 detector 설계

energy 단독 검출은 채택하지 않는다. 이후에도 HFC가 실제 스트로크를 누락하면,
저·중역의 짧은 RMS 상승을 보조 후보로 사용하고, 이후 G5 fundamental 판정이 통과할 때만
타깃 진행을 허용하는 방식을 검토한다.

이 단계는 custom detector와 상태 처리 변경이 필요하므로 whitening/buffer 실험 후에
진행한다.

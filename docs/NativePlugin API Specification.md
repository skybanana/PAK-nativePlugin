# PAK Native Plugin API Specification

Unity에서 `PAKNativePlugin.dll`을 호출하기 위한 C API 명세서입니다.

## 기본 정보

- DLL 이름: `PAKNativePlugin`
- ABI: C ABI (`extern "C"`)
- Windows export: `__declspec(dllexport)`
- 오디오 샘플 포맷: signed 16-bit PCM
- 내부 버퍼 크기: 128 frames
- 세션 시작 전 카운트다운: 5000 ms

## 호출 순서

```text
Initialize
  -> LoadChart
  -> ResetSessionTime
  -> SetDSPParams
  -> StartSession
      -> GetAudioStats 반복 호출
      -> PollGuitarInputEvent 반복 호출
      -> PollJudgeEvent 반복 호출
  -> StopSession
  -> Shutdown
```

`Initialize`는 내부에서 기존 리소스를 먼저 정리한 뒤 새 오디오 스트림을 준비합니다.
`ResetSessionTime`은 새 세션 시작 전에 내부 시간과 판정 진행 상태를 0부터 다시 시작하도록 초기화합니다.
세션 종료 시에는 `StopSession`을 호출하고, 플러그인을 더 이상 쓰지 않을 때 `Shutdown`을 호출합니다.

## Unity C# 선언 예시

```csharp
using System.Runtime.InteropServices;

public static class PakNativePlugin
{
    private const string DllName = "PAKNativePlugin";

    public enum JudgeResult
    {
        Perfect = 0,
        Good = 1,
        Bad = 2,
        Miss = 3,
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct JudgeEvent
    {
        public int noteIndex;
        public int result;
        public double judgedAudioTimeMs;
        public double judgedChartTimeMs;
        public float errorMs;
        public int detectedMidi;
        public int targetMidi;
        public int stringNumber;
        public int fret;
        public int startMs;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)]
        public string noteName;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct GuitarInputEvent
    {
        public int midi;
        public double audioTimeMs;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct AudioStats
    {
        public double streamTime;
        public double audioTimeMs;
        public double chartTimeMs;
        public double countdownMs;
        public int streamLatency;
        public uint bufferFrames;
        public uint droppedAudioBlocks;
        public uint droppedJudgeEvents;
        public int totalNotes;
        public int nextNoteIndex;
        public int isRunning;
        public int isFinished;
    }

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int Initialize(
        uint channels,
        uint sampleRate,
        uint inputDevice,
        uint outputDevice,
        uint inputOffset,
        uint outputOffset);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int LoadChart(
        [MarshalAs(UnmanagedType.LPStr)] string chartPath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ResetSessionTime();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int StartSession();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void StopSession();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetDSPParams(float inputGain, float outputGain, float lpfAlpha);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int PollJudgeEvent(out JudgeEvent outEvent);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int PollGuitarInputEvent(out GuitarInputEvent outEvent);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetAudioStats(out AudioStats outStats);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Shutdown();
}
```

## JudgeResult

| 값 | 이름 | 의미 |
| --- | --- | --- |
| 0 | `Perfect` | 타이밍 오차가 60 ms 이하이고 피치가 일치 |
| 1 | `Good` | 타이밍 오차가 140 ms 이하이고 피치가 일치 |
| 2 | `Bad` | 타이밍 오차가 240 ms 이하이고 피치가 일치 |
| 3 | `Miss` | 타이밍 범위를 벗어났거나 피치가 불일치 |

피치 판정은 현재 목표 MIDI와 감지 MIDI가 정확히 같아야 통과합니다.

## JudgeEvent

`PollJudgeEvent`로 가져오는 노트 1개의 판정 결과입니다.

| 필드 | 타입 | 의미 |
| --- | --- | --- |
| `noteIndex` | `int` | 채보의 노트 인덱스. 0부터 시작 |
| `result` | `int` | `JudgeResult` 값 |
| `judgedAudioTimeMs` | `double` | 플러그인 오디오 스트림 기준 판정 시각 |
| `judgedChartTimeMs` | `double` | 채보 기준 판정 시각 |
| `errorMs` | `float` | `judgedChartTimeMs - startMs` |
| `detectedMidi` | `int` | 감지된 MIDI 피치. 피치가 없으면 0 |
| `targetMidi` | `int` | 채보 노트의 목표 MIDI 피치 |
| `stringNumber` | `int` | 기타 줄 번호 |
| `fret` | `int` | 프렛 번호 |
| `startMs` | `int` | 채보 노트 시작 시각 |
| `noteName` | `char[16]` | 목표 음 이름. 예: `E2`, `F#3` |

## GuitarInputEvent

`PollGuitarInputEvent`로 가져오는 기타 입력 이벤트입니다.

채보 판정과 별개로, 입력 오디오에서 onset이 감지된 뒤 80 ms 피치 안정 구간을 기다리고 MIDI 피치가 확인되면 이벤트가 발생합니다.
MIDI 피치가 감지되지 않으면 이벤트를 발생시키지 않습니다.

| 필드 | 타입 | 의미 |
| --- | --- | --- |
| `midi` | `int` | 감지된 MIDI 피치 |
| `audioTimeMs` | `double` | 플러그인 오디오 스트림 기준 onset 시각 |

## AudioStats

`GetAudioStats`로 가져오는 현재 세션 상태입니다.

| 필드 | 타입 | 의미 |
| --- | --- | --- |
| `streamTime` | `double` | 세션 기준 스트림 시간. 초 단위 |
| `audioTimeMs` | `double` | 오디오 스트림 시간. ms 단위 |
| `chartTimeMs` | `double` | 채보 시간. `audioTimeMs - countdownMs` |
| `countdownMs` | `double` | 세션 시작 전 카운트다운 시간. 현재 5000 |
| `streamLatency` | `int` | RtAudio 스트림 지연 |
| `bufferFrames` | `uint` | 내부 오디오 버퍼 프레임 수. 현재 128 |
| `droppedAudioBlocks` | `uint` | 판정 스레드로 전달하지 못한 오디오 블록 수 |
| `droppedJudgeEvents` | `uint` | Unity가 늦게 polling해서 버려진 판정 이벤트 수 |
| `totalNotes` | `int` | 로드된 채보의 전체 노트 수 |
| `nextNoteIndex` | `int` | 다음 판정 대상 노트 인덱스 |
| `isRunning` | `int` | 오디오 스트림 실행 중이면 1, 아니면 0 |
| `isFinished` | `int` | 모든 노트 판정이 끝났으면 1, 아니면 0 |

`chartTimeMs`가 0보다 작으면 카운트다운 구간입니다.

## Functions

### Initialize

```c
int Initialize(
    unsigned int channels,
    unsigned int sampleRate,
    unsigned int inputDevice,
    unsigned int outputDevice,
    unsigned int inputOffset,
    unsigned int outputOffset);
```

오디오 스트림, DSP 상태, 판정 큐, aubio 피치/온셋 감지기를 초기화합니다.

| 파라미터 | 의미 |
| --- | --- |
| `channels` | 입출력 채널 수 |
| `sampleRate` | 샘플레이트. 예: 48000 |
| `inputDevice` | 입력 장치 선택. 0이면 기본 입력 장치 |
| `outputDevice` | 출력 장치 선택. 0이면 기본 출력 장치 |
| `inputOffset` | 입력 장치의 시작 채널 |
| `outputOffset` | 출력 장치의 시작 채널 |

반환값:

- `0`: 성공
- `-1`: 오디오 스트림 생성 실패

### LoadChart

```c
int LoadChart(const char *chartPath);
```

다음 세션에서 사용할 채보 JSON 파일을 로드합니다.

반환값:

- `0`: 성공
- `-1`: 채보 파일 로드 실패

채보는 `schemaVersion`, `song`, `track`, `notes` 구조를 사용합니다.
노트의 `startTick`은 BPM과 resolution을 기준으로 ms로 변환됩니다.

### ResetSessionTime

```c
void ResetSessionTime(void);
```

내부 세션 시각, 다음 판정 노트 인덱스, 판정 큐, 종료 플래그를 초기화합니다.

새 게임을 시작하기 전 호출하면 `GetAudioStats`의 `audioTimeMs`, `chartTimeMs`가 이전 세션 시간에 영향을 받지 않습니다.
`StartSession`도 내부에서 이 함수를 한 번 호출합니다.

### StartSession

```c
int StartSession(void);
```

오디오 스트림과 판정 스레드를 시작합니다.

반환값:

- `0`: 성공
- `-1`: 초기화되지 않았거나 스트림 시작 실패

세션 시작 직후 5000 ms 카운트다운이 적용됩니다.

### StopSession

```c
void StopSession(void);
```

현재 오디오 스트림을 멈추고 판정 스레드를 종료합니다.

### SetDSPParams

```c
void SetDSPParams(float inputGain, float outputGain, float lpfAlpha);
```

모니터링 오디오의 DSP 파라미터를 갱신합니다.

처리 순서:

```text
input gain -> tanh overdrive -> one-pole low-pass filter -> output gain
```

| 파라미터 | 의미 | 기본값 |
| --- | --- | --- |
| `inputGain` | 입력 게인 | 4.0 |
| `outputGain` | 출력 게인 | 0.5 |
| `lpfAlpha` | low-pass filter alpha | 0.2 |

### PollJudgeEvent

```c
int PollJudgeEvent(JudgeEvent *outEvent);
```

대기 중인 판정 이벤트를 하나 가져옵니다.

반환값:

- `1`: 이벤트 있음. `outEvent`에 값이 복사됨
- `0`: 이벤트 없음

Unity에서는 한 프레임 안에서 `0`이 나올 때까지 반복 호출하면 됩니다.

```csharp
while (PakNativePlugin.PollJudgeEvent(out var judgeEvent) == 1)
{
    // judgeEvent 처리
}
```

### PollGuitarInputEvent

```c
int PollGuitarInputEvent(GuitarInputEvent *outEvent);
```

대기 중인 기타 입력 이벤트를 하나 가져옵니다.

반환값:

- `1`: 이벤트 있음. `outEvent`에 값이 복사됨
- `0`: 이벤트 없음

채보 판정 이벤트와 독립적으로 동작하므로 Client 조작용 입력에 사용할 수 있습니다.
채보를 로드하지 않은 상태에서도 오디오 세션이 실행 중이면 polling할 수 있습니다.
Unity에서는 MIDI 값을 원하는 동작에 매핑하면 됩니다.

```csharp
while (PakNativePlugin.PollGuitarInputEvent(out var inputEvent) == 1)
{
    if (inputEvent.midi == 40) // E2
    {
        // 확인 처리
    }
}
```

### GetAudioStats

```c
int GetAudioStats(AudioStats *outStats);
```

현재 오디오 시간, 채보 진행도, drop count, 종료 여부를 가져옵니다.

반환값:

- `0`: 성공
- `-1`: 초기화되지 않음

### Shutdown

```c
void Shutdown(void);
```

오디오 스트림, 판정 스레드, aubio 리소스를 해제합니다.

## Unity 사용 루프 예시

```csharp
void Update()
{
    if (PakNativePlugin.GetAudioStats(out var stats) != 0)
        return;

    if (stats.chartTimeMs < 0)
    {
        // countdown UI
    }

    while (PakNativePlugin.PollJudgeEvent(out var judgeEvent) == 1)
    {
        var result = (PakNativePlugin.JudgeResult)judgeEvent.result;
        // 판정 UI / 점수 처리
    }

    while (PakNativePlugin.PollGuitarInputEvent(out var inputEvent) == 1)
    {
        if (inputEvent.midi == 40)
        {
            // E2 입력 처리
        }
    }

    if (stats.isFinished == 1)
    {
        // 결과 화면
    }
}
```

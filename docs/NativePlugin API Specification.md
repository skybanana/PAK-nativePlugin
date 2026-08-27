# PAK Native Plugin API Specification

Unity?먯꽌 `PAKNativePlugin.dll`???몄텧?섍린 ?꾪븳 C API 紐낆꽭?쒖엯?덈떎.

## 湲곕낯 ?뺣낫

- DLL ?대쫫: `PAKNativePlugin`
- ABI: C ABI (`extern "C"`)
- Windows export: `__declspec(dllexport)`
- ?ㅻ뵒???섑뵆 ?щ㎎: signed 16-bit PCM
- ?대? 踰꾪띁 ?ш린: 128 frames
- ?몄뀡 ?쒖옉 ??移댁슫?몃떎?? 5000 ms
- 吏??梨꾨낫 踰꾩쟾: `0.1.0`, `0.2.0`

## ?몄텧 ?쒖꽌

### 梨꾨낫 ?먯젙 ?ъ슜

```text
Initialize
  -> LoadChart
  -> ResetSessionTime
  -> SetDSPParams
  -> StartSession
      -> GetAudioStats 諛섎났 ?몄텧
      -> PollJudgeEvent 諛섎났 ?몄텧
  -> StopSession
  -> Shutdown
```

### 湲고? ?낅젰留??ъ슜

```text
Initialize
  -> SetDSPParams
  -> StartSession
      -> GetAudioStats 諛섎났 ?몄텧
      -> PollGuitarInputEvent 諛섎났 ?몄텧
  -> StopSession
  -> Shutdown
```

### 泥쒖쿇???ъ깮 ?곗뒿

```text
Initialize
  -> LoadChart
  -> SetPracticeSpeed(0.25 ~ 1.25)
  -> StartSlowPracticeSession
      -> SetPracticeSpeed濡?吏꾪뻾 以??띾룄 蹂寃?
      -> GetAudioStats / PollJudgeEvent 諛섎났 ?몄텧
  -> StopSession
  -> Shutdown
```

### 湲고? ?곌껐 ?뚯뒪??

```text
InitializeAudioTest
  -> StartAudioTest
      -> GetAudioTestOutputLevelDb 諛섎났 ?몄텧
  -> StopAudioTest
  -> Shutdown
```

湲고? ?낅젰??異쒕젰 ?μ튂濡?洹몃?濡??꾨떖???곌껐怨??뚮━瑜??뺤씤?⑸땲?? ??寃쎈줈?먯꽌???먯젙 ?ㅻ젅??
?쇱튂 遺꾩꽍, 怨??ъ깮, 紐⑤땲??DSP瑜??쒖옉?섏? ?딆뒿?덈떎. ?몄뀡 API? ?숈떆???ъ슜?????놁뒿?덈떎.

`StartSlowPracticeSession`? 梨꾨낫 ?먯젙? ?좎??섍퀬 怨??뚯씪 ???硫뷀듃濡쒕냸??異쒕젰???욎뒿?덈떎.
珥덇린 ?띾룄??100%?대ŉ, `SetPracticeSpeed`濡?25%~125% 踰붿쐞?먯꽌 利됱떆 諛붽? ???덉뒿?덈떎.

### ?댁? ?곗뒿

```text
Initialize
  -> LoadChart
  -> StartFingeringPracticeSession
      -> GetAudioStats / PollJudgeEvent 諛섎났 ?몄텧
  -> StopSession
  -> Shutdown
```

?꾩옱 ?명듃?먯꽌 梨꾨낫 吏꾪뻾??硫덉땅?덈떎. ?щ컮瑜??낅젰? ?ㅼ쓬 ?명듃濡?吏꾪뻾?섍퀬, ?由??낅젰?
`Miss` ?대깽?몃쭔 諛쒖깮?쒗궎硫??꾩옱 ?명듃??癒몃춦?덈떎. 怨≪? ?ъ깮?섏? ?딆뒿?덈떎.

`Initialize`???대??먯꽌 湲곗〈 由ъ냼?ㅻ? 癒쇱? ?뺣━???????ㅻ뵒???ㅽ듃由쇱쓣 以鍮꾪빀?덈떎.
`ResetSessionTime`? ???몄뀡 ?쒖옉 ?꾩뿉 ?대? ?쒓컙怨??먯젙 吏꾪뻾 ?곹깭瑜?0遺???ㅼ떆 ?쒖옉?섎룄濡?珥덇린?뷀빀?덈떎.
`LoadChart`??梨꾨낫 ?먯젙?먮쭔 ?꾩슂?섎ŉ, 湲고? ?낅젰留??ъ슜???뚮뒗 ?몄텧?섏? ?딆븘???⑸땲??
`StartSession`? ?ㅻ뵒???ㅽ듃由쇨낵 ?먯젙 ?ㅻ젅?쒕? ?쒖옉?섎?濡?`PollJudgeEvent`, `PollGuitarInputEvent`瑜??ъ슜?섍린 ?꾩뿉 諛섎뱶???몄텧?댁빞 ?⑸땲??
???몄뀡? 梨꾨낫 ?먯젙 ?먮뒗 湲고? ?낅젰 以??섎굹??紐⑤뱶濡쒕쭔 ?숈옉?⑸땲??
`LoadChart`瑜??몄텧?????쒖옉???몄뀡? 梨꾨낫 ?먯젙 紐⑤뱶?대ŉ, `LoadChart` ?놁씠 ?쒖옉???몄뀡? 湲고? ?낅젰 紐⑤뱶?낅땲??
?몄뀡 醫낅즺 ?쒖뿉??`StopSession`???몄텧?섍퀬, ?뚮윭洹몄씤?????댁긽 ?곗? ?딆쓣 ??`Shutdown`???몄텧?⑸땲??

### ?ㅻ뵒???듭뀡 ?붾㈃

?듭뀡 ?붾㈃? WASAPI ?μ튂? ASIO ?쒕씪?대쾭瑜?蹂꾨룄 寃쎈줈濡?議고쉶?⑸땲?? 湲곕낯 ?곌껐? Windows
湲곕낯 WASAPI ?μ튂?대ŉ, ASIO ?쒕씪?대쾭???ъ슜?먭? 紐낆떆?곸쑝濡??좏깮???뚮쭔 ?쎈땲??

```text
# WASAPI ?μ튂
GetAudioDeviceCount
  -> GetAudioDeviceInfo(index)
  -> InitializeWithAudioDevice(...)

# ASIO ?쒕씪?대쾭
GetAsioDriverCount
  -> GetAsioDriverInfo(index)              // ?깅줉 ?대쫫留?議고쉶, ?쒕씪?대쾭瑜??댁? ?딆쓬
  -> SelectAsioDriver(name)                // ?좏깮???쒕씪?대쾭瑜??닿퀬 梨꾨꼸 ??議고쉶
  -> InitializeWithAsioDriver(name, ...)   // ?대떦 ASIO ?쒕씪?대쾭濡??ㅽ듃由?珥덇린??```

`GetAudioDeviceInfo`??`id`??`InitializeWithAudioDevice`??`inputDeviceId`?
`outputDeviceId`??洹몃?濡??꾨떖?⑸땲?? `inputChannels`???대떦 ?μ튂???낅젰 梨꾨꼸 ?섏씠硫?
UI?먯꽌??1遺??`inputChannels`源뚯? ?쒖떆?⑸땲?? ?ъ슜?먭? 怨좊Ⅸ n踰덉㎏ 梨꾨꼸?
`channels = 1`, `inputOffset = n - 1`濡?珥덇린?뷀빀?덈떎. RtAudio??臾쇰━ ?낅젰??蹂꾨룄 ?대쫫??
?쒓났?섏? ?딆쑝誘濡?梨꾨꼸 ?쒖떆??`Input 1`, `Input 2`泥섎읆 ?쒕쾲?쇰줈 援ъ꽦?⑸땲??

ASIO 紐⑸줉?먮뒗 `GetAudioDeviceCount`? `GetAudioDeviceInfo`瑜??ъ슜?섎㈃ ???⑸땲??
???⑥닔??WASAPI ?μ튂留?議고쉶?⑸땲??
`GetAsioDriverInfo`??Windows ?덉??ㅽ듃由ъ쓽 ?깅줉 ?대쫫留??쎌뒿?덈떎.
`SelectAsioDriver`???좏깮???쒕씪?대쾭留?`ASIOInit`?쇰줈 ?댁뼱 梨꾨꼸 ?섎? 諛섑솚?섍퀬, ?ㅼ쓬
?쒕씪?대쾭 ?좏깮 ?먮뒗 `Shutdown`源뚯? ?대┛ ?곹깭濡??좎??⑸땲??
`InitializeWithAsioDriver`?????좏깮 ?곹깭瑜??レ? ??RtAudio ASIO 諛깆뿏?쒕줈 媛숈? ?대쫫???쒕씪?대쾭瑜?李얠븘 ?ㅼ젣 duplex ?ㅽ듃由쇱쓣 ?쎈땲?? ?낅젰怨?異쒕젰? ?대떦 ?쒕씪?대쾭??梨꾨꼸 offset???ъ슜?⑸땲??

怨??뚮웾? `SetSongVolume`?쇰줈 ?ㅼ젙?⑸땲?? ?대뒗 湲고? ?낅젰??紐⑤땲??寃뚯씤怨?蹂꾧컻?대ŉ,
湲곗〈 `SetDSPParams`??`inputGain`怨?`outputGain`? 洹몃?濡??좎??⑸땲??

## Unity C# ?좎뼵 ?덉떆

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
        public int isPaused;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct SongSyncInfo
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string audioFile;
        public int audioOffsetMs;
        public int durationMs;
        public double songTimeMs;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct AsioDriverInfo
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string name;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct AudioDeviceInfo
    {
        public uint id;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string name;

        public uint inputChannels;
        public uint outputChannels;
        public uint duplexChannels;
        public int isDefaultInput;
        public int isDefaultOutput;
        public uint preferredSampleRate;
    }

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern string GetPluginVersion();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int Initialize(
        uint channels,
        uint sampleRate,
        uint inputDevice,
        uint outputDevice,
        uint inputOffset,
        uint outputOffset);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern uint GetAsioDriverCount();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetAsioDriverInfo(uint driverIndex, out AsioDriverInfo outInfo);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int SelectAsioDriver(
        [MarshalAs(UnmanagedType.LPStr)] string driverName,
        out AudioDeviceInfo outInfo);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int InitializeWithAsioDriver(
        [MarshalAs(UnmanagedType.LPStr)] string driverName,
        uint channels,
        uint sampleRate,
        uint inputOffset,
        uint outputOffset);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern uint GetAudioDeviceCount();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetAudioDeviceInfo(uint deviceIndex, out AudioDeviceInfo outInfo);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int InitializeWithAudioDevice(
        uint channels,
        uint sampleRate,
        uint inputDeviceId,
        uint outputDeviceId,
        uint inputOffset,
        uint outputOffset);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int InitializeAudioTest(
        uint channels,
        uint sampleRate,
        uint inputDeviceId,
        uint outputDeviceId,
        uint inputOffset,
        uint outputOffset);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int StartAudioTest();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void StopAudioTest();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetAudioTestOutputLevelDb(out float outLevelDb);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int LoadChart(
        [MarshalAs(UnmanagedType.LPStr)] string chartPath);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ResetSessionTime();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int StartSession();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int StartSlowPracticeSession();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetPracticeSpeed(float speed);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int StartFingeringPracticeSession();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void StopSession();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void PauseSession();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ResumeSession();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int RestartSession();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetDSPParams(float inputGain, float outputGain, float lpfAlpha);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetSongVolume(float volume);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int PollJudgeEvent(out JudgeEvent outEvent);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int PollGuitarInputEvent(out GuitarInputEvent outEvent);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetAudioStats(out AudioStats outStats);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetSongSyncInfo(out SongSyncInfo outInfo);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void Shutdown();
}
```

## JudgeResult

| 媛? | ?대쫫      | ?섎?                                      |
| --- | --------- | ----------------------------------------- |
| 0   | `Perfect` | ??대컢 ?ㅼ감媛 60 ms ?댄븯?닿퀬 ?쇱튂媛 ?쇱튂  |
| 1   | `Good`    | ??대컢 ?ㅼ감媛 140 ms ?댄븯?닿퀬 ?쇱튂媛 ?쇱튂 |
| 2   | `Bad`     | ??대컢 ?ㅼ감媛 240 ms ?댄븯?닿퀬 ?쇱튂媛 ?쇱튂 |
| 3   | `Miss`    | ??대컢 踰붿쐞瑜?踰쀬뼱?ш굅???쇱튂媛 遺덉씪移?   |

`single` ?대깽?몄쓽 ?쇱튂 ?먯젙? 紐⑺몴 MIDI? 媛먯? MIDI媛 ?뺥솗??媛숈븘???듦낵?⑸땲??
`chord` ?대깽?몃뒗 ?꾨옒??CG-FPM 肄붾뱶 ?먯젙 寃곌낵媛 ?쇱튂?댁빞 ?듦낵?⑸땲??

## 梨꾨낫 interpretation怨?肄붾뱶 ?먯젙

`schemaVersion: "0.2.0"`??媛?`notes` ?대깽?몃뒗 `interpretation`?쇰줈 ?먯젙 諛⑹떇??吏?뺥빀?덈떎.

| `interpretation` | 梨꾨낫 ?꾩닔 ?뺣낫 | ?먯젙 諛⑹떇 | ?뚮윭洹몄씤 ?대깽????|
| ---------------- | -------------- | --------- | ------------------ |
| `single` | `string`, `fret`, `finger`, `technique` | aubio MIDI ?쇱튂 ?먯젙 | ?명듃??1媛?|
| `chord` | `chordId`, `strumTechnique` | CG-FPM 肄붾뱶 ?먯젙 | 肄붾뱶 ?대깽?몃떦 1媛?|

`chordId`??`track.chordDefinitions`??`id`瑜?李몄“?⑸땲?? ?뚮윭洹몄씤? ?대떦 肄붾뱶??`fingering`怨?`track.tuning`?쇰줈 ?ㅼ젣 MIDI ??紐⑸줉??怨꾩궛?⑸땲?? 裕ㅽ듃(`fret: -1`) 以꾩? 肄붾뱶 ?먯젙 ??곸뿉???쒖쇅?⑸땲??

肄붾뱶???댁???以꾨쭏???⑥쓬 ?대깽?몃줈 遺꾨━?섏? ?딆뒿?덈떎. ?섎굹??chord ?대깽?멸? ?섎굹???먯젙 ??곸씠硫? `noteName`?먮뒗 肄붾뱶 湲고샇(`chordDefinitions[].symbol`)媛 ?닿퉩?덈떎.

### CG-FPM: Chart-Guided Fundamental Presence Matching

CG-FPM? 梨꾨낫 ?댁???媛?MIDI 湲곕낯?????뿉 ?먮꼫吏媛 ?덈뒗吏 ?뺤씤?섎뒗 諛⑹떇?낅땲?? 肄붾뱶 ?꾩껜瑜??꾨낫 以묒뿉??遺꾨쪟?섏? ?딆쑝硫? ???뚯쓽 諛곗쓬???ㅻⅨ 紐⑺몴?뚯쓽 洹쇨굅媛 ?섏? ?딅룄濡?湲곕낯?????쭔 ?ъ슜?⑸땲??

1. 肄붾뱶 onset ??160 ms ?숈븞 紐⑤끂 ?낅젰 ?섑뵆???섏쭛?⑸땲??
2. Hann window? 16,384-point FFT瑜??곸슜?⑸땲??
3. 梨꾨낫 ?댁? MIDI 媛곴컖??湲곕낯??二쇳뙆??二쇰? 5媛?FFT bin ?먮꼫吏瑜?怨꾩궛?⑸땲??
4. 媛?紐⑺몴 湲곕낯??二쇰??먯꽌 理쒓컯 ?ㅽ럺?몃읆 ?쇳겕媛 ?대떦 紐⑺몴 湲곕낯??FFT bin???덈뒗吏 ?뺤씤?⑸땲??
5. 紐⑤뱺 紐⑺몴???먮꼫吏媛 紐⑺몴??以?理쒓컯 湲곕낯???먮꼫吏??15% ?댁긽?대㈃ ?쇱튂濡??먯젙?⑸땲??

?덈? ?ㅼ뼱 G5(`G2`, `D3`)????98 Hz? 147 Hz ???씠 紐⑤몢 ?덉뼱???듦낵?⑸땲?? ?먰븳 媛????二쇰???理쒓컯 ?쇳겕媛 紐⑺몴 二쇳뙆?섏뿉 ?덉뼱???섎?濡? ?몄젒??F#5??G#5泥섎읆 ?ㅻⅨ 二쇳뙆?섍? ??媛뺥븳 ?낅젰? 嫄곗젅?⑸땲?? G2??3李?諛곗쓬????294 Hz??D3??湲곕낯?????씠 ?꾨땲誘濡?D3??洹쇨굅濡??ъ슜?섏? ?딆뒿?덈떎.

### CG-HCM: Chart-Guided Harmonic Chroma Matching (蹂댁〈?? ?꾩옱 誘몄궗??

CG-HCM? 梨꾨낫媛 ?붽뎄?섎뒗 肄붾뱶媛 ?낅젰 ?ㅽ럺?몃읆???ы븿?먮뒗吏 ?뺤씤?섎뒗 諛⑹떇?낅땲?? 肄붾뱶 ?먯껜瑜??꾩껜 ?꾨낫 以묒뿉???덈줈 遺꾨쪟?섏? ?딆뒿?덈떎.

1. 肄붾뱶 onset ??160 ms ?숈븞 紐⑤끂 ?낅젰 ?섑뵆???섏쭛?⑸땲??
2. Hann window? 16,384-point FFT瑜??곸슜?⑸땲??
3. 梨꾨낫 ?댁? MIDI? ?몄젒 ?ν?釉뚯뿉??湲곕낯??諛?理쒕? 6媛?諛곗쓬???먮꼫吏瑜?怨꾩궛?⑸땲??
4. ?먮꼫吏瑜?12媛?pitch class chroma濡??⑹궛?⑸땲??
5. 紐⑺몴 肄붾뱶??chroma ?먮꼫吏 鍮꾩쑉??0.65 ?댁긽?닿퀬, 紐⑤뱺 紐⑺몴 pitch class媛 理쒓컯 pitch class ?먮꼫吏??15% ?댁긽?대㈃ ?쇱튂濡??먯젙?⑸땲??

160 ms ?섏쭛 援ш컙? ?ㅽ듃?쇱뿉??以꾨쭏???뚮━媛 ?섎뒗 ?쒖젏 李⑥씠瑜?諛섏쁺?⑸땲?? ?곕씪???ㅼ쓬 肄붾뱶 onset??160 ms ?대궡???ㅼ뼱?ㅻ㈃ ???ㅽ듃?쇱쓽 ?ㅽ럺?몃읆??寃뱀퀜 ?먯젙???붾뱾由????덉뒿?덈떎.

?꾩옱 諛⑹떇? ?쇰컲 ?ㅽ듃?쇨낵 而룻똿?먯꽌 ?ㅽ뿕 以묒엯?덈떎. ?쒕??몃뒗 諛곗쓬怨?吏???쒓컙??以꾩뼱?ㅼ뼱 ?몄떇瑜좎씠 ??쓣 ???덉쑝硫? 鍮좊Ⅸ ?곗냽 ?ㅽ듃??諛?肄붾뱶 援먯껜??蹂꾨룄 ?ㅽ뿕???꾩슂?⑸땲??

## JudgeEvent

`PollJudgeEvent`濡?媛?몄삤???명듃 1媛쒖쓽 ?먯젙 寃곌낵?낅땲??

| ?꾨뱶                | ???      | ?섎?                                  |
| ------------------- | ---------- | ------------------------------------- |
| `noteIndex`         | `int`      | 梨꾨낫???명듃 ?몃뜳?? 0遺???쒖옉        |
| `result`            | `int`      | `JudgeResult` 媛?                     |
| `judgedAudioTimeMs` | `double`   | ?뚮윭洹몄씤 ?ㅻ뵒???ㅽ듃由?湲곗? ?먯젙 ?쒓컖 |
| `judgedChartTimeMs` | `double`   | 梨꾨낫 湲곗? ?먯젙 ?쒓컖                   |
| `errorMs`           | `float`    | `judgedChartTimeMs - startMs`         |
| `detectedMidi`      | `int`      | `single`: 媛먯? MIDI. 媛먯??섏? ?딆쑝硫?0 / `chord`: 0 (?ъ슜 ???? |
| `targetMidi`        | `int`      | `single`: 紐⑺몴 MIDI / `chord`: 0 (?ъ슜 ???? |
| `stringNumber`      | `int`      | `single`: 湲고? 以?踰덊샇 / `chord`: 0 (?ъ슜 ???? |
| `fret`              | `int`      | `single`: ?꾨젢 踰덊샇 / `chord`: 0 (?ъ슜 ???? |
| `startMs`           | `int`      | 梨꾨낫 ?명듃 ?쒖옉 ?쒓컖                   |
| `noteName`          | `char[16]` | `single`: 紐⑺몴 ???대쫫. ?? `E2`, `F#3` / `chord`: 肄붾뱶 湲고샇. ?? `C`, `Am7` |

`chord` ?대깽?몄뿉 ???`detectedMidi`? `targetMidi`瑜?鍮꾧탳?섎㈃ ???⑸땲?? ???꾨뱶??0?대?濡? ?덈? ?ㅼ뼱 `MIDI 48/0`? 肄붾뱶??紐⑺몴 MIDI媛 0?대씪???살씠吏 ?낅젰???놁뿀?ㅻ뒗 ?살? ?꾨떃?덈떎.

## GuitarInputEvent

`PollGuitarInputEvent`濡?媛?몄삤??湲고? ?낅젰 ?대깽?몄엯?덈떎.

梨꾨낫 ?먯젙怨?蹂꾧컻濡? ?낅젰 ?ㅻ뵒?ㅼ뿉??onset??媛먯?????80 ms ?쇱튂 ?덉젙 援ш컙??湲곕떎由ш퀬 MIDI ?쇱튂媛 ?뺤씤?섎㈃ ?대깽?멸? 諛쒖깮?⑸땲??
MIDI ?쇱튂媛 媛먯??섏? ?딆쑝硫??대깽?몃? 諛쒖깮?쒗궎吏 ?딆뒿?덈떎.

| ?꾨뱶          | ???    | ?섎?                                   |
| ------------- | -------- | -------------------------------------- |
| `midi`        | `int`    | 媛먯???MIDI ?쇱튂                       |
| `audioTimeMs` | `double` | ?뚮윭洹몄씤 ?ㅻ뵒???ㅽ듃由?湲곗? onset ?쒓컖 |

## AudioStats

`GetAudioStats`濡?媛?몄삤???꾩옱 ?몄뀡 ?곹깭?낅땲??

| ?꾨뱶                 | ???    | ?섎?                                           |
| -------------------- | -------- | ---------------------------------------------- |
| `streamTime`         | `double` | ?몄뀡 湲곗? ?ㅽ듃由??쒓컙. 珥??⑥쐞                 |
| `audioTimeMs`        | `double` | ?ㅻ뵒???ㅽ듃由??쒓컙. ms ?⑥쐞                    |
| `chartTimeMs`        | `double` | 梨꾨낫 ?쒓컙. 泥쒖쿇???ъ깮 ?곗뒿?먯꽌???ㅼ젙???띾룄濡?吏꾪뻾, ?댁? ?곗뒿?먯꽌???꾩옱 ?명듃 ?쒓컖??怨좎젙 |
| `countdownMs`        | `double` | ?몄뀡 ?쒖옉 ??移댁슫?몃떎???쒓컙. ?꾩옱 5000        |
| `streamLatency`      | `int`    | RtAudio ?ㅽ듃由?吏??                           |
| `bufferFrames`       | `uint`   | ?대? ?ㅻ뵒??踰꾪띁 ?꾨젅???? ?꾩옱 128           |
| `droppedAudioBlocks` | `uint`   | ?먯젙 ?ㅻ젅?쒕줈 ?꾨떖?섏? 紐삵븳 ?ㅻ뵒??釉붾줉 ??    |
| `droppedJudgeEvents` | `uint`   | Unity媛 ??쾶 polling?댁꽌 踰꾨젮吏??먯젙 ?대깽????|
| `totalNotes`         | `int`    | 濡쒕뱶??梨꾨낫???꾩껜 ?명듃 ??                    |
| `nextNoteIndex`      | `int`    | ?ㅼ쓬 ?먯젙 ????명듃 ?몃뜳??                    |
| `isRunning`          | `int`    | ?ㅻ뵒???ㅽ듃由??ㅽ뻾 以묒씠硫?1, ?꾨땲硫?0          |
| `isFinished`         | `int`    | 紐⑤뱺 ?명듃 ?먯젙???앸궗?쇰㈃ 1, ?꾨땲硫?0          |
| `isPaused`           | `int`    | ?몄뀡???쇱떆?뺤? ?곹깭?대㈃ 1, ?꾨땲硫?0           |

`chartTimeMs`媛 0蹂대떎 ?묒쑝硫?移댁슫?몃떎??援ш컙?낅땲??

## Functions

### GetPluginVersion

```c
const char *GetPluginVersion(void);
```

濡쒕뱶???ㅼ씠?곕툕 ?뚮윭洹몄씤??踰꾩쟾 臾몄옄?댁쓣 諛섑솚?⑸땲??
珥덇린???꾪썑? 愿怨꾩뾾???몄텧?????덉뒿?덈떎.

?꾩옱 諛섑솚媛믪? `"0.4.8"`?낅땲??

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

?ㅻ뵒???ㅽ듃由? DSP ?곹깭, ?먯젙 ?? aubio ?쇱튂/?⑥뀑 媛먯?湲곕? 珥덇린?뷀빀?덈떎.

| ?뚮씪誘명꽣       | ?섎?                                 |
| -------------- | ------------------------------------ |
| `channels`     | ?낆텧??梨꾨꼸 ??                      |
| `sampleRate`   | ?섑뵆?덉씠?? ?? 48000                |
| `inputDevice`  | ?낅젰 ?μ튂 ?좏깮. 0?대㈃ 湲곕낯 ?낅젰 ?μ튂 |
| `outputDevice` | 異쒕젰 ?μ튂 ?좏깮. 0?대㈃ 湲곕낯 異쒕젰 ?μ튂 |
| `inputOffset`  | ?낅젰 ?μ튂???쒖옉 梨꾨꼸                |
| `outputOffset` | 異쒕젰 ?μ튂???쒖옉 梨꾨꼸                |

諛섑솚媛?

- `0`: ?깃났
- `-1`: ?ㅻ뵒???ㅽ듃由??앹꽦 ?ㅽ뙣

### InitializeAudioTest

```c
int InitializeAudioTest(
    unsigned int channels,
    unsigned int sampleRate,
    unsigned int inputDeviceId,
    unsigned int outputDeviceId,
    unsigned int inputOffset,
    unsigned int outputOffset);
```

湲고? ?곌껐???뺤씤?섍린 ?꾪븳 ?낅젰-異쒕젰 ?⑥뒪?ㅻ（ ?ㅽ듃由쇱쓣 珥덇린?뷀빀?덈떎. `inputDeviceId`?
`outputDeviceId`?먮뒗 `GetAudioDeviceInfo`濡??살? `id`瑜??꾨떖?섎ŉ, `0`? 媛?湲곕낯 ?μ튂?낅땲??
?낅젰 PCM? DSP 泥섎━ ?놁씠 異쒕젰?쇰줈 蹂듭궗?⑸땲??

諛섑솚媛?

- `0`: ?깃났
- `-1`: ?ㅻ뵒???ㅽ듃由??앹꽦 ?ㅽ뙣

### StartAudioTest

```c
int StartAudioTest(void);
```

`InitializeAudioTest`濡?以鍮꾪븳 ?⑥뒪?ㅻ（ ?ㅽ듃由쇱쓣 ?쒖옉?⑸땲?? 湲고?瑜??곗＜?섎㈃ ?대떦 ?낅젰??
?좏깮??異쒕젰 ?μ튂濡??ъ깮?⑸땲??

諛섑솚媛?

- `0`: ?깃났
- `-1`: ?ㅻ뵒???뚯뒪?멸? 珥덇린?붾릺吏 ?딆븯嫄곕굹 ?ㅽ듃由??쒖옉 ?ㅽ뙣

### StopAudioTest

```c
void StopAudioTest(void);
```

?ㅽ뻾 以묒씤 湲고? ?곌껐 ?뚯뒪???ㅽ듃由쇱쓣 ?뺤??⑸땲??

### GetAudioTestOutputLevelDb

```c
int GetAudioTestOutputLevelDb(float *outLevelDb);
```

理쒓렐 ?⑥뒪?ㅻ（ 異쒕젰 踰꾪띁??RMS ?덈꺼??dBFS濡?蹂듭궗?⑸땲?? 湲고?瑜?移섏? ?딆븘 ?낅젰??臾댁쓬?대㈃
`-96 dBFS`?대ŉ, 湲고? ?낅젰???ㅼ뼱?ㅻ㈃ 媛믪씠 而ㅼ쭛?덈떎. ??媛믪? ?ㅼ젣 ?ㅽ뵾而??뚯븬(dB SPL)???꾨땶
?붿???PCM ?좏샇 ?덈꺼?낅땲??

諛섑솚媛?

- `0`: ?깃났
- `-1`: ?ㅻ뵒???뚯뒪?멸? 珥덇린?붾릺吏 ?딆쓬

### LoadChart

```c
int LoadChart(const char *chartPath);
```

?ㅼ쓬 ?몄뀡?먯꽌 ?ъ슜??梨꾨낫 JSON ?뚯씪??濡쒕뱶?⑸땲??
湲고? ?낅젰留??ъ슜???뚮뒗 ?몄텧?섏? ?딆븘???⑸땲??
梨꾨낫??`song.audioFile` MP3???④퍡 PCM?쇰줈 ?붿퐫?⑺빀?덈떎.

諛섑솚媛?

- `0`: ?깃났
- `-1`: 梨꾨낫 ?뚯씪 濡쒕뱶 ?ㅽ뙣

梨꾨낫??`schemaVersion`, `song`, `track`, `notes` 援ъ“瑜??ъ슜?⑸땲??
?명듃??`startTick`? BPM怨?resolution??湲곗??쇰줈 ms濡?蹂?섎맗?덈떎.

`0.2.0` chord ?대깽???덉떆:

```json
{
  "interpretation": "chord",
  "startTick": 480,
  "durationTick": 480,
  "chordId": "C_open",
  "strumTechnique": "down"
}
```

### ResetSessionTime

```c
void ResetSessionTime(void);
```

?대? ?몄뀡 ?쒓컖, ?ㅼ쓬 ?먯젙 ?명듃 ?몃뜳?? ?먯젙 ?? 醫낅즺 ?뚮옒洹몃? 珥덇린?뷀빀?덈떎.

??寃뚯엫???쒖옉?섍린 ???몄텧?섎㈃ `GetAudioStats`??`audioTimeMs`, `chartTimeMs`媛 ?댁쟾 ?몄뀡 ?쒓컙???곹뼢??諛쏆? ?딆뒿?덈떎.
`StartSession`???대??먯꽌 ???⑥닔瑜???踰??몄텧?⑸땲??

### StartSession

```c
int StartSession(void);
```

?ㅻ뵒???ㅽ듃由쇨낵 ?먯젙 ?ㅻ젅?쒕? ?쒖옉?⑸땲??

諛섑솚媛?

- `0`: ?깃났
- `-1`: 珥덇린?붾릺吏 ?딆븯嫄곕굹 ?ㅽ듃由??쒖옉 ?ㅽ뙣

?몄뀡 ?쒖옉 吏곹썑 5000 ms 移댁슫?몃떎?댁씠 ?곸슜?⑸땲??

### StartSlowPracticeSession

```c
int StartSlowPracticeSession(void);
```

怨????以鍮꾨맂 硫뷀듃濡쒕냸 ?④낵?뚯쓣 ?ъ깮?섎뒗 泥쒖쿇???ъ깮 ?곗뒿 ?몄뀡???쒖옉?⑸땲?? `LoadChart`瑜?
癒쇱? ?몄텧?댁빞 ?섎ŉ, 梨꾨낫??BPM怨?`audioOffsetMs`??留욎떠 4遺꾩쓬?쒕쭏???대┃???ъ깮?⑸땲??
梨꾨낫? 硫뷀듃濡쒕냸, 移댁슫?몃떎?댁? ?꾩옱 ?곗뒿 ?띾룄濡?吏꾪뻾?⑸땲??

### SetPracticeSpeed

```c
void SetPracticeSpeed(float speed);
```

泥쒖쿇???ъ깮 ?곗뒿??梨꾨낫 吏꾪뻾 ?띾룄瑜??ㅼ젙?⑸땲?? ?덉슜 踰붿쐞??`0.25f`~`1.25f`?대ŉ,
踰붿쐞瑜?踰쀬뼱??媛믪? 媛??媛源뚯슫 ?덉슜媛믪쑝濡??곸슜?⑸땲?? ?ㅽ뻾 以??몄텧?섎㈃ ?ㅼ쓬 ?ㅻ뵒??踰꾪띁遺??諛섏쁺?⑸땲??

### StartFingeringPracticeSession

```c
int StartFingeringPracticeSession(void);
```

怨≪쓣 ?ъ깮?섏? ?딄퀬 ?꾩옱 ?명듃??梨꾨낫 ?쒓컖?먯꽌 吏꾪뻾??硫덉땅?덈떎. ?щ컮瑜??쇱튂 ?먮뒗 肄붾뱶 ?낅젰?
`Perfect` ?대깽?몃? 諛쒖깮?쒗궎怨??ㅼ쓬 ?명듃濡??대룞?⑸땲?? ?由??낅젰? `Miss` ?대깽?몃? 諛쒖깮?쒗궎硫?
?꾩옱 ?명듃???⑥뒿?덈떎. 梨꾨낫???쒓컙 ?ㅼ감???먯젙???ъ슜?섏? ?딆뒿?덈떎.

### StopSession

```c
void StopSession(void);
```

?꾩옱 ?ㅻ뵒???ㅽ듃由쇱쓣 硫덉텛怨??먯젙 ?ㅻ젅?쒕? 醫낅즺?⑸땲??

### PauseSession

```c
void PauseSession(void);
```

?꾩옱 ?몄뀡???ㅻ뵒??異쒕젰怨??낅젰 ?먯젙??硫덉땅?덈떎. ?몄뀡怨?梨꾨낫 ?쒓컖? ?뺤????곹깭濡??좎??⑸땲??

### ResumeSession

```c
void ResumeSession(void);
```

`PauseSession`?쇰줈 硫덉텣 ?몄뀡??湲곗〈 吏꾪뻾?꾩뿉???ш컻?⑸땲??

### RestartSession

```c
int RestartSession(void);
```

?꾩옱 ?몄뀡??泥섏쓬遺???ㅼ떆 ?쒖옉?⑸땲?? ?ㅽ뻾 以묒씠???몄뀡 紐⑤뱶(?쇰컲 寃뚯엫, ?먮┛ ?곗뒿, ?댁? ?곗뒿)瑜??좎??섎ŉ,
?몄뀡 ?쒓컖怨??먯젙 吏꾪뻾?꾨? 珥덇린?뷀빀?덈떎.

諛섑솚媛?

- `0`: ?깃났
- `-1`: 珥덇린?붾릺吏 ?딆븯嫄곕굹 ?ㅽ듃由??쒖옉 ?ㅽ뙣

### SetDSPParams

```c
void SetDSPParams(float inputGain, float outputGain, float lpfAlpha);
```

紐⑤땲?곕쭅 ?ㅻ뵒?ㅼ쓽 DSP ?뚮씪誘명꽣瑜?媛깆떊?⑸땲??

泥섎━ ?쒖꽌:

```text
input gain -> tanh overdrive -> one-pole low-pass filter -> output gain
```

| ?뚮씪誘명꽣     | ?섎?                  | 湲곕낯媛?|
| ------------ | --------------------- | ------ |
| `inputGain`  | ?낅젰 寃뚯씤             | 4.0    |
| `outputGain` | 異쒕젰 寃뚯씤             | 0.5    |
| `lpfAlpha`   | low-pass filter alpha | 0.2    |

### PollJudgeEvent

```c
int PollJudgeEvent(JudgeEvent *outEvent);
```

?湲?以묒씤 ?먯젙 ?대깽?몃? ?섎굹 媛?몄샃?덈떎.

諛섑솚媛?

- `1`: ?대깽???덉쓬. `outEvent`??媛믪씠 蹂듭궗??
- `0`: ?대깽???놁쓬

Unity?먯꽌?????꾨젅???덉뿉??`0`???섏삱 ?뚭퉴吏 諛섎났 ?몄텧?섎㈃ ?⑸땲??

```csharp
while (PakNativePlugin.PollJudgeEvent(out var judgeEvent) == 1)
{
    // judgeEvent 泥섎━
}
```

### PollGuitarInputEvent

```c
int PollGuitarInputEvent(GuitarInputEvent *outEvent);
```

?湲?以묒씤 湲고? ?낅젰 ?대깽?몃? ?섎굹 媛?몄샃?덈떎.

諛섑솚媛?

- `1`: ?대깽???덉쓬. `outEvent`??媛믪씠 蹂듭궗??
- `0`: ?대깽???놁쓬

梨꾨낫 ?먯젙 ?대깽?몄? ?낅┰?곸쑝濡??숈옉?섎?濡?Client 議곗옉???낅젰???ъ슜?????덉뒿?덈떎.
湲고? ?낅젰 紐⑤뱶??梨꾨낫瑜?濡쒕뱶?섏? ?딆? ?곹깭?먯꽌 `StartSession`???몄텧???쒖옉?⑸땲??
梨꾨낫 ?먯젙 紐⑤뱶濡??쒖옉???몄뀡?먯꽌??`PollGuitarInputEvent`媛 ?대깽?몃? 諛섑솚?섏? ?딆뒿?덈떎.
Unity?먯꽌??MIDI 媛믪쓣 ?먰븯???숈옉??留ㅽ븨?섎㈃ ?⑸땲??

```csharp
while (PakNativePlugin.PollGuitarInputEvent(out var inputEvent) == 1)
{
    if (inputEvent.midi == 40) // E2
    {
        // ?뺤씤 泥섎━
    }
}
```

### GetAudioStats

```c
int GetAudioStats(AudioStats *outStats);
```

?꾩옱 ?ㅻ뵒???쒓컙, 梨꾨낫 吏꾪뻾?? drop count, 醫낅즺 ?щ?瑜?媛?몄샃?덈떎.

諛섑솚媛?

- `0`: ?깃났
- `-1`: 珥덇린?붾릺吏 ?딆쓬

### GetSongSyncInfo

```c
int GetSongSyncInfo(SongSyncInfo *outInfo);
```

濡쒕뱶??梨꾨낫??怨??뚯씪怨??ㅼ씠?곕툕 ?ㅻ뵒???쒓퀎???숆린?붾맂 ?ъ깮 ?쒓컖??媛?몄샃?덈떎.
怨≪? `LoadChart`?먯꽌 DLL ?대? PCM 踰꾪띁濡??쏀엳硫? `StartSession` ??移댁슫?몃떎?댁씠 ?앸굹硫?
湲고? ?낅젰怨?誘뱀떛?섏뼱 DLL??RtAudio 異쒕젰怨?DSP瑜??듦낵?⑸땲?? `StartSlowPracticeSession`?먯꽌??
怨????硫뷀듃濡쒕냸??誘뱀떛?⑸땲??

| ?꾨뱶 | ???| ?섎? |
| --- | --- | --- |
| `audioFile` | `char[260]` | 梨꾨낫 `song.audioFile` 寃쎈줈 |
| `audioOffsetMs` | `int` | 梨꾨낫 tick 蹂?섏뿉 ?ъ슜??`song.audioOffsetMs` |
| `durationMs` | `int` | 梨꾨낫??湲곕줉??怨?湲몄씠 |
| `songTimeMs` | `double` | 怨??쒖옉 湲곗? ?ъ깮 ?쒓컖. 泥쒖쿇???ъ깮 ?곗뒿?먯꽌??梨꾨낫 ?쒓컖 |

`songTimeMs`??DLL???ㅼ젣濡?異쒕젰?섎뒗 怨≪쓽 ?ъ깮 ?꾩튂?낅땲?? 移댁슫?몃떎??以묒뿉???뚯닔?대ŉ,
0 ?댁긽遺??怨?PCM??RtAudio 異쒕젰??誘뱀떛?⑸땲??

諛섑솚媛?

- `0`: ?깃났
- `-1`: 珥덇린?붾릺吏 ?딆븯嫄곕굹 梨꾨낫媛 濡쒕뱶?섏? ?딆쓬

### Shutdown

```c
void Shutdown(void);
```

?ㅻ뵒???ㅽ듃由? ?먯젙 ?ㅻ젅?? aubio 由ъ냼?ㅻ? ?댁젣?⑸땲??

## Unity 梨꾨낫 ?먯젙 猷⑦봽 ?덉떆

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
        // ?먯젙 UI / ?먯닔 泥섎━
    }

    if (stats.isFinished == 1)
    {
        // 寃곌낵 ?붾㈃
    }
}
```

## Unity 怨??ъ깮 ?쒓컖 議고쉶 ?덉떆

```csharp
void Update()
{
    if (PakNativePlugin.GetSongSyncInfo(out var song) != 0)
        return;

    // song.songTimeMs??DLL??異쒕젰 以묒씤 怨??꾩튂??
    songProgressSlider.value = (float)(song.songTimeMs / song.durationMs);
}
```

Unity `AudioSource`濡?媛숈? 怨≪쓣 蹂꾨룄濡??ъ깮?섎㈃ ?댁쨷 異쒕젰?섎?濡??ъ슜?섏? ?딆뒿?덈떎.

## Unity 湲고? ?낅젰 猷⑦봽 ?덉떆

```csharp
void Update()
{
    if (PakNativePlugin.GetAudioStats(out var stats) != 0)
        return;

    while (PakNativePlugin.PollGuitarInputEvent(out var inputEvent) == 1)
    {
        if (inputEvent.midi == 40)
        {
            // E2 ?낅젰 泥섎━
        }
    }
}
```

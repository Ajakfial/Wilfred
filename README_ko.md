# (이 README는 CURSOR AI가 작성했으며, 코드를 포함한 나머지 모든 내용은 사람이 직접 작성했습니다.)

# Wilfred

**웹사이트:** https://ajakfial.github.io/Wilfred/

Wilfred는 빠르고 가볍고 크로스 플랫폼 데스크톱 검색 엔진 및 애플리케이션 런처입니다. 파일, 폴더, 애플리케이션, 브라우저 통합, 계산기, 필터링된 쿼리를 단일 호출 가능한 검색창에서 실행할 수 있도록 OS의 깊은 검색 계층으로 설계되었습니다.

**빠름. 깊이. 모듈식. 낮은 리소스. 크로스 플랫폼. 확장 가능.**

## 기능

* 쓰기 전용 로그를 갖춘 증분적, 영구적 파일명/경로/메타데이터 인덱스
* 파일 시스템 감시자 (ReadDirectoryChangesW, FSEvents, inotify) 및 재스캔 폴백
* 퍼지 매칭, 약어, 토큰/경로 랭킹, 최신성 및 빈도 신호
* 오타 안전: Damerau (전치 인식) near-miss 매칭 (이름, 토큰, 경로) + `혹시` 수정 (명령, 기록, 파일명)
* 인라인 자동완성: 고스트 완성 (`→`로 수락) + 기록, 명령, 인덱스에서 상위 N 후보
* 컨텍스트 인식 랭킹 (최근 폴더, 파일 유형, 시간대, 세션 쿼리)
* 클립보드를 보조 소스로 활용 (텍스트 매치, 복사된 경로, 클립 히스토리)
* 영구 클립보드 관리자 (`clips [query]`, 고정, `clips clear`, `clips url|email|path|code|ip`)
* 소스, 설정, 텍스트 문서에 대한 콘텐츠 인덱싱
* 실제 문서 텍스트 추출 (PDF, Office, RTF, HTML)을 콘텐츠 인덱스로
* 선택적 시맨틱 검색: 로컬 임베딩 (llama.cpp 서버/모델 또는 내장 해시) + HNSW 벡터 인덱스 (`vectors.bin`을 WAL/스냅샷 옆에 배치) + 트라이그램 소프트 매치 병합 (`providers.semantic`, `embedding.*`)
* 선택적 AI 어시스턴트 (`ai ...`/`ask ...`, 화면 인식 `ai see ...`) 와 고유 키: OpenAI, Anthropic, Gemini, Groq
* 폴더, `{date}`/`{clipboard}`/`{query}` 플레이스홀더, 선택적 전역 약어(모든 앱에서 사용 가능)를 가진 스니펫
* 빠른 노트 + 할 일 (`note ...`, `todo ...`, 데이터 디렉터리에 지속)
* 포모도로 / 카운트다운 타이머 + 스톱워치 (`timer`, `pomodoro`, `stopwatch`)
* 오버레이 내 리치 파일 프리뷰 (문서, CSV, 이미지, 폴더) (`F3`) + `wilfred preview`
* 캘린더 (`.ics`), 연락처 (`.vcf`), 메모 (Markdown) 소스 + 선택적 OCR 인덱싱 via `tesseract` (`sources.*`)
* 크로스 플랫폼 오버레이 (Windows WebView2 최상위, macOS 플로팅 패널, Linux WebKitGTK + 자동 X11 캔버스 폴백, Wayland 컴포지터에서 레이어 셸 앵커, 모두 상단/작업표시줄 건너뛰기) + `Ctrl`/`⌘`+`K` 액션 팝오버 및 두 Linux 백엔드에서 일치하는 단축키
* 브라우저 북마크, 기록, 열린 탭 (`bm`, 프로바이더 기반)
* Windows에서 USN 저널을 통한 즉각적인 NTFS 전체 디스크 열거
* 공식 OS 인덱스 연동 (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` 가 Wilfred 인덱스가 놓친 공백을 채움 (`os <query>` 강제 적용, 중복 제거)
* 오버레이 내 프리뷰 팬 (`F3`) + `wilfred preview`, 체인된 결과 액션 (`open+copy_path`) + 명명된 다중 단계 워크플로 (`workflows:`)
* 날씨, 시간, 디스크, RAM, CPU, 프로세스, 배터리, 창, 타이머, 노트, 미디어, 네트워크, 레이아웃 등에 대한 미니 결과
* 창 관리 (`windows`, `minimize`/`maximize`/`close window`, 스냅) + 저장된 창 레이아웃 (`layout save`, `layout`)
* 화면 인식 AI (`ai see ...` 스크린샷을 비전 입력으로 공급자 전송)
* 빠른 메모가 실제 마크다운 파일로 존재, 카드와 노트 검색 모두에서 검색 가능
* 시스템 전체 자동화: 추가 전역 핫키 (`hotkeys:`), `wilfred workflow` / `wilfred exec` CLI 트리거
* 검색 매크로 (`!yt`, `gh`, `wiki`, 설정의 커스텀 템플릿) + 매개변수화된 퀵링크 (`quicklinks:` `{1}`/`{*}`/`{query}`)
* 내장 파일 액션 위에 앱별 추가 컨텍스트 액션 (`app_actions:`)
* 대용량 파일 찾기 + 인덱스에서 중복 후보 (`large`, `dupes`)
* 파일 변환 (`convert song.wav to mp3`, `convert photo.bmp to png`: 네이티브 WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg로 mp3/ogg/flac/jpg/webp) + 배경 제거 (`bgremove photo.png` 투명 PNG, 모델 불필요)
* 프로세스 킬러 (`kill <pid|name>`), 네이티브 미디어 키 (`media play/next/mute/vol` -- 도우미 설치 불필요), 네트워크 도구 (`ping`, `dns`, `myip`), 음성-텍스트 (`transcribe` whisper CLI 또는 내장 spoke 엔진 경유)
* 조합 가능한 필터 (`*.cpp in Projects`, `type:image`, `size:>10mb`, 명명된 스코프)
* 애플리케이션 발견 (시작 메뉴 / `.app` 번들 / `.desktop` 파일)
* 리치 파일 액션 (경로 복사 변형, SHA-256 해시, zip, 터미널/에디터 여기서, 새 파일/폴더, 열기)
* 기본 브라우저 감지, URL 열기, 웹 검색 폴백
* 안전한 표현식 파서 (산술, 함수, 단위/통화/색상/날짜 변환, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, 비트, regex/url/jwt)
* 네이티브 및 stdio 플러그인, 프로바이더를 통한 추가 검색 백엔드
* 단위, 통화, 색상, 날짜, 소규모 개발 유틸리티를 갖춘 계산기
* 오버레이에서 시스템 명령 (`lock`, `sleep`, `shutdown`, `restart`, `logout`, 휴지통 비우기)
* 시스템 토글 (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + 설정 딥링크 (`settings wifi/bluetooth/sound/display/...`) -- Windows/macOS/Linux 실제 OS 백엔드, 모바일/BSD 갭에서는 정직한 미지원 에러
* 패키지 관리자 검색 (`winget firefox`, `brew switch`, `apt vlc` -- 엔터로 설치, `Tab`으로 명령 복사; `packages`로 감지된 관리자 나열)
* 설정 + 설정 (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) 및 옵트인 원격 검색 백엔드 (`remotes:` -- 사용자 엔드포인트만, 기본 꺼짐; 소스별 `headers:` + `max_results:`)
* 최초 사용 시 신뢰 플러그인 (`plugins` 승인 카드, `wilfred plugin list/install/approve/revoke`, 선택적 `plugins.registry` 인덱스, sha256 검증 설치)
* 창 타일링 (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + 모니터 변경 자동 적용 (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* 안드로이드 앱 (Kotlin UI + NDK를 통한 C++ 코어 → `.apk`): 떠다니는 W 버튼, 검색창, 동일한 인덱스/검색/랭킹 -- `docs/android.md` 참조
* iOS 앱 (SwiftUI + 동일한 C++ 코어 → 시뮬레이터 `.app`): 안드로이드와 같은 검색 UI (앱 아이콘 진입; iOS는 오버레이 버튼 금지), 샌드박스 전용 인덱스 -- `docs/ios.md` 참조
* 로컬 백업/복원 및 선택적 원격 동기화 (`backup`, `restore`, `sync-push`, `sync-pull`)
* 다른 런처에서 가져오기 (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- `docs/import.md` 참조
* 검증, `이게 무슨 뜻인가요` 힌트, 사람이 읽을 수 있는 에러를 가진 YAML 설정
* 전역 핫키: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), 구성 가능
* 로컬 검색 기록 (선택 사항; 비활성화 가능 또는 `wilfred history-clear`)
* 오버레이 UI + CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## 빌드

CMake 3.16+ 및 C++20 컴파일러 (MSVC, Apple Clang, GCC) 필요.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Linux에서 X11 개발 헤더로 오버레이 활성화 (`libx11-dev`); Wayland 세션에서 `libgtk-layer-shell-dev` 설치 시 WebKit 오버레이가 네이티브 레이어 셸 서피스가 됨 (Sway/wlroots, KDE Plasma -- GNOME은 레이어 셸 프로토콜 없음, 창은 일반 toplevel 유지).

## 실행

```bash
# 백그라운드 데몬: 오버레이, 인덱서, 핫키
wilfred

# 로컬 인덱스에 대한 원샷 검색
wilfred search firefox

# 검색 및 상위 결과 열기
wilfred launch firefox

# 구성된 루트 즉시 인덱싱
wilfred index

# 통계
wilfred status

# 다른 런처에서 설정 가져오기
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
```

첫 실행 시 사용자 설정 생성:

| 플랫폼 | 설정 | 인덱스 데이터 |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

전체 스키마 (인덱스 루트, 제외, 시스템 디렉토리, 랭킹 가중치, 별칭, 핫키, 기록, 브라우저)는 `config/wilfred.default.yml` 참조.

## 다른 런처에서 마이그레이션

`wilfred import`는 핫키, 사용자 정의 웹 검색 (`macros:` + `quicklinks:`로), 스니펫, 별칭, 테마, 기본 검색 템플릿을 `wilfred.yml`로 가져옵니다. 병합이 기본값, `--overwrite`로 충돌 교체, `--dry-run`으로 미리보기, 실제 임포트 전 `.pre-import.bak` 백업 작성.

| OS | 지원 소스 |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` 번들), Raycast (퀵링크 JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

자세한 런처별 매핑은 `docs/import.md` (및 `wilfred import --list`) 참조.

## 쿼리 언어 (예시)

| 쿼리 | 의미 |
|---|---|
| `firefox` | 퍼지 파일/앱 검색 |
| `25 * 42` | 계산기 |
| `https://example.com` | URL 열기 |
| `? 고양이` / `g 고양이` | 기본 브라우저에서 웹 검색 |
| `*.cpp in Projects` | 확장자 + 디렉터리 필터 |
| `type:image name:logo` | 종류 + 파일명 |
| `content:widget` / `intext foo` | 인덱싱된 파일 내용 검색 |
| `> notepad` | 명령 / 실행 스타일 |
| `scope:home 메모` | 설정에서 명명된 디렉터리 그룹 |
| `날씨` / `날씨 서울` | 미니 카드: 로컬 또는 도시 예보 |
| `speedtest` | 실시간 핑, 다운로드, 업로드 (`speedtest again` 재실행) |
| `시간` `디스크` `디스크사용` `ram` `cpu` | 시계, 드라이브, 메모리, 프로세서 |
| `프로세스 chrome` / `top` | 실시간 프로세스 CPU, RAM, 스레드 (`kill <pid|name>`로 종료) |
| `스크린샷` / `스크린샷 window` / `스크린샷 region` | 전체 화면, 윈도우, 영역 캡처 → `Pictures/Wilfred` |
| `emoji 미소` / `symbol 유로` | 이모지 및 심볼 선택기 (엔터로 복사) |
| `100 usd를 eur로` / `fx 25 gbp jpy` | 통화 변환 |
| `1 큰술을 g로` / `1 컵 밀가루를 g로` | 요리 부피↔질량 (물 기본, 선택적 재료) |
| `#ff5500` / `rgb(255, 85, 0)` / `색상 coral` | 색상 변환 (hex / rgb / hsl) |
| `오후 3시 est를 pst로` / `now in tokyo` / `tz 런던` | 시간대 |
| `오늘 + 7일` / `2024-01-31 + 1개월` | 날짜 계산 |
| `uuid` / `base64 안녕` / `sha256 abc` / `lorem 12` / `json {"a":1}` | 개발 유틸리티 (엔터로 복사) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | 진법 + 비트 도구 |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | 정규식 테스터, URL 코덱, JWT 디코드 |
| `timer 10m` / `pomodoro` / `stopwatch start` | 카운트다운, 포모도로 프리셋, 스톱워치 |
| `note 우유 사기` / `notes` / `todo 보내기` / `todos` | 빠른 메모 + 할 일 (`todo done 1`) |
| `media play` / `media next` / `media mute` | 네이티브 미디어 키 + 볼륨 (Win 미디어 키; mac HID + Music/Spotify/VLC; Linux 네이티브 MPRIS D-Bus + wpctl/pactl/amixer, playerctl 선택) |
| `ping example.com` / `dns example.com` / `myip` | 핑, DNS 조회, 공개 IP |
| `large 10` / `dupes` | 인덱스에서 가장 큰 파일 + 중복 후보 |
| `transcribe 이야기.mp3` / `dictate 10` | mp3/mp4 오디오 음성-텍스트, 마이크 받아쓰기 (`transcription.engine`: whisper CLI 또는 내장 spoke 엔진) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | 파일 변환 (네이티브 PCM + BMP/PNG/PPM/TGA, ffmpeg로 압축) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | 배경 제거로 투명 PNG (자동 모서리 또는 명시적 색상) |
| `창 chrome` / `최소화 spotify` | 창 나열, 전환, 최소화, 최대화, 닫기, 스냅 |
| `layout save 작업` / `layout 작업` | 창 레이아웃 저장 및 복원 |
| `define resilient` / `thesaurus happy` | 오프라인 정의 + 동의어 (엔터로 복사) |
| `pins` / `pin firefox` / `unpin firefox` | 고정된 즐겨찾기 (`ranking.pinned`로 부스트) |
| `event add 팀 동기화 | 내일 10am` | 캘린더 이벤트 생성 (`.ics` 작성) |
| `contact add Jane Doe jane@x.com 555-0100` | 연락처 생성 (`.vcf` 작성) |
| `rename ./photos photo-{n}.jpg` | 일괄 이름 변경 (`{n}` `{name}` `{ext}` 사용) |
| `template python foo in ./dir` | 템플릿에서 새 파일 (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | 타입별 필터링된 클립보드 기록 (text/url/email/path/code/ip/image) |
| `workflow 검토` / `run 검토` | 명명된 다중 단계 워크플로 (`workflows` 목록) |
| `ql docs hello` / `ticket:ABC-123` | 매개변수화된 퀵링크 (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | 시스템 명령 |
| `wifi` / `wifi off` / `bluetooth toggle` | Wi-Fi + Bluetooth 상태 및 on/off (엔터 적용) |
| `volume 50` / `volume mute` / `brightness 70` | 볼륨 레벨/뮤트 + 화면 밝기 (엔터 적용) |
| `settings wifi` / `settings sound` / `settings` | OS 설정 페이지 열기 (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | 최초 실행 상태 + wilfred.yml 편집/검증 (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | 부팅 시 체크의 새 버전 상태 (엔터로 링크 복사, `wilfred update`로 설치) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | 설정 제자리 업데이트/읽기, 검증됨 (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | 플러그인 목록 + 신뢰 승인 (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout 작업` | 열려 있는 창 타일링 또는 저장된 레이아웃 적용 (`wilfred tile`, 모니터 변경 시 `layouts.auto_apply`) |
| `winget firefox` / `brew switch` / `apt vlc` | 패키지 검색 -- 엔터로 설치, `Tab`로 명령 복사 (`packages`로 관리자 나열) |
| `clip` / `clips` | 클립보드 및 최근 클립 |
| `!yt 고양이` / `gh wilfred` | 검색 매크로 (`macros` 나열) |

## 아키텍처

플랫폼 I/O는 `wilfred/platform/native.hpp` 뒤에 있습니다. 핵심 모듈:

* `config` -- YAML 파싱 + 스키마 검증
* `index` -- 문자열 인터닝 풀, 압축 레코드, 역/트라이그램 인덱스, WAL
* `fs` -- 워커, 분류, 볼륨, 감시자
* `search` -- 퍼지, 랭킹 가중치, 필터, 쿼리 캐시
* `query` -- 분류 + 인터프리터
* `math` -- 재귀 하강 계산기
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `plugin` -- 네이티브 `.dll`/`.so`/`.dylib` ABI 및 stdio 플러그인, 신뢰 저장소, 레지스트리 인덱스
* `sync` -- 로컬 백업 아카이브 및 선택적 원격 push/pull
* `import` -- 크로스 OS 런처 설정 임포터 (핫키, 검색, 스니펫)
* `providers` -- 추가 검색 백엔드 레지스트리 (시맨틱, 브라우저 라이브러리, 캘린더/연락처/노트, 리모트, 패키지)

## 테스트 및 벤치마크

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # 또는 build/Release/wilfred_tests.exe
./build/wilfred_bench
```

테스트는 인덱스 CRUD, 지속성/복구, 퍼지/랭크/필터, 수학, 쿼리 분류, 설정 검증, 기록, 경로, IPC, 애플리케이션 검색을 다룹니다.

## 성능

Wilfred는 벤더링된 서드파티 라이브러리 없는 단일 C++20 바이너리입니다 -- 모든 것 (JSON, YAML, mmap 래퍼, CRC32)이 트리 내에 구현되어 추가 설치 불필요 (Linux 오버레이는 시스템 X11/WebKit 라이브러리 사용 시 존재, Wayland 네이티브 앵커링을 위한 gtk-layer-shell, X11 캔버스 폴백). 속도는 인덱스 설계에서 나옵니다 (`docs/architecture.md` 참조):

* 문자열 인턴 풀을 가진 컴팩트한 메모리 맵 레코드 저장소 (RAM에 중복 경로/이름 문자열 없음)
* 쿼리가 전체 스캔 대신 포스팅 리스트를 치게 하는 트라이그램 + 토큰 인덱스
* 주기적 스냅샷 및 크래시 복구로 Write-Ahead Log -- 쓰기는 추가만, 읽기는 지속성에서 절대 블록되지 않음
* 리스캔 대신 증분, 디바운스된 파일시스템 워처 업데이트
* 플러그인에서 쿼리별 타임아웃과 함께 바운디드 결과 세트 (상위 40개)

`wilfred_bench` (`benches/bench_main.cpp`)는 합성 1K-100K 파일명 데이터셋 (10회 타이밍 반복 + 1 워밍업, 100K 삽입 시 5회)에서 핫 패스를 측정하고 Min / Median / Mean / P95 / Max 밀리초 보고:

| 벤치마크 | 측정 내용 |
|---|---|
| String pool: intern 100K | 인터닝 처리량 (인덱스 메모리 효율성) |
| Tokenizer: tokenize 100K | 파일명 토큰화 |
| Index insert: 1K / 10K / 100K | 대량 인덱스 구축 |
| Index query: 10K / 100K records | 포스팅 리스트 조회 |
| Fuzzy search: 10K targets | Damerau-aware 스코어링 처리량 |
| Ranking: 10K scored hits | 가중치 스코어링 + 상위 40 선택 |
| Filter chain: 10K records | 확장자/종류 필터 평가 |
| Math evaluation: 1K expressions | 계산기 파싱 + 평가 |
| Snapshot save/load: 10K | 지속성 라운드트립 |
| Memory reporting: 100K index | 인덱스 풋프린트 회계 |

자신의 하드웨어에서 실행 후 수치 인용 -- 결과는 CPU, 디스크, 데이터셋 모양에 따라 다름:

```bash
cmake --build build --config Release --target wilfred_bench
./build/wilfred_bench          # 또는 build/Release/wilfred_bench.exe
```

## 패키징

`v*.*.*` 태그 푸시 시 `.github/workflows/release.yml` 실행, Release 바이너리 빌드 및 플랫폼별 아카이브를 GitHub Release에 첨부 (자세한 내용은 `docs/building.md#packaging-what-ci-does` 참조):

| 작업 | 아티팩트 |
|---|---|
| `windows-x64` | 포터블 `.zip`, 서명된 `.msi` (WiX v3, 사용자 단위, UAC 없음), Chocolatey `.nupkg` (임베디드 포터블, 설치 시 다운로드 없음) |
| `linux-x64` | 포터블 `.tar.gz` |
| `freebsd-x64` | 포터블 `.tar.gz` (FreeBSD 릴리스 작업; 다른 BSD는 소스에서 빌드 -- `docs/bsd.md` 참조) |
| `macos-arm64` | 포터블 `.tar.gz` |
| `android` | `.apk` + `.aab` -- 키스토어 비밀이 있으면 서명, 아니면 `-unsigned` 접미사 (자세한 내용은 `docs/android.md`) |
| `ios` | 미서명 시뮬레이터 `.zip` (자세한 내용은 `docs/ios.md`) |

데스크톱 아카이브는 `ui/overlay/`, `README.md`, `config/wilfred.default.yml`로 바이너리를 스테이징합니다. Windows 바이너리는 항상 VERSIONINFO를 통해 `Wilfred Open Contributors` 퍼블리셔 스탬프를 가짐; Authenticode 신뢰는 구성 시 PFX 또는 Azure Trusted Signing 비밀에서 나옴 (자세한 내용은 `docs/signing.md`, `docs/installer.md` 참조).

## 프로덕션 배포

* **데몬 실행, 원샷 아님.** `wilfred daemon` (단순 `wilfred`)은 오래 살아있는 프로세스: 인덱서 + 감시자 + 핫키 + 오버레이 + IPC 서버. 로그인 항목 / 자동 시작 항목으로 실행 (`wilfred autostart enable`로 이 바이너리를 Windows/macOS/Linux/BSD에 등록); 트레이 아이콘 (Windows)으로 종료. CLI 서브커맨드 (`search`, `launch`, `status`, ...)는 사용 가능한 경우 로컬 IPC 소켓을 통해 실행 중인 데몬과 통신 (자세한 내용은 `docs/cli.md`, `docs/ipc-and-api.md` 참조).
* **설정은 코드.** 첫 실행 시 검증된 `wilfred.yml` 작성 (위 위치 표 참조); 잘못된 키는 사람이 읽을 수 있는 에러와 `Did you mean` 힌트로 실패, 절대 사일런트 기본값 아님. 플릿의 경우 기준 `wilfred.yml` 배포 (루트, 제외, 가중치, 별칭 -- 전체 스키마는 `docs/configuration.md` 및 `config/wilfred.default.yml`) 후 사용자가 로컬에서 오버라이드 허용.
* **데이터 및 DR.** 인덱스, 기록, 클립, 노트, 로그는 OS별 데이터 디렉터리에 저장 (위 표 참조). `backup` / `restore`로 머신 상태 이동; `sync-push` / `sync-pull`로 복제 (자세한 내용은 `docs/sync-and-backup.md` 참조). 필요하기 전에 복원 테스트.
* **관찰.** `wilfred status`로 인덱스 건강 보고; `logging.*` 키로 로그 상세도/보존 제어; 선택적 로컬 HTTP API (`api.*`, 인증됨) 가 `/search`, `/status`, `/index`, `/launch`, 백업/동기화 엔드포인트를 대시보드와 자동화에 노출.
* **개인정보 보호 태도.** 로컬 우선: 계정 없음, 텔레메트리 없음, 네트워크 호출 없음 (날씨/AI/동기화 활성화하지 않는 한). 정책 요구 시 `history.*`로 기록 비활성화 또는 제한; AI 제공자는 사용자 자체 API 키 필요 (`ai.*`, 기본 꺼짐).
* **모바일은 실험적 (v23.0.5 기준).** 안드로이드 (`.apk`) 및 iOS (시뮬레이터 `.app`) 빌드는 설치/실행되나, 추가 테스트와 개발 대기 중으로 일상 드라이버 준비 안 됨 -- 데스크톱 우선 배포 (`docs/android.md`, `docs/ios.md` 참조).
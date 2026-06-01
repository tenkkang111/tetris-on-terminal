# TETRIS Terminal Edition

```
 __               __
/\ \__           /\ \__           __
\ \ ,_\     __   \ \ ,_\   _ __  /\_\     ____
 \ \ \/   /'__`\  \ \ \/  /\`'__\\/\ \   /',__\
  \ \ \_ /\  __/   \ \ \_ \ \ \/  \ \ \ /\__, `\
   \ \__\\ \____\   \ \__\ \ \_\   \ \_\\/\____/
    \/__/ \/____/    \/__/  \/_/    \/_/ \/___/

              TERMINAL EDITION
```

**TETR.IO** 룰을 따르는 터미널 기반 테트리스 게임입니다.
싱글플레이 및 LAN 1v1 멀티플레이를 지원합니다.

## 주요 기능

| 기능 | 설명 |
|------|------|
| **모던 테트리스** | SRS 회전, 7-bag 랜덤, 고스트 피스, 홀드 |
| **증강 시스템** | 레벨업 시 카드를 선택하여 능력 강화 |
| **LAN 멀티플레이** | 같은 네트워크 내 자동 검색, 방 이름/비밀번호 지원 |
| **터미널 UI** | ncurses 기반 컬러 인터페이스 |

## 빠른 시작

```bash
# 의존성 설치 (Debian/Ubuntu)
sudo apt install build-essential libncurses-dev

# 빌드 및 실행
make && ./tetris
```

## 요구 사항

- Linux 또는 WSL
- GCC (C11 이상)
- ncurses 라이브러리

<details>
<summary>다른 배포판 설치 명령</summary>

**Fedora:**
```bash
sudo dnf install gcc make ncurses-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel ncurses
```
</details>

## 명령어

```
Usage: ./tetris [command] [options]

Commands:
  (none)                        메인 메뉴로 시작
  host [port] [name] [pass]     게임 호스트
  join <ip> [port] [pass]       IP로 게임 참가
  search                        LAN 게임 검색 후 참가
  help                          도움말 표시
```

### 예시

```bash
# 메인 메뉴
./tetris

# 호스트
./tetris host                       # 기본 포트(5555)로 호스트
./tetris host 6000                  # 포트 6000으로 호스트
./tetris host 5555 "My Room"        # 방 이름 설정
./tetris host 5555 "My Room" 1234   # 비밀번호 설정

# 참가
./tetris join 192.168.1.10          # IP로 참가
./tetris join 192.168.1.10 6000     # 포트 지정
./tetris join 192.168.1.10 5555 pw  # 비밀번호 입력

# LAN 검색
./tetris search                     # 자동 검색 후 선택
```

## 조작법

| 동작 | 키 |
|:----:|:----:|
| 이동 | `←` `→` |
| 소프트 드롭 | `↓` |
| 하드 드롭 | `Space` |
| 회전 (CW) | `↑` / `X` |
| 회전 (CCW) | `Z` |
| 180° 회전 | `A` |
| 홀드 | `C` |
| 종료 | `Q` |

**카드 선택** (레벨업 시): `1` `2` `3` 키 또는 마우스 클릭

## 게임 화면

### 메인 메뉴

```
┌──────────────────────────────┐
│           > SOLO             │
│         Play alone           │
├──────────────────────────────┤
│           HOST GAME          │
│    Host a room for LAN       │
├──────────────────────────────┤
│           FIND GAME          │
│   Search for games on LAN    │
└──────────────────────────────┘
    UP/DOWN: Navigate
    ENTER: Select   Q: Quit
```

### 게임 플레이

- **보드**: 10×20 플레이필드
- **NEXT**: 다음 피스 대기열 (기본 5개)
- **HOLD**: 홀드된 피스
- **상태**: 점수, 라인, 레벨, 콤보
- **XP바**: 레벨업 진행도
- **상대 보드**: 멀티플레이 시 표시

## 멀티플레이

### 게임 호스트하기

**메뉴에서:**
1. **HOST GAME** 선택
2. 방 설정 입력 (Room Name, Port, Password)
3. 상대방 접속 대기

**커맨드라인:**
```bash
./tetris host [port] [name] [password]
```

### 게임 찾기

**메뉴에서:**
1. **FIND GAME** 선택
2. LAN에서 자동으로 호스트 검색 (3초)
3. 목록에서 방 선택 (`[P]` = 비밀번호 필요)
4. 비밀번호 방이면 비밀번호 입력

**커맨드라인:**
```bash
./tetris search                     # LAN 검색
./tetris join <ip> [port] [pass]    # 직접 연결
```

### 화면 크기 동기화

멀티플레이 중 한쪽 화면이 너무 작으면:
- 양쪽 모두 자동 일시정지
- 60초 내 해결하지 않으면 해당 플레이어 패배

## 증강 시스템

라인 클리어로 XP를 획득하고, 레벨업 시 3장의 카드 중 1장을 선택합니다.

### 등급

| 등급 | 확률 |
|:----:|:----:|
| BRONZE | 60% |
| SILVER | 25% |
| GOLD | 12% |
| PRISM | 3% |

### 카드 목록

| 카드 | 효과 | B | S | G | P |
|------|------|:-:|:-:|:-:|:-:|
| **Score Boost** | 점수 +X% | 10 | 25 | 50 | 100 |
| **Tetris Bonus** | 테트리스 공격 +X줄 | 1 | 2 | 3 | 5 |
| **T-Spin Master** | T-스핀 공격 +X줄 | 1 | 2 | 3 | 5 |
| **Combo King** | 콤보(≥3) 단계당 +X | 1 | 2 | 3 | 4 |
| **Aegis** | 가비지 X회 차단 | 1 | 3 | 6 | 12 |
| **Iron Grip** | 락 리셋 +X회 | 5 | 10 | 20 | 40 |
| **Reflexes** | 락 딜레이 +Xms | 200 | 500 | 1000 | 2000 |
| **Bag Vision** | NEXT +X개 | 1 | 2 | 3 | 5 |
| **Scholar** | XP +X% | 15 | 30 | 50 | 100 |
| **Lucky Star** | 락당 +X점 | 100 | 300 | 700 | 1500 |

## 공격 계산

### 기본 공격력

| 클리어 | 줄 |
|--------|:--:|
| Single | 0 |
| Double | 1 |
| Triple | 2 |
| Tetris | 4 |
| T-Spin Single | 2 |
| T-Spin Double | 4 |
| T-Spin Triple | 6 |

### 보너스

- **Back-to-Back**: +1줄
- **Combo**: TETR.IO 테이블 적용
- **Perfect Clear**: +10줄

## 게임 규칙

| 규칙 | 설명 |
|------|------|
| **SRS** | Super Rotation System (180° 킥 포함) |
| **7-Bag** | 7개 피스가 한 세트, 각각 1회씩 등장 |
| **Hold** | 피스당 1회 홀드 가능 |
| **Ghost** | 착지 위치 미리보기 |
| **Lock Delay** | 500ms, 최대 15회 리셋 |
| **T-Spin** | 3-corner 규칙으로 판정 |
| **Garbage** | 공격으로 수신 가비지 상쇄 |

## 제한 사항

- 터미널 최소 크기: 80×24 (멀티플레이 시 더 넓은 화면 필요)
- 사운드 미지원

## 정리

```bash
make clean          # 빌드 파일 삭제
```

## 라이선스

MIT License

---

*ncurses와 커피로 만들어졌습니다.*

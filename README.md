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

**TETR.IO** 룰을 따르는 터미널 기반 테트리스 게임입니다. 싱글플레이 및 1v1 멀티플레이를 지원합니다.

---

## 주요 기능

- **모던 테트리스** - SRS 회전, 7-bag 랜덤, 고스트 피스, 홀드
- **증강 시스템** - 레벨업 시 카드를 선택하여 능력 강화
- **로컬 멀티플레이** - TCP/IP로 호스트 또는 참가
- **터미널 UI** - ncurses 기반 컬러 인터페이스
- **크로스 플랫폼** - Linux 및 WSL 지원

---

## 요구 사항

- Linux 또는 WSL
- GCC (C11 이상)
- Make
- ncurses 라이브러리

### 의존성 설치

**Debian/Ubuntu:**
```bash
sudo apt update
sudo apt install build-essential libncurses-dev
```

**Fedora:**
```bash
sudo dnf install gcc make ncurses-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel ncurses
```

---

## 설치

### 로컬 빌드

```bash
git clone https://github.com/your-username/tetris-on-terminal.git
cd tetris-on-terminal
make
./tetris
```

### 글로벌 설치

```bash
make
sudo cp tetris /usr/local/bin/
```

이제 어디서든 `tetris` 명령으로 실행할 수 있습니다.

### 제거

```bash
sudo rm /usr/local/bin/tetris
```

---

## 게임 씬

게임은 4개의 씬으로 구성됩니다:

### 로비 (Lobby)
```
┌─────────────────────────────────┐
│         TETRIS v1.0.0           │
├─────────────────────────────────┤
│  > SOLO                         │
│    HOST GAME                    │
│    JOIN GAME                    │
│    QUIT                         │
└─────────────────────────────────┘
```
메인 메뉴입니다. 방향키 또는 W/S로 이동, Enter로 선택합니다.

### 매칭 (Matching)
멀티플레이 연결을 처리하는 화면입니다:
- **호스트**: 포트를 설정하고 상대방 접속을 대기
- **참가**: 호스트의 IP 주소와 포트를 입력하여 연결

### 게임 (Game)
메인 게임 플레이 화면입니다:
- 내 보드 (10×20 플레이필드)
- NEXT 피스 대기열 (기본 5개, 증강으로 추가 가능)
- HOLD 피스 표시
- 점수, 라인, 레벨, 콤보 카운터
- XP 바 및 증강 인벤토리
- 상대방 보드 (멀티플레이 시)

### 결과 (Result)
게임 종료 후 최종 통계를 표시합니다:
- **솔로**: 점수, 클리어 라인, 도달 레벨
- **멀티플레이**: 상대방과 나란히 비교, 승패 표시

---

## 조작법

| 동작 | 키 |
|------|-----|
| 왼쪽 이동 | `←` |
| 오른쪽 이동 | `→` |
| 소프트 드롭 | `↓` |
| 하드 드롭 | `Space` |
| 시계방향 회전 | `↑` 또는 `X` |
| 반시계방향 회전 | `Z` |
| 180° 회전 | `A` |
| 홀드 | `C` |
| 종료 | `Q` |

### 카드 선택
레벨업 시:
- `1`, `2`, `3` 키로 카드 선택
- 또는 마우스 클릭

---

## 증강 시스템

라인 클리어로 XP를 획득하고, 레벨업 시 카드를 선택하여 능력을 강화합니다.

### 진행 방식
- **최대 레벨**: 5
- **카드 선택**: 레벨업마다 3장 중 1장 선택 (최대 4장 보유)
- **필요 XP**: `10 + 5 × 레벨`

### 등급별 확률
| 등급 | 색상 | 확률 |
|------|------|------|
| BRONZE | 갈색 | 60% |
| SILVER | 흰색 | 25% |
| GOLD | 노란색 | 12% |
| PRISM | 무지개 | 3% |

### 증강 목록

| 증강 | 효과 | Bronze | Silver | Gold | Prism |
|------|------|--------|--------|------|-------|
| **Score Boost** | 점수 +X% | +10% | +25% | +50% | +100% |
| **Tetris Bonus** | 테트리스 공격 +X줄 | +1 | +2 | +3 | +5 |
| **T-Spin Master** | T-스핀 공격 +X줄 | +1 | +2 | +3 | +5 |
| **Combo King** | 콤보(≥3) 단계당 +X | +1 | +2 | +3 | +4 |
| **Aegis** | 수신 가비지 X회 차단 | 1 | 3 | 6 | 12 |
| **Iron Grip** | 락 리셋 +X회 | +5 | +10 | +20 | +40 |
| **Reflexes** | 락 딜레이 +Xms | +200 | +500 | +1000 | +2000 |
| **Bag Vision** | NEXT 표시 +X개 | +1 | +2 | +3 | +5 |
| **Scholar** | XP 획득 +X% | +15% | +30% | +50% | +100% |
| **Lucky Star** | 락당 점수 +X | +100 | +300 | +700 | +1500 |

### 증강 상세 설명

- **Score Boost**: 모든 점수 획득량을 비율로 증가
- **Tetris Bonus**: 4줄 클리어 시 추가 공격 라인
- **T-Spin Master**: T-스핀 클리어 시 추가 공격 라인
- **Combo King**: 3연속 이상 콤보 시 단계당 추가 공격
- **Aegis**: 상대방 가비지를 흡수하는 보호막
- **Iron Grip**: 착지 후 이동/회전 가능 횟수 증가
- **Reflexes**: 착지 후 락까지의 시간 연장
- **Bag Vision**: NEXT 대기열에 더 많은 피스 표시
- **Scholar**: XP 획득량 증가로 빠른 레벨업
- **Lucky Star**: 피스가 락될 때마다 보너스 점수

---

## 공격 라인 산정

| 클리어 종류 | 라인 |
|-------------|------|
| Single | 0 |
| Double | 1 |
| Triple | 2 |
| Tetris | 4 |
| T-Spin Mini Single | 0 |
| T-Spin Mini Double | 1 |
| T-Spin Single | 2 |
| T-Spin Double | 4 |
| T-Spin Triple | 6 |

**추가 보너스:**
- Back-to-Back: +1줄
- Combo: TETR.IO 테이블에 따른 가산
- Perfect Clear: +10줄

---

## 멀티플레이

### 게임 호스트

```bash
./tetris
# "HOST GAME" 선택
# 포트 입력 (기본: 5555)
# 상대방 대기...
```

또는 커맨드라인:
```bash
./tetris host 5555
```

### 게임 참가

```bash
./tetris
# "JOIN GAME" 선택
# 호스트 IP와 포트 입력
```

또는 커맨드라인:
```bash
./tetris join 192.168.1.100 5555
```

### 로컬 테스트
터미널 2개에서 각각 실행:
```bash
# 터미널 1
./tetris host

# 터미널 2
./tetris join 127.0.0.1
```

---

## 게임 규칙

**TETR.IO** 가이드라인 준수:

- **SRS**: Super Rotation System (180° 킥 포함)
- **7-Bag**: 7개 피스가 한 백에서 각각 1회씩 등장
- **Hold**: 현재 피스를 홀드와 교체 (피스당 1회)
- **Ghost Piece**: 피스가 착지할 위치 표시
- **Lock Delay**: 500ms, 최대 15회 리셋
- **T-Spin 판정**: 3-corner 규칙
- **Garbage**: 송신 공격으로 수신 가비지 상쇄, 잔량만 전송

---

## 제한 사항

- DAS/ARR 타이밍은 OS 키 리피트 설정에 제한됨
- 터미널 최소 크기: 80×24 (멀티플레이 시 더 넓은 화면 필요)
- 사운드 미지원

---

## 정리

```bash
make clean
```

---

## 라이선스

MIT License

---

ncurses와 커피로 만들어졌습니다.

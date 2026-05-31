# tetris-on-terminal

터미널 기반 테트리스. TETR.IO 룰 준거. 싱글 및 1v1 멀티플레이 지원.

## 요구 사항

- Linux 또는 WSL
- gcc (C11 이상)
- make
- libncurses-dev

## 빌드

```
make
```

## 실행

싱글:

```
./tetris
```

호스트:

```
./tetris host [port]
```

참가:

```
./tetris join <ip> [port]
```

포트 미지정 시 5555 사용.

## 조작

| 동작        | 키                          |
|-------------|-----------------------------|
| 좌/우 이동  | ← / →                       |
| Soft Drop   | ↓                           |
| Hard Drop   | Space                       |
| 회전 CW     | ↑ 또는 X                    |
| 회전 CCW    | Z                           |
| 회전 180°   | A                           |
| Hold        | C                           |
| 종료        | Q                           |

## 적용 룰

- SRS (Super Rotation System) 및 SRS+ 180° 회전
- 7-Bag 랜덤
- Hold (피스당 1회)
- Ghost Piece
- Lock Delay 500ms, 최대 15회 reset
- T-Spin / T-Spin Mini 판정 (3-corner rule)
- Back-to-Back, Combo
- Perfect Clear 보너스
- Garbage 큐 상쇄 (송신 공격으로 수신 garbage 차감 후 잔량 송신)

## 레벨 / 증강 시스템

- 라인 클리어 시 XP 획득. 최대 레벨 5.
- 레벨업 시 카드 3장이 제시되며 1장 선택 (마우스 클릭 또는 1 / 2 / 3 키).
- 카드 등급 가중치: BRONZE 60% / SILVER 25% / GOLD 12% / PRISM 3%.
- 총 최대 4장 장착 가능. 같은 효과는 누적 합산.

| ID | 효과 | B / S / G / P |
|----|------|---------------|
| Score Boost | 점수 +X% | 10 / 25 / 50 / 100 |
| Tetris Bonus | Tetris +X 공격 | 1 / 2 / 3 / 5 |
| T-Spin Master | T-spin 클리어 +X 공격 | 1 / 2 / 3 / 5 |
| Combo King | 콤보≥3 매 단계 +X 공격 | 1 / 2 / 3 / 4 |
| Aegis | 수신 가비지 X회 차단 | 1 / 3 / 6 / 12 |
| Iron Grip | Lock reset +X | 5 / 10 / 20 / 40 |
| Reflexes | Lock delay +X ms | 200 / 500 / 1000 / 2000 |
| Bag Vision | NEXT +X | 1 / 2 / 3 / 5 |
| Scholar | XP +X% | 15 / 30 / 50 / 100 |
| Lucky Star | 락당 +X 점수 | 100 / 300 / 700 / 1500 |

### 공격 라인 산정

| 종류             | 라인 |
|------------------|------|
| Single           | 0    |
| Double           | 1    |
| Triple           | 2    |
| Tetris           | 4    |
| T-Spin Mini Sgl  | 0    |
| T-Spin Mini Dbl  | 1    |
| T-Spin Single    | 2    |
| T-Spin Double    | 4    |
| T-Spin Triple    | 6    |

추가 보너스: B2B +1, Combo 가산(TETR.IO 테이블), Perfect Clear +10.

## 로컬 멀티플레이

터미널 두 개에서 각각 실행.

```
./tetris host
./tetris join 127.0.0.1
```

## 제한 사항

- DAS / ARR은 게임 내부 타이머로 구현되어 있으나, 터미널이 키 release 이벤트를 송신하지 않으므로 유효 DAS의 하한은 OS 키 리피트 첫-딜레이, 유효 ARR의 하한은 OS 키 리피트 레이트로 고정. 게임 내 설정값은 그 이상으로 늦추는 용도로만 동작.

## 정리

```
make clean
```

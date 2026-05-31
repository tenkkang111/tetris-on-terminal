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

# tetris-on-terminal

터미널 기반 테트리스. 싱글 및 1v1 멀티플레이 지원.

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

- 좌/우 방향키: 이동
- 아래 방향키: Soft Drop
- 위 방향키: 회전
- Space: Hard Drop
- C: Hold
- Q: 종료

## 로컬 멀티플레이

터미널 두 개에서 각각 실행.

```
./tetris host
./tetris join 127.0.0.1
```

## 정리

```
make clean
```

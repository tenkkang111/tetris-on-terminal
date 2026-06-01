#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>

/* 서버측 핸드셰이크 메시지 ID (게임 프로토콜과 충돌 없음) */
#define SRV_MSG_HELLO   0x10
#define SRV_MSG_MATCHED 0x18
#define SRV_MSG_FULL    0x19

/* HELLO 페이로드: [type=0x10][ver=1][roomName 32B] */
#define SRV_HELLO_SIZE  34
#define SRV_ROOM_NAME_LEN 32

/**
 * @brief Run the dedicated relay server. Blocks until process termination.
 * @return 0 on graceful exit, non-zero on error.
 */
int runServer(uint16_t port);

#endif

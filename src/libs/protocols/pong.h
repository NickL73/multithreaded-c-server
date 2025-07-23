//
// Created by nick on 7/22/25.
//

#ifndef PONG_H
#define PONG_H

#include <stdint.h>
#include <string.h>

#define PING_PONG_LEN 5 // strlen("ping") + 1

typedef enum ping_pong_type_t
{
    PING_TYPE     = 0,
    PONG_TYPE     = 1,
    PING_PONG_ERR = 2
} ping_pong_type_t;

typedef struct __attribute__((packed)) ping_pong_t
{
    uint8_t  type;
    uint16_t len;
    uint8_t  volley;
    char     buf[PING_PONG_LEN];
} ping_pong_t;

int proto_pingpong_create_response(unsigned char * p_in_buf, char * p_out_buf, uint16_t in_len, uint16_t * p_out_len);

#endif // PONG_H

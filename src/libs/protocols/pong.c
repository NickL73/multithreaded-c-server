//
// Created by nick on 7/22/25.
//

#include "pong.h"

#include "utils.h"

#include <netinet/in.h>

int proto_pingpong_create_response(unsigned char * p_in_buf, char * p_out_buf, uint16_t inbuf_len, uint16_t * p_out_len)
{
    int           res           = -1;
    ping_pong_t   pong_response = {0};
    ping_pong_t * p_in          = NULL;
    uint16_t      cli_id        = 0;

    if ((NULL == p_in_buf) || (NULL == p_out_buf) || (NULL == p_out_len))
    {
        LOG_ERROR("Invalid arguments");
        goto end;
    }

    if (sizeof(ping_pong_t) != inbuf_len)
    {
        LOG_WARN("Invalid ping pong message length. Sending error code back.");
        pong_response.type   = PING_PONG_ERR;
        pong_response.len    = htons(sizeof(ping_pong_t));
        pong_response.cli_id = htons(cli_id);
        pong_response.volley = 0;
        memcpy(p_out_buf, &pong_response, sizeof(ping_pong_t));
        *p_out_len = sizeof(ping_pong_t);

        res = 0;
        goto end;
    }

    p_in   = (ping_pong_t *)p_in_buf;
    cli_id = ntohs(p_in->cli_id);
    if ((PING_TYPE != p_in->type) || (strncmp(p_in->buf, "ping", strlen("ping")) != 0))
    {
        LOG_WARN("Invalid ping pong message. Sending error code back. Type: %d. Buff: %s", p_in->type, p_in->buf);
        pong_response.type   = PING_PONG_ERR;
        pong_response.len    = htons(sizeof(ping_pong_t));
        pong_response.cli_id = htons(cli_id);
        pong_response.volley = 0;
        memcpy(p_out_buf, &pong_response, sizeof(ping_pong_t));
        *p_out_len = sizeof(ping_pong_t);
    }

    else
    {
        LOG_INFO("Received ping pong message. Sending pong back.");
        pong_response.type   = PONG_TYPE;
        pong_response.len    = htons(sizeof(ping_pong_t));
        pong_response.cli_id = htons(cli_id);
        pong_response.volley = p_in->volley + 1;
        memcpy(pong_response.buf, "pong", sizeof(pong_response.buf));
        memcpy(p_out_buf, &pong_response, sizeof(ping_pong_t));
        *p_out_len = sizeof(ping_pong_t);
    }

    res = 0;

end:
    return res;
}

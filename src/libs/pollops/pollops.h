//
// Created by nick on 7/12/25.
//

#ifndef POLLOPS_H
#define POLLOPS_H

#include <poll.h>
#include <stddef.h>

typedef struct conn_context conn_context_t;

typedef struct
{
    struct pollfd    pfd;
    conn_context_t * p_context;
} poll_entry_t;

typedef struct
{
    poll_entry_t * p_entries;
    size_t         num_entries;
    size_t         capacity;
} pollops_table_t;

int pollops_init(pollops_table_t * p_table, size_t initial_capacity);
int pollops_deinit(pollops_table_t * p_table);

int pollops_add_entry(pollops_table_t * p_table, int fd, conn_context_t * p_ctx);
int pollops_mark_for_close(pollops_table_t * p_table, size_t idx);
int pollops_compact(pollops_table_t * p_table);

#endif // POLLOPS_H

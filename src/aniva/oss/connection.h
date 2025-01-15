#ifndef __ANIVA_OSS_CONNECTION_H__
#define __ANIVA_OSS_CONNECTION_H__

#include <lightos/types.h>

struct oss_object;

typedef struct oss_connection {
    struct oss_object* parent;
    struct oss_object* child;
} oss_connection_t;

static inline bool oss_connection_equals(oss_connection_t* target, oss_connection_t* b)
{
    return (target->parent == b->parent && target->child == b->child);
}

/*
 * If @obj is marked as the parent in this connection, it means that this connection
 * points to an object that is downstream, relative to @obj
 */
static inline bool oss_connection_is_downstream(oss_connection_t* conn, struct oss_object* obj)
{
    return (conn->parent == obj);
}

/*
 * If @obj is marked as the child in this connection, it means that this connection
 * points to an object that is upstream, relative to @obj
 */
static inline bool oss_connection_is_upstream(oss_connection_t* conn, struct oss_object* obj)
{
    return (conn->child == obj);
}

oss_connection_t* create_oss_connection(struct oss_object* parent, struct oss_object* child);
void destroy_oss_connection(oss_connection_t* conn);

int oss_connect(struct oss_object* parent, struct oss_object* child);
int oss_disconnect(oss_connection_t* conn);

void init_oss_connections();

#endif // !__ANIVA_OSS_CONNECTION_H__

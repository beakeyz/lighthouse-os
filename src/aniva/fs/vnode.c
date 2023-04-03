
#include "vnode.h"

/*
 * Also prepare the write queues
 */
void vn_freeze(vnode_t* node) {
  node->m_flags |= VN_FROZEN;
}

/*
 * If writes are queued, we need to flush them...
 */
void vn_unfreeze(vnode_t* node) {
  node->m_flags &= ~VN_FROZEN;
}

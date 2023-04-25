#include "buddy.h"
#define NULL ((void *)0)

struct Node {
  int rank;
  void *addr;
  struct Node *nxt;
};

struct Node *free_areas[MAXRANK + 1], *used_areas[MAXRANK + 1];
void *start_addr, *end_addr;

struct Node *new_node(int rank, void *addr, struct Node *nxt) {
  struct Node *p = (struct Node *)malloc(sizeof(struct Node));
  p->rank = rank;
  p->addr = addr;
  p->nxt = nxt;
  return p;
}

int init_page(void *p, int pgcount) {
  start_addr = p;
  end_addr = p + pgcount * PAGE_SIZE;
  int rank = 1;
  for (; pgcount > 1; pgcount >>= 1, ++rank);
  free_areas[rank] = new_node(rank, p, NULL);
  return OK;
}

void *alloc_pages(int rank) { 
  if (rank < 1 || rank > MAXRANK) return -EINVAL;
  int at;
  for (at = rank; at <= MAXRANK; ++at)
    if (free_areas[at] != NULL) break;
  if (at > MAXRANK) return -ENOSPC;
  struct Node *p = free_areas[at];
  while (at > rank) {
    free_areas[at--] = p->nxt;
    free_areas[at] = new_node(at, p->addr + (1 << at - 1) * PAGE_SIZE, free_areas[at]);
    free_areas[at] = new_node(at, p->addr, free_areas[at]);
    free(p), p = free_areas[at];
  }
  free_areas[at] = p->nxt;
  used_areas[at] = new_node(rank, p->addr, used_areas[at]);
  return p->addr;
}

struct Node *find_node(struct Node **lst, void *addr, int isDelete) {
  for (int i = 1; i <= MAXRANK; ++i)
    for (struct Node *pre = NULL, *p = lst[i]; p; pre = p, p = p->nxt)
      if (p->addr == addr) {
        if (isDelete) pre ? (pre->nxt = p->nxt) : (lst[i] = p->nxt);
        return p;
      }
  return NULL;
}

struct Node *try_combine(struct Node *p, struct Node *q) {
  if (p->rank != q->rank) return NULL;
  if (p->addr > q->addr) {
    struct Node *tmp = p;
    p = q, q = tmp;
  }
  if (p->addr + (1 << p->rank - 1) * PAGE_SIZE == q->addr &&
      !(p->addr - start_addr >> PAGE_SHIFT & (1 << p->rank) - 1))
    return new_node(p->rank + 1, p->addr, NULL);
  return NULL;
}

int return_pages(void *p) {
  struct Node *node = find_node(used_areas, p, 1);
  if (node == NULL) return -EINVAL;
  for (int at = node->rank; at <= MAXRANK; ++at) {
    struct Node *combine = NULL;
    for (struct Node *pre = NULL, *q = free_areas[at]; q; pre = q, q = q->nxt) {
      combine = try_combine(q, node);
      if (combine) {
        pre ? (pre->nxt = q->nxt) : (free_areas[at] = q->nxt);
        free(q);
        break;
      }
    }
    if (combine) {
      free(node), node = combine;
    } else {
      free_areas[at] = new_node(at, node->addr, free_areas[at]);
      free(node);
      break;
    }
  }
  return OK;
}

int query_ranks(void *p) {
  struct Node *node = find_node(used_areas, p, 0);
  if (node == NULL) {
    node = find_node(free_areas, p, 0);
    if (node == NULL) return -EINVAL;
  }
  return node->rank;
}

int query_page_counts(int rank) {
  if (rank < 1 || rank > MAXRANK) return -EINVAL;
  int cnt = 0;
  for (struct Node *p = free_areas[rank]; p; p = p->nxt) ++cnt;
  return cnt;
}

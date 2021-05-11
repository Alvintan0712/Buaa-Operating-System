#include <stdio.h>
#include "pageReplace.h"
#define MAX_PHY_PAGE 64
#define MAX_PAGE 12
#define get_Page(x) (x>>MAX_PAGE)

typedef struct Node {
    int idx;
    struct Node *nxt;
} node;
node LRU[MAX_PHY_PAGE];
node *head;
int freemem = 0;
long *pages;

inline node* alloc() {
    node *p = LRU + freemem;
    p->idx = freemem++;
    p->nxt = head;

    return p;
}

inline int find(int page) {
    node *p = head, *q = NULL;
    
    if (p == NULL) {
        head = alloc();
    } else if (p->nxt == NULL) {
        if (pages[p->idx] == page) return 1;
        head = alloc();
    } else {
        node *r = NULL;
        while (p) {
            if (pages[p->idx] == page) {
                if (!q) return 1;
                q->nxt = p->nxt;
                p->nxt = head;
                head = p;
                pages[head->idx] = page;
                return 1;
            }
            r = q;
            q = p;
            p = p->nxt;
        }
        if (freemem < MAX_PHY_PAGE) head = alloc();
        else {
            r->nxt = NULL;
            q->nxt = head;
            head = q;
        }
    }
    pages[head->idx] = page;
    return 0;
}

void pageReplace(long *physic_memory, long page) {
    pages = physic_memory;

    if (page == 0) return;
    int r = find(page >> MAX_PAGE);
    // printf("%d:", page >> MAX_PAGE);
    // puts(r ? "pop!" : "miss!");
}

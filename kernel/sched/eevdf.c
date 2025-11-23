#include <kernel/printk.h>
#include <kernel/sched/eevdf.h>
#include <string.h>

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

const uint32_t eevdf_nice_to_weight[40] = {
    88761, 71755, 56483, 46273, 36291, 29154, 23254, 18705, 14949, 11916,
    9548,  7620,  6100,  4904,  3906,  3121,  2501,  1991,  1586,  1277,
    1024,  820,   655,   526,   423,   335,   272,   215,   172,   137,
    110,   87,    70,    56,    45,    36,    29,    23,    18,    15,
};

const uint32_t eevdf_nice_to_wmult[40] = {
    48388,     59856,     76040,     92818,     118348,    147320,   184698,
    229616,    287308,    360437,    449829,    563644,    704093,   875809,
    1099582,   1376151,   1717300,   2157191,   2708050,   3363326,  4194304,
    5237765,   6557202,   8165337,   10153587,  12820798,  15790321, 19976592,
    24970740,  31350126,  39045157,  49367440,  61356676,  76695844, 95443717,
    119304647, 148102320, 186737708, 238609294, 286331153,
};

static eevdf_rq_t runqueue;
static eevdf_rb_node_t node_pool[64];
static uint64_t node_bitmap[2];

static eevdf_rb_node_t *alloc_node(void) {
    for (int i = 0; i < 64; i++) {
        int word = i / 64;
        int bit = i % 64;
        if (!(node_bitmap[word] & (1ULL << bit))) {
            node_bitmap[word] |= (1ULL << bit);
            return &node_pool[i];
        }
    }
    return NULL;
}

static void free_node(eevdf_rb_node_t *node) {
    if (!node)
        return;
    int idx = node - node_pool;
    if (idx < 0 || idx >= 64)
        return;
    int word = idx / 64;
    int bit = idx % 64;
    node_bitmap[word] &= ~(1ULL << bit);
}

static void rotate_left(eevdf_rb_node_t **root, eevdf_rb_node_t *x) {
    eevdf_rb_node_t *y = x->right;
    x->right = y->left;
    if (y->left)
        y->left->parent = x;
    y->parent = x->parent;
    if (!x->parent)
        *root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;
    y->left = x;
    x->parent = y;
}

static void rotate_right(eevdf_rb_node_t **root, eevdf_rb_node_t *y) {
    eevdf_rb_node_t *x = y->left;
    y->left = x->right;
    if (x->right)
        x->right->parent = y;
    x->parent = y->parent;
    if (!y->parent)
        *root = x;
    else if (y == y->parent->right)
        y->parent->right = x;
    else
        y->parent->left = x;
    x->right = y;
    y->parent = x;
}

static void insert_fixup(eevdf_rb_node_t **root, eevdf_rb_node_t *z) {
    while (z->parent && z->parent->color == 1) {
        if (z->parent == z->parent->parent->left) {
            eevdf_rb_node_t *y = z->parent->parent->right;
            if (y && y->color == 1) {
                z->parent->color = 0;
                y->color = 0;
                z->parent->parent->color = 1;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    rotate_left(root, z);
                }
                z->parent->color = 0;
                z->parent->parent->color = 1;
                rotate_right(root, z->parent->parent);
            }
        } else {
            eevdf_rb_node_t *y = z->parent->parent->left;
            if (y && y->color == 1) {
                z->parent->color = 0;
                y->color = 0;
                z->parent->parent->color = 1;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rotate_right(root, z);
                }
                z->parent->color = 0;
                z->parent->parent->color = 1;
                rotate_left(root, z->parent->parent);
            }
        }
    }
    (*root)->color = 0;
}

static eevdf_rb_node_t *rb_first(eevdf_rb_node_t *root) {
    if (!root)
        return NULL;
    while (root->left)
        root = root->left;
    return root;
}

static void delete_fixup(eevdf_rb_node_t **root, eevdf_rb_node_t *x,
                         eevdf_rb_node_t *parent) {
    while (x != *root && (!x || x->color == 0)) {
        if (x == parent->left) {
            eevdf_rb_node_t *w = parent->right;
            if (w && w->color == 1) {
                w->color = 0;
                parent->color = 1;
                rotate_left(root, parent);
                w = parent->right;
            }
            if (w && (!w->left || w->left->color == 0) &&
                (!w->right || w->right->color == 0)) {
                if (w)
                    w->color = 1;
                x = parent;
                parent = x->parent;
            } else {
                if (w && (!w->right || w->right->color == 0)) {
                    if (w->left)
                        w->left->color = 0;
                    w->color = 1;
                    rotate_right(root, w);
                    w = parent->right;
                }
                if (w) {
                    w->color = parent->color;
                    parent->color = 0;
                    if (w->right)
                        w->right->color = 0;
                }
                rotate_left(root, parent);
                x = *root;
            }
        } else {
            eevdf_rb_node_t *w = parent->left;
            if (w && w->color == 1) {
                w->color = 0;
                parent->color = 1;
                rotate_right(root, parent);
                w = parent->left;
            }
            if (w && (!w->right || w->right->color == 0) &&
                (!w->left || w->left->color == 0)) {
                if (w)
                    w->color = 1;
                x = parent;
                parent = x->parent;
            } else {
                if (w && (!w->left || w->left->color == 0)) {
                    if (w->right)
                        w->right->color = 0;
                    w->color = 1;
                    rotate_left(root, w);
                    w = parent->left;
                }
                if (w) {
                    w->color = parent->color;
                    parent->color = 0;
                    if (w->left)
                        w->left->color = 0;
                }
                rotate_right(root, parent);
                x = *root;
            }
        }
    }
    if (x)
        x->color = 0;
}

void eevdf_init(void) {
    memset(&runqueue, 0, sizeof(eevdf_rq_t));
    memset(node_pool, 0, sizeof(node_pool));
    memset(node_bitmap, 0, sizeof(node_bitmap));
}

void eevdf_enqueue(task_t *task) {
    if (!task || task->state != TASK_READY)
        return;

    if (task->vruntime < runqueue.min_vruntime) {
        task->vruntime = runqueue.min_vruntime;
    }

    eevdf_rb_node_t *node = alloc_node();
    if (!node) {
        printk("[EEVDF] FAILED to alloc node for PID %d\n", task->pid);
        return;
    }

    node->task = task;
    node->left = node->right = node->parent = NULL;
    node->color = 1;

    eevdf_rb_node_t *parent = NULL;
    eevdf_rb_node_t **link = &runqueue.root;
    int leftmost = 1;

    while (*link) {
        parent = *link;
        if (task->vruntime < parent->task->vruntime) {
            link = &parent->left;
        } else {
            link = &parent->right;
            leftmost = 0;
        }
    }

    if (leftmost)
        runqueue.leftmost = node;

    node->parent = parent;
    *link = node;

    insert_fixup(&runqueue.root, node);

    uint32_t weight = eevdf_nice_to_weight[task->priority + 20];
    runqueue.load_weight += weight;
    runqueue.nr_running++;
}

void eevdf_dequeue(task_t *task) {
    if (!task)
        return;

    // Search for the task's node in the runqueue
    eevdf_rb_node_t *node = NULL;
    for (int i = 0; i < 64; i++) {
        if ((node_bitmap[i / 64] & (1ULL << (i % 64))) &&
            node_pool[i].task == task) {
            node = &node_pool[i];
            break;
        }
    }

    // Safe for absent tasks: if task is not in queue, return without error
    // This can happen when dequeue is called on idle task or already-dequeued task
    if (!node)
        return;

    if (runqueue.leftmost == node) {
        if (node->right) {
            runqueue.leftmost = rb_first(node->right);
        } else {
            eevdf_rb_node_t *current = node;
            eevdf_rb_node_t *parent = current->parent;
            while (parent && current == parent->right) {
                current = parent;
                parent = parent->parent;
            }
            runqueue.leftmost = parent;
        }
    }

    eevdf_rb_node_t *y = node;
    eevdf_rb_node_t *x, *x_parent;
    uint8_t y_color = y->color;

    if (!node->left) {
        x = node->right;
        x_parent = node->parent;
        if (!node->parent)
            runqueue.root = node->right;
        else if (node == node->parent->left)
            node->parent->left = node->right;
        else
            node->parent->right = node->right;
        if (node->right)
            node->right->parent = node->parent;
    } else if (!node->right) {
        x = node->left;
        x_parent = node->parent;
        if (!node->parent)
            runqueue.root = node->left;
        else if (node == node->parent->left)
            node->parent->left = node->left;
        else
            node->parent->right = node->left;
        node->left->parent = node->parent;
    } else {
        y = node->right;
        while (y->left)
            y = y->left;
        y_color = y->color;
        x = y->right;
        x_parent = y->parent;

        if (y->parent == node) {
            x_parent = y;
        } else {
            if (y->right)
                y->right->parent = y->parent;
            y->parent->left = y->right;
            y->right = node->right;
            y->right->parent = y;
            x_parent = y->parent;
        }

        if (!node->parent)
            runqueue.root = y;
        else if (node == node->parent->left)
            node->parent->left = y;
        else
            node->parent->right = y;

        y->parent = node->parent;
        y->color = node->color;
        y->left = node->left;
        y->left->parent = y;
    }

    if (y_color == 0) {
        delete_fixup(&runqueue.root, x, x_parent);
    }

    uint32_t weight = eevdf_nice_to_weight[task->priority + 20];
    if (runqueue.load_weight >= weight)
        runqueue.load_weight -= weight;
    else
        runqueue.load_weight = 0;

    if (runqueue.nr_running > 0)
        runqueue.nr_running--;

    free_node(node);
}

task_t *eevdf_pick_next(void) {
    if (!runqueue.leftmost)
        return NULL;
    return runqueue.leftmost->task;
}

void eevdf_update_curr(task_t *task, uint64_t now) {
    if (!task)
        return;

    uint64_t delta = now - task->context.x23;
    if (delta == 0)
        return;

    task->context.x23 = now;

    // uint32_t weight = eevdf_nice_to_weight[task->priority + 20];
    uint64_t delta_fair =
        (delta * EEVDF_NICE_0_LOAD) /
        (runqueue.load_weight ? runqueue.load_weight : EEVDF_NICE_0_LOAD);
    task->vruntime += delta_fair;

    if (runqueue.leftmost) {
        runqueue.min_vruntime = runqueue.leftmost->task->vruntime;
    } else {
        runqueue.min_vruntime = task->vruntime;
    }
}

uint64_t eevdf_calc_slice(task_t *task) {
    if (runqueue.nr_running == 0)
        return EEVDF_TIME_SLICE_NS;

    uint32_t weight = eevdf_nice_to_weight[task->priority + 20];
    uint64_t slice =
        (EEVDF_TARGET_LATENCY * weight) /
        (runqueue.load_weight ? runqueue.load_weight : EEVDF_NICE_0_LOAD);

    if (slice < EEVDF_MIN_GRANULARITY)
        slice = EEVDF_MIN_GRANULARITY;
    if (slice > EEVDF_MAX_TIME_SLICE_NS)
        slice = EEVDF_MAX_TIME_SLICE_NS;

    return slice;
}

void eevdf_set_nice(task_t *task, int nice) {
    if (!task)
        return;
    if (nice < EEVDF_MIN_NICE)
        nice = EEVDF_MIN_NICE;
    if (nice > EEVDF_MAX_NICE)
        nice = EEVDF_MAX_NICE;
    task->priority = nice;
}

int eevdf_is_queued(task_t *task) {
    if (!task)
        return 0;
    
    for (int i = 0; i < 64; i++) {
        if ((node_bitmap[i / 64] & (1ULL << (i % 64))) &&
            node_pool[i].task == task) {
            return 1;
        }
    }
    return 0;
}

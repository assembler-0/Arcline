#ifndef ARCLINE_EEVDF_H
#define ARCLINE_EEVDF_H

#include <kernel/sched/task.h>
#include <stdint.h>

#define EEVDF_MIN_GRANULARITY 750000
#define EEVDF_TARGET_LATENCY 6000000
#define EEVDF_WAKEUP_GRANULARITY 1000000
#define EEVDF_NICE_0_LOAD 1024
#define EEVDF_MIN_NICE (-20)
#define EEVDF_MAX_NICE 19
#define EEVDF_TIME_SLICE_NS (4 * 1000000)
#define EEVDF_MAX_TIME_SLICE_NS (100 * 1000000)

extern const uint32_t eevdf_nice_to_weight[40];
extern const uint32_t eevdf_nice_to_wmult[40];

typedef struct eevdf_rb_node {
    struct eevdf_rb_node *left;
    struct eevdf_rb_node *right;
    struct eevdf_rb_node *parent;
    uint8_t color;
    task_t *task;
} eevdf_rb_node_t;

typedef struct {
    eevdf_rb_node_t *root;
    eevdf_rb_node_t *leftmost;
    uint64_t min_vruntime;
    uint32_t load_weight;
    uint32_t nr_running;
} eevdf_rq_t;

void eevdf_init(void);
void eevdf_enqueue(task_t *task);
void eevdf_dequeue(task_t *task);
task_t *eevdf_pick_next(void);
void eevdf_update_curr(task_t *task, uint64_t now);
uint64_t eevdf_calc_slice(task_t *task);
void eevdf_set_nice(task_t *task, int nice);
int eevdf_is_queued(task_t *task);

#endif

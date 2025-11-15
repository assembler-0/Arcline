// Minimal VMM with RB-tree VMA manager; page tables/MMU not enabled yet.

#include <mm/vmm.h>
#include <kernel/printk.h>
#include <stdint.h>

// 4 KiB page alignment for now
#define VMM_PAGE_SIZE 4096ULL
#define ALIGN_DOWN(x, a) ((uint64_t)(x) & ~((uint64_t)(a) - 1))
#define ALIGN_UP(x, a)   (((uint64_t)(x) + ((uint64_t)(a) - 1)) & ~((uint64_t)(a) - 1))

typedef enum { RB_RED = 0, RB_BLACK = 1 } rb_color_t;

typedef struct vma_node {
    uint64_t va;
    uint64_t pa;
    uint64_t size;    // bytes
    uint32_t attrs;
    // RB-tree links
    struct vma_node *left;
    struct vma_node *right;
    struct vma_node *parent;
    rb_color_t color;
} vma_node_t;

static vma_node_t *vma_root = NULL;

static inline int is_red(vma_node_t *n){ return n && n->color==RB_RED; }
static inline int is_black(vma_node_t *n){ return !n || n->color==RB_BLACK; }

static void rotate_left(vma_node_t **root, vma_node_t *x) {
    vma_node_t *y = x->right;
    x->right = y->left;
    if (y->left) y->left->parent = x;
    y->parent = x->parent;
    if (!x->parent) *root = y;
    else if (x == x->parent->left) x->parent->left = y;
    else x->parent->right = y;
    y->left = x;
    x->parent = y;
}

static void rotate_right(vma_node_t **root, vma_node_t *y) {
    vma_node_t *x = y->left;
    y->left = x->right;
    if (x->right) x->right->parent = y;
    x->parent = y->parent;
    if (!y->parent) *root = x;
    else if (y == y->parent->left) y->parent->left = x;
    else y->parent->right = x;
    x->right = y;
    y->parent = x;
}

static void insert_fixup(vma_node_t **root, vma_node_t *z) {
    while (is_red(z->parent)) {
        vma_node_t *p = z->parent;
        vma_node_t *g = p->parent;
        if (p == g->left) {
            vma_node_t *u = g->right;
            if (is_red(u)) { // case 1
                p->color = RB_BLACK; u->color = RB_BLACK; g->color = RB_RED; z = g; continue;
            }
            if (z == p->right) { // case 2
                z = p; rotate_left(root, z); p = z->parent; g = p->parent;
            }
            // case 3
            p->color = RB_BLACK; g->color = RB_RED; rotate_right(root, g);
        } else {
            vma_node_t *u = g->left;
            if (is_red(u)) { // mirror case 1
                p->color = RB_BLACK; u->color = RB_BLACK; g->color = RB_RED; z = g; continue;
            }
            if (z == p->left) { // mirror case 2
                z = p; rotate_right(root, z); p = z->parent; g = p->parent;
            }
            // mirror case 3
            p->color = RB_BLACK; g->color = RB_RED; rotate_left(root, g);
        }
    }
    (*root)->color = RB_BLACK;
}

static vma_node_t* tree_min(vma_node_t *n){ while(n && n->left) n = n->left; return n; }

static void transplant(vma_node_t **root, vma_node_t *u, vma_node_t *v) {
    if (!u->parent) *root = v;
    else if (u == u->parent->left) u->parent->left = v;
    else u->parent->right = v;
    if (v) v->parent = u->parent;
}

static void delete_fixup(vma_node_t **root, vma_node_t *x, vma_node_t *x_parent) {
    while ((x != *root) && is_black(x)) {
        if (x == (x_parent ? x_parent->left : NULL)) {
            vma_node_t *w = x_parent ? x_parent->right : NULL;
            if (is_red(w)) { w->color = RB_BLACK; if (x_parent) x_parent->color = RB_RED; rotate_left(root, x_parent); w = x_parent ? x_parent->right : NULL; }
            if (is_black(w ? w->left : NULL) && is_black(w ? w->right : NULL)) { if (w) w->color = RB_RED; x = x_parent; x_parent = x_parent ? x_parent->parent : NULL; }
            else {
                if (is_black(w ? w->right : NULL)) { if (w && w->left) w->left->color = RB_BLACK; if (w) w->color = RB_RED; rotate_right(root, w); w = x_parent ? x_parent->right : NULL; }
                if (w) w->color = x_parent ? x_parent->color : RB_BLACK;
                if (x_parent) x_parent->color = RB_BLACK;
                if (w && w->right) w->right->color = RB_BLACK;
                rotate_left(root, x_parent);
                x = *root; break;
            }
        } else {
            vma_node_t *w = x_parent ? x_parent->left : NULL;
            if (is_red(w)) { w->color = RB_BLACK; if (x_parent) x_parent->color = RB_RED; rotate_right(root, x_parent); w = x_parent ? x_parent->left : NULL; }
            if (is_black(w ? w->right : NULL) && is_black(w ? w->left : NULL)) { if (w) w->color = RB_RED; x = x_parent; x_parent = x_parent ? x_parent->parent : NULL; }
            else {
                if (is_black(w ? w->left : NULL)) { if (w && w->right) w->right->color = RB_BLACK; if (w) w->color = RB_RED; rotate_left(root, w); w = x_parent ? x_parent->left : NULL; }
                if (w) w->color = x_parent ? x_parent->color : RB_BLACK;
                if (x_parent) x_parent->color = RB_BLACK;
                if (w && w->left) w->left->color = RB_BLACK;
                rotate_right(root, x_parent);
                x = *root; break;
            }
        }
    }
    if (x) x->color = RB_BLACK;
}

static void rb_delete(vma_node_t **root, vma_node_t *z) {
    vma_node_t *y = z;
    vma_node_t *x = NULL;
    vma_node_t *x_parent = NULL;
    rb_color_t y_orig = y->color;
    if (!z->left) { x = z->right; x_parent = z->parent; transplant(root, z, z->right); }
    else if (!z->right) { x = z->left; x_parent = z->parent; transplant(root, z, z->left); }
    else {
        y = tree_min(z->right); y_orig = y->color; x = y->right; x_parent = y->parent;
        if (y->parent == z) { if (x) x->parent = y; x_parent = y; }
        else { transplant(root, y, y->right); y->right = z->right; if (y->right) y->right->parent = y; }
        transplant(root, z, y); y->left = z->left; if (y->left) y->left->parent = y; y->color = z->color;
    }
    if (y_orig == RB_BLACK) delete_fixup(root, x, x_parent);
}

static vma_node_t* find_le(vma_node_t *root, uint64_t va) {
    vma_node_t *res = NULL;
    while (root) {
        if (va < root->va) root = root->left;
        else { res = root; root = root->right; }
    }
    return res; // greatest <= va
}

static int overlaps(vma_node_t *n, uint64_t va, uint64_t size) {
    if (!n) return 0;
    uint64_t a0 = n->va, a1 = n->va + n->size;
    uint64_t b0 = va, b1 = va + size;
    return !(b1 <= a0 || a1 <= b0);
}

// For now, allocate VMA nodes from a simple static pool to avoid PMM dependency cycles
#define VMA_POOL_CAP 128
static vma_node_t vma_pool[VMA_POOL_CAP];
static size_t vma_pool_used = 0;
static vma_node_t* vma_alloc_node(void) {
    if (vma_pool_used >= VMA_POOL_CAP) return NULL;
    vma_node_t *n = &vma_pool[vma_pool_used++];
    // zero minimal fields
    n->left = n->right = n->parent = NULL; n->color = RB_RED;
    return n;
}
static void vma_free_node(vma_node_t *n) {
    (void)n; // no-op for static pool; could add a free list later
}

void vmm_init_identity(void) { /* placeholder for future page tables */ }

int vmm_init(void) {
    vma_root = NULL;
    vma_pool_used = 0;
    return 0;
}

int vmm_map(uint64_t va, uint64_t pa, uint64_t size, uint32_t attrs) {
    if (size == 0) return -1;
    if ((va & (VMM_PAGE_SIZE-1)) || (pa & (VMM_PAGE_SIZE-1)) || (size & (VMM_PAGE_SIZE-1))) return -2; // alignment
    // Check for overlaps: predecessor and its successor
    vma_node_t *pred = find_le(vma_root, va);
    if (overlaps(pred, va, size)) return -3;
    vma_node_t *succ = pred;
    // find next greater node
    if (succ) { // successor is minimal node with va > input
        vma_node_t *cur = vma_root; vma_node_t *best = NULL;
        while (cur) {
            if (cur->va > va) { best = cur; cur = cur->left; } else cur = cur->right;
        }
        succ = best;
    } else {
        // pred is NULL; check tree min as successor
        succ = tree_min(vma_root);
    }
    if (overlaps(succ, va, size)) return -3;

    vma_node_t *n = vma_alloc_node();
    if (!n) return -4;
    n->va = va; n->pa = pa; n->size = size; n->attrs = attrs; n->color = RB_RED;

    // BST insert
    vma_node_t **link = &vma_root; vma_node_t *parent = NULL;
    while (*link) {
        parent = *link;
        if (va < parent->va) link = &parent->left; else link = &parent->right;
    }
    *link = n; n->parent = parent; n->left = n->right = NULL; n->color = RB_RED;
    insert_fixup(&vma_root, n);

    // Note: page-table programming will be added later when MMU is enabled.
    return 0;
}

int vmm_unmap(uint64_t va, uint64_t size) {
    if (size == 0) return -1;
    if ((va & (VMM_PAGE_SIZE-1)) || (size & (VMM_PAGE_SIZE-1))) return -2;
    // For initial implementation, require exact match of an existing VMA
    vma_node_t *cur = vma_root;
    while (cur) {
        if (va < cur->va) cur = cur->left;
        else if (va > cur->va) cur = cur->right;
        else break;
    }
    if (!cur || cur->size != size) return -3; // not found or size mismatch

    rb_delete(&vma_root, cur);
    vma_free_node(cur);
    // TLB maintenance would go here in the future
    return 0;
}

int vmm_protect(uint64_t va, uint64_t size, uint32_t attrs) {
    if (size == 0) return -1;
    if ((va & (VMM_PAGE_SIZE-1)) || (size & (VMM_PAGE_SIZE-1))) return -2;
    vma_node_t *cur = vma_root;
    while (cur) {
        if (va < cur->va) cur = cur->left;
        else if (va > cur->va) cur = cur->right;
        else break;
    }
    if (!cur || cur->size != size) return -3;
    cur->attrs = attrs;
    // Future: update page table entries' attributes
    return 0;
}

static void inorder_dump(vma_node_t *n) {
    if (!n) return;
    inorder_dump(n->left);
    printk("VMM: VMA va=%p..%p -> pa=%p attrs=%x\n", (void*)n->va, (void*)(n->va + n->size), (void*)n->pa, (unsigned)n->attrs);
    inorder_dump(n->right);
}

void vmm_dump(void) { inorder_dump(vma_root); }

int vmm_virt_to_phys(uint64_t va, uint64_t *pa_out) {
    if (!pa_out) return -1;
    // If a VMA covers this VA, translate via offset; otherwise assume identity (early boot)
    vma_node_t *n = find_le(vma_root, va);
    if (n && va >= n->va && va < (n->va + n->size)) {
        *pa_out = n->pa + (va - n->va);
        return 0;
    }
    *pa_out = va; // identity fallback
    return 0;
}

uint64_t vmm_kernel_base(void) {
    return (uint64_t)VMM_KERNEL_VIRT_BASE;
}

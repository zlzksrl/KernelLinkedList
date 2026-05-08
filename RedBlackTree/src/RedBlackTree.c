/**
 * @file        RedBlackTree.c
 * @brief       LinuxARM-PublicLib-内核风格侵入式红黑树-实现文件
 * @details     IMX6ULL平台
 *              红黑树节点嵌入到用户结构体中，不分配/释放节点内存。
 *              参照 Linux Kernel rbtree 实现，使用侵入式设计。
 *
 * @author      zlzksrl
 * @Version     V1.0.0
 * @date        2026-05-08
 * @copyright   copyright (C) 2026
 */

/**
 * @date        2026-05-08
 * @Version     V1.0.0
 * @brief       创建文件，实现内核风格侵入式红黑树
 * @author      zlzksrl
 */

#include "../include/RedBlackTree.h"
#include <stddef.h>

/* ======================== 内部函数声明 ======================== */

static void RedBlackTree_rotate_left(struct rb_root *root, struct rb_node *node);
static void RedBlackTree_rotate_right(struct rb_root *root, struct rb_node *node);
static void RedBlackTree_insert_fixup(struct rb_root *root, struct rb_node *node);
static void RedBlackTree_delete_fixup(struct rb_root *root, struct rb_node *parent,
                            struct rb_node *node);
static void RedBlackTree_transplant(struct rb_root *root, struct rb_node *u,
                          struct rb_node *v);
static struct rb_node *RedBlackTree_subtree_min(struct rb_node *node);
static struct rb_node *RedBlackTree_subtree_max(struct rb_node *node);
static void RedBlackTree_destroy_subtree(struct rb_node *node,
                               void (*free_fn)(struct rb_node *, void *),
                               void *arg);


/* ======================== 核心操作实现 ======================== */

/**
 * @func        RedBlackTree_insert
 * @brief       向红黑树插入节点
 */
int RedBlackTree_insert(struct rb_root *root, struct rb_node *node,
              int (*cmp)(struct rb_node *, struct rb_node *, void *),
              void *arg)
{
    if (root == NULL || node == NULL || cmp == NULL)
    {
        return -1;
    }

    /* 初始化新节点为红色 */
    node->color  = REDBLACKTREE_RED;
    node->left   = NULL;
    node->right  = NULL;
    node->parent = NULL;

    /* 空树：直接作为根节点 */
    if (root->rb_node == NULL) {
        root->rb_node = node;
        root->rb_node->color = REDBLACKTREE_BLACK;
        root->count = 1;
        return 0;
    }

    /* BST 查找插入位置 */
    struct rb_node *parent = NULL;
    struct rb_node *current = root->rb_node;
    int cmp_result = 0;

    while (current != NULL) {
        parent = current;
        cmp_result = cmp(node, current, arg);

        if (cmp_result < 0) {
            current = current->left;
        } else if (cmp_result > 0) {
            current = current->right;
        } else {
            /* 键值已存在 */
            return -1;
        }
    }

    /* 插入节点 */
    node->parent = parent;
    if (cmp_result < 0) {
        parent->left = node;
    } else {
        parent->right = node;
    }

    root->count++;

    /* 修复红黑树性质 */
    RedBlackTree_insert_fixup(root, node);

    return 0;
}

/**
 * @func        RedBlackTree_delete
 * @brief       从红黑树删除节点
 */
int RedBlackTree_delete(struct rb_root *root, struct rb_node *node)
{
    if (root == NULL || node == NULL)
    {
        return -1;
    }

    struct rb_node *child = NULL;
    struct rb_node *parent = NULL;
    unsigned int color = node->color;

    if (node->left == NULL) {
        /* 只有右子节点（或无子节点） */
        child = node->right;
        parent = node->parent;  /* 在 transplant 前保存 parent */
        RedBlackTree_transplant(root, node, child);
    } else if (node->right == NULL) {
        /* 只有左子节点 */
        child = node->left;
        parent = node->parent;  /* 在 transplant 前保存 parent */
        RedBlackTree_transplant(root, node, child);
    } else {
        /* 有两个子节点：找后继节点 */
        struct rb_node *successor = RedBlackTree_subtree_min(node->right);
        color = successor->color;
        child = successor->right;

        if (successor->parent == node) {
            /* 后继是直接右子：后继取代 node 后，child 的父节点就是 successor */
            parent = successor;
        } else {
            /* 后继更深：保存后继的父节点（在 transplant 前保存） */
            parent = successor->parent;
            RedBlackTree_transplant(root, successor, child);
            successor->right = node->right;
            if (successor->right != NULL) {
                successor->right->parent = successor;
            }
        }

        RedBlackTree_transplant(root, node, successor);
        successor->left = node->left;
        if (successor->left != NULL) {
            successor->left->parent = successor;
        }
        successor->color = node->color;
    }

    /* 清除被删除节点的链接 */
    node->parent = NULL;
    node->left   = NULL;
    node->right  = NULL;

    root->count--;

    /* 如果删除的是黑色节点，需要修复 */
    if (color == REDBLACKTREE_BLACK) {
        RedBlackTree_delete_fixup(root, parent, child);
    }

    return 0;
}

/**
 * @func        RedBlackTree_search
 * @brief       在红黑树中查找节点
 */
struct rb_node *RedBlackTree_search(const struct rb_root *root, struct rb_node *key,
                           int (*cmp)(struct rb_node *, struct rb_node *, void *),
                           void *arg)
{
    if (root == NULL || key == NULL || cmp == NULL)
    {
        return NULL;
    }

    struct rb_node *current = root->rb_node;
    while (current != NULL) {
        int cmp_result = cmp(key, current, arg);
        if (cmp_result < 0) {
            current = current->left;
        } else if (cmp_result > 0) {
            current = current->right;
        } else {
            return current;
        }
    }

    return NULL;
}

/**
 * @func        RedBlackTree_replace
 * @brief       替换树中的节点
 */
int RedBlackTree_replace(struct rb_root *root, struct rb_node *old_node,
               struct rb_node *new_node)
{
    if (root == NULL || old_node == NULL || new_node == NULL)
    {
        return -1;
    }

    /* old_node 和 new_node 相同时无需替换，避免清除自身指针 */
    if (old_node == new_node)
    {
        return -1;
    }

    /* 复制旧节点的树链接到新节点 */
    new_node->parent = old_node->parent;
    new_node->left   = old_node->left;
    new_node->right  = old_node->right;
    new_node->color  = old_node->color;

    /* 更新父节点的指针 */
    if (old_node->parent == NULL) {
        root->rb_node = new_node;
    } else {
        if (old_node == old_node->parent->left) {
            old_node->parent->left = new_node;
        } else {
            old_node->parent->right = new_node;
        }
    }

    /* 更新子节点的父指针 */
    if (old_node->left != NULL) {
        old_node->left->parent = new_node;
    }
    if (old_node->right != NULL) {
        old_node->right->parent = new_node;
    }

    /* 清除旧节点的链接 */
    old_node->parent = NULL;
    old_node->left   = NULL;
    old_node->right  = NULL;

    return 0;
}


/* ======================== 迭代操作实现 ======================== */

/**
 * @func        RedBlackTree_first
 * @brief       获取红黑树最小节点
 */
struct rb_node *RedBlackTree_first(const struct rb_root *root)
{
    if (root == NULL || root->rb_node == NULL)
    {
        return NULL;
    }
    return RedBlackTree_subtree_min(root->rb_node);
}

/**
 * @func        RedBlackTree_last
 * @brief       获取红黑树最大节点
 */
struct rb_node *RedBlackTree_last(const struct rb_root *root)
{
    if (root == NULL || root->rb_node == NULL)
    {
        return NULL;
    }
    return RedBlackTree_subtree_max(root->rb_node);
}

/**
 * @func        RedBlackTree_next
 * @brief       获取后继节点
 */
struct rb_node *RedBlackTree_next(const struct rb_node *node)
{
    if (node == NULL)
    {
        return NULL;
    }

    /* 有右子树：后继是右子树的最小节点 */
    if (node->right != NULL) {
        return RedBlackTree_subtree_min(node->right);
    }

    /* 无右子树：向上找第一个"左拐"的祖先 */
    struct rb_node *parent = node->parent;
    struct rb_node *current = (struct rb_node *)node;

    while (parent != NULL && current == parent->right) {
        current = parent;
        parent = parent->parent;
    }

    return parent;
}

/**
 * @func        RedBlackTree_prev
 * @brief       获取前驱节点
 */
struct rb_node *RedBlackTree_prev(const struct rb_node *node)
{
    if (node == NULL)
    {
        return NULL;
    }

    /* 有左子树：前驱是左子树的最大节点 */
    if (node->left != NULL) {
        return RedBlackTree_subtree_max(node->left);
    }

    /* 无左子树：向上找第一个"右拐"的祖先 */
    struct rb_node *parent = node->parent;
    struct rb_node *current = (struct rb_node *)node;

    while (parent != NULL && current == parent->left) {
        current = parent;
        parent = parent->parent;
    }

    return parent;
}


/* ======================== 批量操作实现 ======================== */

/**
 * @func        RedBlackTree_destroy
 * @brief       销毁红黑树中所有节点
 */
int RedBlackTree_destroy(struct rb_root *root,
               void (*free_fn)(struct rb_node *, void *),
               void *arg)
{
    if (root == NULL)
    {
        return -1;
    }

    RedBlackTree_destroy_subtree(root->rb_node, free_fn, arg);
    root->rb_node = NULL;
    root->count = 0;

    return 0;
}

/**
 * @func        RedBlackTree_traverse
 * @brief       中序遍历红黑树
 */
int RedBlackTree_traverse(const struct rb_root *root,
                int (*callback)(struct rb_node *, void *),
                void *arg)
{
    if (root == NULL || callback == NULL)
    {
        return -1;
    }

    /* 使用 RedBlackTree_first/RedBlackTree_next 进行中序遍历 */
    struct rb_node *node = RedBlackTree_first(root);
    struct rb_node *next = NULL;

    while (node != NULL) {
        next = RedBlackTree_next(node);
        int ret = callback(node, arg);
        if (ret != 0)
        {
            return ret;
        }
        node = next;
    }

    return 0;
}


/* ======================== 内部函数实现 ======================== */

/**
 * @func        RedBlackTree_rotate_left
 * @brief       红黑树内部-左旋操作
 */
static void RedBlackTree_rotate_left(struct rb_root *root, struct rb_node *node)
{
    if (root == NULL || node == NULL || node->right == NULL)
    {
        return;
    }

    struct rb_node *right = node->right;
    node->right = right->left;

    if (right->left != NULL) {
        right->left->parent = node;
    }

    right->parent = node->parent;

    if (node->parent == NULL) {
        root->rb_node = right;
    } else if (node == node->parent->left) {
        node->parent->left = right;
    } else {
        node->parent->right = right;
    }

    right->left = node;
    node->parent = right;
}

/**
 * @func        RedBlackTree_rotate_right
 * @brief       红黑树内部-右旋操作
 */
static void RedBlackTree_rotate_right(struct rb_root *root, struct rb_node *node)
{
    if (root == NULL || node == NULL || node->left == NULL)
    {
        return;
    }

    struct rb_node *left = node->left;
    node->left = left->right;

    if (left->right != NULL) {
        left->right->parent = node;
    }

    left->parent = node->parent;

    if (node->parent == NULL) {
        root->rb_node = left;
    } else if (node == node->parent->right) {
        node->parent->right = left;
    } else {
        node->parent->left = left;
    }

    left->right = node;
    node->parent = left;
}

/**
 * @func        RedBlackTree_insert_fixup
 * @brief       红黑树内部-插入修复
 */
static void RedBlackTree_insert_fixup(struct rb_root *root, struct rb_node *node)
{
    if (root == NULL || node == NULL)
    {
        return;
    }

    while (node->parent != NULL && node->parent->color == REDBLACKTREE_RED) {
        if (node->parent == node->parent->parent->left) {
            struct rb_node *uncle = node->parent->parent->right;

            if (uncle != NULL && uncle->color == REDBLACKTREE_RED) {
                /* Case 1: 叔节点是红色 */
                node->parent->color = REDBLACKTREE_BLACK;
                uncle->color = REDBLACKTREE_BLACK;
                node->parent->parent->color = REDBLACKTREE_RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->right) {
                    /* Case 2: LR 型 */
                    node = node->parent;
                    RedBlackTree_rotate_left(root, node);
                }
                /* Case 3: LL 型 */
                node->parent->color = REDBLACKTREE_BLACK;
                node->parent->parent->color = REDBLACKTREE_RED;
                RedBlackTree_rotate_right(root, node->parent->parent);
            }
        } else {
            struct rb_node *uncle = node->parent->parent->left;

            if (uncle != NULL && uncle->color == REDBLACKTREE_RED) {
                /* Case 1: 叔节点是红色（镜像） */
                node->parent->color = REDBLACKTREE_BLACK;
                uncle->color = REDBLACKTREE_BLACK;
                node->parent->parent->color = REDBLACKTREE_RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->left) {
                    /* Case 2: RL 型 */
                    node = node->parent;
                    RedBlackTree_rotate_right(root, node);
                }
                /* Case 3: RR 型 */
                node->parent->color = REDBLACKTREE_BLACK;
                node->parent->parent->color = REDBLACKTREE_RED;
                RedBlackTree_rotate_left(root, node->parent->parent);
            }
        }
    }

    root->rb_node->color = REDBLACKTREE_BLACK;
}

/**
 * @func        RedBlackTree_transplant
 * @brief       红黑树内部-节点替换（将 v 替换 u 的位置）
 */
static void RedBlackTree_transplant(struct rb_root *root, struct rb_node *u,
                          struct rb_node *v)
{
    if (root == NULL || u == NULL)
    {
        return;
    }

    if (u->parent == NULL) {
        root->rb_node = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }

    if (v != NULL) {
        v->parent = u->parent;
    }
}

/**
 * @func        RedBlackTree_subtree_min
 * @brief       红黑树内部-获取子树最小节点（最左节点）
 */
static struct rb_node *RedBlackTree_subtree_min(struct rb_node *node)
{
    if (node == NULL)
    {
        return NULL;
    }
    while (node->left != NULL) {
        node = node->left;
    }
    return node;
}

/**
 * @func        RedBlackTree_subtree_max
 * @brief       红黑树内部-获取子树最大节点（最右节点）
 */
static struct rb_node *RedBlackTree_subtree_max(struct rb_node *node)
{
    if (node == NULL)
    {
        return NULL;
    }
    while (node->right != NULL) {
        node = node->right;
    }
    return node;
}

/**
 * @func        RedBlackTree_delete_fixup
 * @brief       红黑树内部-删除修复
 * @param       root    红黑树根
 * @param       parent  被删除位置的父节点（child 可能为 NULL 时需要）
 * @param       node    需要修复的节点（可能为 NULL）
 */
static void RedBlackTree_delete_fixup(struct rb_root *root, struct rb_node *parent,
                            struct rb_node *node)
{
    if (root == NULL)
    {
        return;
    }

    while ((node == NULL || node->color == REDBLACKTREE_BLACK) &&
           node != root->rb_node) {
        if (node == parent->left) {
            struct rb_node *sibling = parent->right;

            if (sibling != NULL && sibling->color == REDBLACKTREE_RED) {
                /* Case 1: 兄弟节点是红色 */
                sibling->color = REDBLACKTREE_BLACK;
                parent->color = REDBLACKTREE_RED;
                RedBlackTree_rotate_left(root, parent);
                sibling = parent->right;
            }

            if ((sibling == NULL) ||
                ((sibling->left == NULL || sibling->left->color == REDBLACKTREE_BLACK) &&
                 (sibling->right == NULL || sibling->right->color == REDBLACKTREE_BLACK))) {
                /* Case 2: 兄弟节点的两个子节点都是黑色 */
                if (sibling != NULL)
                {
                    sibling->color = REDBLACKTREE_RED;
                }
                node = parent;
                parent = node->parent;
            } else {
                if (sibling->right == NULL ||
                    sibling->right->color == REDBLACKTREE_BLACK) {
                    /* Case 3: 兄弟节点的右子节点是黑色 */
                    if (sibling->left != NULL)
                    {
                        sibling->left->color = REDBLACKTREE_BLACK;
                    }
                    sibling->color = REDBLACKTREE_RED;
                    RedBlackTree_rotate_right(root, sibling);
                    sibling = parent->right;
                }
                /* Case 4: 兄弟节点的右子节点是红色 */
                if (sibling != NULL) {
                    sibling->color = parent->color;
                    parent->color = REDBLACKTREE_BLACK;
                    if (sibling->right != NULL)
                    {
                        sibling->right->color = REDBLACKTREE_BLACK;
                    }
                    RedBlackTree_rotate_left(root, parent);
                }
                node = root->rb_node;
            }
        } else {
            struct rb_node *sibling = parent->left;

            if (sibling != NULL && sibling->color == REDBLACKTREE_RED) {
                /* Case 1: 兄弟节点是红色（镜像） */
                sibling->color = REDBLACKTREE_BLACK;
                parent->color = REDBLACKTREE_RED;
                RedBlackTree_rotate_right(root, parent);
                sibling = parent->left;
            }

            if ((sibling == NULL) ||
                ((sibling->right == NULL || sibling->right->color == REDBLACKTREE_BLACK) &&
                 (sibling->left == NULL || sibling->left->color == REDBLACKTREE_BLACK))) {
                /* Case 2: 兄弟节点的两个子节点都是黑色（镜像） */
                if (sibling != NULL)
                {
                    sibling->color = REDBLACKTREE_RED;
                }
                node = parent;
                parent = node->parent;
            } else {
                if (sibling->left == NULL ||
                    sibling->left->color == REDBLACKTREE_BLACK) {
                    /* Case 3: 兄弟节点的左子节点是黑色（镜像） */
                    if (sibling->right != NULL)
                    {
                        sibling->right->color = REDBLACKTREE_BLACK;
                    }
                    sibling->color = REDBLACKTREE_RED;
                    RedBlackTree_rotate_left(root, sibling);
                    sibling = parent->left;
                }
                /* Case 4: 兄弟节点的左子节点是红色（镜像） */
                if (sibling != NULL) {
                    sibling->color = parent->color;
                    parent->color = REDBLACKTREE_BLACK;
                    if (sibling->left != NULL)
                    {
                        sibling->left->color = REDBLACKTREE_BLACK;
                    }
                    RedBlackTree_rotate_right(root, parent);
                }
                node = root->rb_node;
            }
        }
    }

    if (node != NULL) {
        node->color = REDBLACKTREE_BLACK;
    }
}

/**
 * @func        RedBlackTree_destroy_subtree
 * @brief       红黑树内部-递归销毁子树（后序遍历）
 */
static void RedBlackTree_destroy_subtree(struct rb_node *node,
                               void (*free_fn)(struct rb_node *, void *),
                               void *arg)
{
    if (node == NULL)
    {
        return;
    }

    RedBlackTree_destroy_subtree(node->left, free_fn, arg);
    RedBlackTree_destroy_subtree(node->right, free_fn, arg);

    if (free_fn != NULL) {
        free_fn(node, arg);
    }
}
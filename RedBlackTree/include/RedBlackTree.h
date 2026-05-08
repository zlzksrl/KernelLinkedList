/**
 * @file        RedBlackTree.h
 * @brief       LinuxARM-PublicLib-内核风格侵入式红黑树-公共API头文件
 * @details     IMX6ULL平台
 *              本文件提供 Linux 内核风格的侵入式红黑树实现，
 *              参照 Linux Kernel rbtree 设计，红黑树节点嵌入到用户结构体中，
 *              通过 rb_entry / container_of 宏反查宿主结构体。
 *
 *              核心特性:
 *              - 侵入式设计: 红黑树节点嵌入用户数据结构中，无需额外分配节点内存
 *              - 自平衡BST:  保证查找/插入/删除 O(log n) 时间复杂度
 *              - 类型安全:   通过 rb_entry 宏从红黑树节点反查宿主结构体
 *              - 用户自定义比较: 通过比较函数指针支持任意键类型
 *              - 非线程安全: 多线程环境需调用者自行加锁保护
 *
 *              使用示例:
 *              @code
 *              struct my_task {
 *                  int id;
 *                  struct rb_node node;   // 红黑树节点嵌入
 *              };
 *
 *              // 比较函数
 *              int my_cmp(struct rb_node *a, struct rb_node *b, void *arg) {
 *                  struct my_task *ta = rb_entry(a, struct my_task, node);
 *                  struct my_task *tb = rb_entry(b, struct my_task, node);
 *                  (void)arg;
 *                  return ta->id - tb->id;
 *              }
 *
 *              // 初始化红黑树
 *              DEFINE_RB_ROOT(my_tree);
 *
 *              // 插入
 *              struct my_task *task = malloc(sizeof(*task));
 *              task->id = 42;
 *              rb_init_node(&task->node);
 *              rb_insert(&my_tree, &task->node, my_cmp, NULL);
 *
 *              // 查找
 *              struct my_task key = { .id = 42 };
 *              struct rb_node *found = rb_search(&my_tree, &key.node, my_cmp, NULL);
 *              if (found) {
 *                  struct my_task *t = rb_entry(found, struct my_task, node);
 *                  printf("Found: %d\n", t->id);
 *              }
 *
 *              // 遍历
 *              struct rb_node *pos;
 *              rb_for_each(pos, &my_tree) {
 *                  struct my_task *t = rb_entry(pos, struct my_task, node);
 *                  printf("id = %d\n", t->id);
 *              }
 *
 *              // 删除
 *              rb_delete(&my_tree, &task->node);
 *              free(task);
 *              @endcode
 *
 * @author      zlzksrl
 * @Version     V1.0.0
 * @date        2026-05-08
 * @copyright   copyright (C) 2026
 */

/**
 * @date        2026-05-08
 * @Version     V1.0.0
 * @brief       创建文件，提供内核风格侵入式红黑树全套API
 * @author      zlzksrl
 */
#ifndef REDBLACKTREE_H_
#define REDBLACKTREE_H_

#include "KernelLinkedList.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ================================================================== */
/*                                                                    */
/*     红黑树节点与根定义                                                */
/*                                                                    */
/*  说明:                                                              */
/*    rb_node 是红黑树的核心节点结构体，                                */
/*    仅包含父指针、左右子指针和颜色，不承载数据。                       */
/*    用户将此结构体嵌入自己的数据结构中来使用红黑树功能。                 */
/*                                                                    */
/*    rb_root 是红黑树的根结构体，                                      */
/*    包含根节点指针和节点计数。                                         */
/*                                                                    */
/*  初始化方式:                                                        */
/*    1. 静态初始化: RB_ROOT_INIT 或 DEFINE_RB_ROOT(name)             */
/*    2. 动态初始化: rb_init_root(ptr)                                */
/*                                                                    */
/* ================================================================== */

/** 红黑树节点颜色常量: 黑色 */
#define RB_BLACK   0

/** 红黑树节点颜色常量: 红色 */
#define RB_RED     1

/**
 * @struct      rb_node
 * @brief       红黑树节点结构体
 * @details     内核风格红黑树的核心数据结构。
 *              用户应将此结构体作为成员嵌入到自定义数据结构中。
 *              所有字段均为内部使用，用户不应直接访问或修改。
 */
struct rb_node {
    struct rb_node *parent;     /**< 父节点指针（内部使用） */
    struct rb_node *left;       /**< 左子节点指针（内部使用） */
    struct rb_node *right;      /**< 右子节点指针（内部使用） */
    unsigned int color;         /**< 节点颜色（内部使用，RB_BLACK / RB_RED） */
};

/**
 * @struct      rb_root
 * @brief       红黑树根结构体
 * @details     包含红黑树的根节点指针和节点计数。
 *              节点计数由库内部维护，用户不应直接修改。
 */
struct rb_root {
    struct rb_node *rb_node;    /**< 根节点指针 */
    unsigned int count;         /**< 树中节点数量（内部维护） */
};


/* ================================================================== */
/*                                                                    */
/*     入口获取宏 (Entry Access Macros)                                */
/*                                                                    */
/*  说明:                                                              */
/*    通过 rb_node 指针获取包含它的宿主结构体指针。                     */
/*    这些宏是侵入式红黑树的核心优势所在。                               */
/*                                                                    */
/*  API列表:                                                          */
/*    rb_entry      - 从 rb_node 指针获取宿主结构体指针                 */
/*    rb_entry_safe - 安全版本的 rb_entry（处理 NULL 指针）             */
/*                                                                    */
/* ================================================================== */

/**
 * @macro       rb_entry
 * @brief       通过红黑树节点指针获取宿主结构体指针
 * @details     container_of 的封装，通过成员指针反查宿主结构体首地址。
 * @param       ptr     红黑树节点指针 (struct rb_node *)
 * @param       type    宿主结构体类型
 * @param       member  rb_node 在宿主结构体中的成员名
 * @return      指向宿主结构体的指针 (type *)
 * @par 使用示例:
 * @code
 *   struct my_task *task = rb_entry(node_ptr, struct my_task, node);
 * @endcode
 */
#define rb_entry(ptr, type, member) \
    container_of(ptr, type, member)

/**
 * @macro       rb_entry_safe
 * @brief       安全版本的 rb_entry（处理 NULL 指针）
 * @details     当 ptr 为 NULL 时返回 NULL，避免解引用空指针。
 * @param       ptr     红黑树节点指针
 * @param       type    宿主结构体类型
 * @param       member  rb_node 在宿主结构体中的成员名
 * @return      指向宿主结构体的指针 (type *)，ptr 为 NULL 时返回 NULL
 */
#define rb_entry_safe(ptr, type, member) ({                          \
    const typeof( ((type *)0)->member ) *__ptr = (ptr);             \
    __ptr ? rb_entry(__ptr, type, member) : (type *)0; })


/* ================================================================== */
/*                                                                    */
/*     初始化                                                          */
/*                                                                    */
/*  API列表:                                                          */
/*    RB_ROOT_INIT   - 静态初始化器                                     */
/*    DEFINE_RB_ROOT - 定义并静态初始化红黑树根                          */
/*    rb_init_root   - 运行时初始化红黑树根                              */
/*    rb_init_node   - 初始化红黑树节点                                  */
/*                                                                    */
/* ================================================================== */

/**
 * @macro       RB_ROOT_INIT
 * @brief       红黑树根的静态初始化器
 * @details     用于在变量声明时进行静态初始化，使根节点指针为 NULL，计数为 0。
 * @return      初始化器 { NULL, 0 }
 * @note        通常使用 DEFINE_RB_ROOT 宏代替直接使用此宏
 */
#define RB_ROOT_INIT (struct rb_root){ NULL, 0 }

/**
 * @macro       DEFINE_RB_ROOT
 * @brief       定义并静态初始化一个红黑树根
 * @details     声明一个 struct rb_root 变量并静态初始化为空树。
 * @param       name: 红黑树根变量名
 * @par 使用示例:
 * @code
 *   DEFINE_RB_ROOT(my_tree);  // 定义并初始化一棵空红黑树
 * @endcode
 */
#define DEFINE_RB_ROOT(name) \
    struct rb_root name = RB_ROOT_INIT

/**
 * @func        rb_init_root
 * @brief       运行时初始化红黑树根
 * @details     将根节点指针置 NULL，计数归零。
 *              适用于在运行时初始化红黑树根（如动态分配的结构体成员）。
 * @param[in]   root: 红黑树根指针
 * @warning     root 不能为 NULL
 */
static inline void rb_init_root(struct rb_root *root)
{
    root->rb_node = NULL;
    root->count = 0;
}

/**
 * @func        rb_init_node
 * @brief       初始化红黑树节点
 * @details     将节点颜色设为红色，所有指针置 NULL。
 *              在调用 rb_insert 之前，用户应先调用此函数初始化节点。
 * @param[in]   node: 红黑树节点指针
 * @warning     node 不能为 NULL
 * @note        rb_insert 内部也会重新初始化节点，但显式调用此函数是推荐做法
 */
static inline void rb_init_node(struct rb_node *node)
{
    node->color  = RB_RED;
    node->parent = NULL;
    node->left   = NULL;
    node->right  = NULL;
}


/* ================================================================== */
/*                                                                    */
/*     查询操作                                                        */
/*                                                                    */
/*  API列表:                                                          */
/*    rb_count - 获取红黑树节点数量                                      */
/*    rb_empty - 判断红黑树是否为空                                       */
/*                                                                    */
/* ================================================================== */

/**
 * @func        rb_count
 * @brief       获取红黑树中的节点数量
 * @param[in]   root: 红黑树根指针
 * @return      unsigned int 节点数量
 * @retval      >=0: 节点数量（root 为 NULL 时返回 0）
 */
static inline unsigned int rb_count(const struct rb_root *root)
{
    if (root == NULL)
    {
        return 0;
    }
    return root->count;
}

/**
 * @func        rb_empty
 * @brief       判断红黑树是否为空
 * @param[in]   root: 红黑树根指针
 * @return      int
 * @retval      1: 红黑树为空（或 root 为 NULL）
 * @retval      0: 红黑树不为空
 */
static inline int rb_empty(const struct rb_root *root)
{
    if (root == NULL)
    {
        return 1;
    }
    return root->rb_node == NULL;
}


/* ================================================================== */
/*                                                                    */
/*     核心操作                                                        */
/*                                                                    */
/*  API列表:                                                          */
/*    rb_insert  - 向红黑树插入节点                                      */
/*    rb_delete  - 从红黑树删除节点                                      */
/*    rb_search  - 在红黑树中查找节点                                    */
/*    rb_replace - 替换树中的节点                                        */
/*                                                                    */
/*  注意:                                                              */
/*    - 所有节点由用户分配和释放内存，红黑树不负责内存管理                  */
/*    - 比较函数返回值: <0 表示 a<b, >0 表示 a>b, =0 表示相等             */
/*    - rb_delete 后节点的内存由用户负责释放                              */
/*                                                                    */
/* ================================================================== */

/**
 * @func        rb_insert
 * @brief       向红黑树插入节点
 * @details     将新节点插入到红黑树中并保持平衡性质。
 *              如果键值已存在则插入失败。
 *              新节点由用户分配内存，红黑树不负责分配/释放节点。
 * @param[in]   root:  红黑树根指针
 * @param[in]   node:  新节点指针（已由用户初始化并设置好 key）
 * @param[in]   cmp:   比较函数指针
 *                      int (*cmp)(struct rb_node *a, struct rb_node *b, void *arg)
 *                      返回值 <0 表示 a<b, >0 表示 a>b, =0 表示相等
 * @param[in]   arg:   传递给比较函数的用户参数
 * @return      int
 * @retval      0:  成功
 * @retval      -1: 失败（键值已存在或参数无效）
 * @warning     node 必须未被插入到任何红黑树中
 * @par 使用示例:
 * @code
 *   struct my_task *task = malloc(sizeof(*task));
 *   task->id = 42;
 *   rb_init_node(&task->node);
 *   rb_insert(&root, &task->node, my_cmp, NULL);
 * @endcode
 */
int rb_insert(struct rb_root *root, struct rb_node *node,
              int (*cmp)(struct rb_node *, struct rb_node *, void *),
              void *arg);

/**
 * @func        rb_delete
 * @brief       从红黑树删除节点
 * @details     将指定节点从红黑树中移除并保持平衡性质。
 *              删除后节点的指针被清零，但不释放内存。
 * @param[in]   root: 红黑树根指针
 * @param[in]   node: 要删除的节点指针
 * @return      int
 * @retval      0:  成功
 * @retval      -1: 失败（参数无效）
 * @warning     node 必须在 root 所指的红黑树中
 * @warning     删除后需由用户负责释放 node 所属宿主结构体的内存
 * @par 使用示例:
 * @code
 *   rb_delete(&root, &task->node);
 *   free(task);  // 用户负责释放
 * @endcode
 */
int rb_delete(struct rb_root *root, struct rb_node *node);

/**
 * @func        rb_search
 * @brief       在红黑树中查找节点
 * @details     使用二叉搜索查找指定键值对应的节点。
 * @param[in]   root: 红黑树根指针
 * @param[in]   key:  包含 key 的模板节点（仅用于比较）
 * @param[in]   cmp:  比较函数指针（与 rb_insert 使用相同的比较函数）
 * @param[in]   arg:  传递给比较函数的用户参数
 * @return      struct rb_node* 找到的节点指针
 * @retval      非 NULL: 找到的节点
 * @retval      NULL: 未找到或参数无效
 * @par 使用示例:
 * @code
 *   struct my_task key = { .id = 42 };
 *   struct rb_node *found = rb_search(&root, &key.node, my_cmp, NULL);
 *   if (found) {
 *       struct my_task *t = rb_entry(found, struct my_task, node);
 *       printf("Found id=%d\n", t->id);
 *   }
 * @endcode
 */
struct rb_node *rb_search(const struct rb_root *root, struct rb_node *key,
                           int (*cmp)(struct rb_node *, struct rb_node *, void *),
                           void *arg);

/**
 * @func        rb_replace
 * @brief       替换树中的节点
 * @details     将 old_node 从红黑树中移除，并将 new_node 插入到 old_node
 *              原来的位置。new_node 继承 old_node 的颜色和树位置。
 *              要求 new_node 的键值与 old_node 相同。
 * @param[in]   root:     红黑树根指针
 * @param[in]   old_node: 旧节点（必须在树中）
 * @param[in]   new_node: 新节点（必须不在任何树中）
 * @return      int
 * @retval      0:  成功
 * @retval      -1: 失败（参数无效）
 * @warning     不会触发重平衡，因此新旧节点的键值必须相同
 * @note        替换后 old_node 的指针被清零
 */
int rb_replace(struct rb_root *root, struct rb_node *old_node,
               struct rb_node *new_node);


/* ================================================================== */
/*                                                                    */
/*     迭代操作                                                        */
/*                                                                    */
/*  API列表:                                                          */
/*    rb_first - 获取红黑树中最小节点（最左节点）                        */
/*    rb_last  - 获取红黑树中最大节点（最右节点）                        */
/*    rb_next  - 获取指定节点的后继节点                                  */
/*    rb_prev  - 获取指定节点的前驱节点                                  */
/*                                                                    */
/*  时间复杂度: O(log n) 均摊 O(1)                                     */
/*                                                                    */
/* ================================================================== */

/**
 * @func        rb_first
 * @brief       获取红黑树中最小节点（最左节点）
 * @param[in]   root: 红黑树根指针
 * @return      struct rb_node* 最小节点
 * @retval      非 NULL: 最小节点
 * @retval      NULL: 空树或参数无效
 */
struct rb_node *rb_first(const struct rb_root *root);

/**
 * @func        rb_last
 * @brief       获取红黑树中最大节点（最右节点）
 * @param[in]   root: 红黑树根指针
 * @return      struct rb_node* 最大节点
 * @retval      非 NULL: 最大节点
 * @retval      NULL: 空树或参数无效
 */
struct rb_node *rb_last(const struct rb_root *root);

/**
 * @func        rb_next
 * @brief       获取指定节点的后继节点（中序遍历的下一个节点）
 * @param[in]   node: 当前节点
 * @return      struct rb_node* 后继节点
 * @retval      非 NULL: 后继节点
 * @retval      NULL: 无后继（node 是最大节点）或参数无效
 */
struct rb_node *rb_next(const struct rb_node *node);

/**
 * @func        rb_prev
 * @brief       获取指定节点的前驱节点（中序遍历的上一个节点）
 * @param[in]   node: 当前节点
 * @return      struct rb_node* 前驱节点
 * @retval      非 NULL: 前驱节点
 * @retval      NULL: 无前驱（node 是最小节点）或参数无效
 */
struct rb_node *rb_prev(const struct rb_node *node);


/* ================================================================== */
/*                                                                    */
/*     批量操作                                                        */
/*                                                                    */
/*  API列表:                                                          */
/*    rb_destroy  - 销毁红黑树中所有节点                                */
/*    rb_traverse - 中序遍历红黑树                                      */
/*                                                                    */
/* ================================================================== */

/**
 * @func        rb_destroy
 * @brief       销毁红黑树中所有节点
 * @details     移除红黑树中的所有节点，并通过回调函数释放用户内存。
 *              销毁后红黑树变为空树。
 * @param[in]   root:     红黑树根指针
 * @param[in]   free_fn:  节点释放回调函数，NULL 表示不释放
 *                          void (*free_fn)(struct rb_node *node, void *arg)
 * @param[in]   arg:      传递给释放回调的用户参数
 * @return      int
 * @retval      0:  成功
 * @retval      -1: 失败（参数无效）
 * @warning     内部使用递归后序遍历，极深的树可能导致栈溢出
 * @par 使用示例:
 * @code
 *   void my_free(struct rb_node *node, void *arg) {
 *       struct my_task *t = rb_entry(node, struct my_task, node);
 *       free(t);
 *   }
 *   rb_destroy(&root, my_free, NULL);
 * @endcode
 */
int rb_destroy(struct rb_root *root,
               void (*free_fn)(struct rb_node *, void *),
               void *arg);

/**
 * @func        rb_traverse
 * @brief       中序遍历红黑树
 * @details     按键值从小到大的顺序遍历红黑树中的所有节点。
 *              遍历期间不应修改树结构（不要插入/删除节点）。
 * @param[in]   root:     红黑树根指针
 * @param[in]   callback: 遍历回调函数，返回非 0 则停止遍历
 *                          int (*callback)(struct rb_node *node, void *arg)
 * @param[in]   arg:      传递给回调的用户参数
 * @return      int
 * @retval      0:   遍历完成
 * @retval      非0:  回调函数的返回值（提前停止）
 */
int rb_traverse(const struct rb_root *root,
                int (*callback)(struct rb_node *, void *),
                void *arg);


/* ================================================================== */
/*                                                                    */
/*     遍历宏 (Iteration Macros)                                       */
/*                                                                    */
/*  说明:                                                              */
/*    提供多种遍历方式，分为基础遍历和安全遍历两类:                       */
/*    - 基础遍历: 遍历期间不可删除当前节点                               */
/*    - 安全遍历 (_safe): 预存下一个节点，允许删除当前节点                 */
/*                                                                    */
/*  API列表:                                                          */
/*    rb_for_each              - 基础正向遍历 (rb_node 指针)             */
/*    rb_for_each_reverse      - 基础反向遍历 (rb_node 指针)             */
/*    rb_for_each_safe         - 安全正向遍历（可删除当前节点）            */
/*    rb_for_each_entry        - 正向遍历并获取宿主结构体                 */
/*    rb_for_each_entry_safe   - 安全正向遍历并获取宿主结构体（可删除）   */
/*                                                                    */
/* ================================================================== */

/**
 * @macro       rb_for_each
 * @brief       从最小节点开始正向遍历红黑树
 * @details     按中序遍历顺序（键值从小到大）遍历所有节点。
 *              遍历期间不可删除当前节点 pos。
 * @param       pos:  循环变量 (struct rb_node *)
 * @param       root: 红黑树根指针
 * @par 使用示例:
 * @code
 *   struct rb_node *pos;
 *   rb_for_each(pos, &root) {
 *       struct my_task *t = rb_entry(pos, struct my_task, node);
 *       printf("id = %d\n", t->id);
 *   }
 * @endcode
 */
#define rb_for_each(pos, root) \
    for (pos = rb_first(root); pos; pos = rb_next(pos))

/**
 * @macro       rb_for_each_reverse
 * @brief       从最大节点开始反向遍历红黑树
 * @details     按逆中序遍历顺序（键值从大到小）遍历所有节点。
 *              遍历期间不可删除当前节点 pos。
 * @param       pos:  循环变量 (struct rb_node *)
 * @param       root: 红黑树根指针
 */
#define rb_for_each_reverse(pos, root) \
    for (pos = rb_last(root); pos; pos = rb_prev(pos))

/**
 * @macro       rb_for_each_safe
 * @brief       安全正向遍历红黑树（可删除当前节点）
 * @details     预存下一个节点到 n，允许在循环体内安全删除 pos。
 * @param       pos:  循环变量，当前节点 (struct rb_node *)
 * @param       n:    临时变量，预存下一个节点 (struct rb_node *)
 * @param       root: 红黑树根指针
 * @par 使用示例:
 * @code
 *   struct rb_node *pos, *n;
 *   rb_for_each_safe(pos, n, &root) {
 *       rb_delete(&root, pos);
 *       free(rb_entry(pos, struct my_task, node));
 *   }
 * @endcode
 */
#define rb_for_each_safe(pos, n, root) \
    for (pos = rb_first(root), n = pos ? rb_next(pos) : NULL; \
         pos; pos = n, n = pos ? rb_next(pos) : NULL)

/**
 * @macro       rb_for_each_entry
 * @brief       正向遍历红黑树并直接获取宿主结构体指针
 * @details     按中序遍历顺序遍历，直接获取包含 rb_node 的宿主结构体。
 *              遍历期间不可删除当前节点 pos。
 * @param       pos:    循环变量（宿主结构体指针）
 * @param       root:   红黑树根指针
 * @param       member: rb_node 在宿主结构体中的成员名
 */
#define rb_for_each_entry(pos, root, member) \
    for (pos = rb_entry_safe(rb_first(root), typeof(*pos), member); \
         pos; \
         pos = rb_entry_safe(rb_next(&pos->member), typeof(*pos), member))

/**
 * @macro       rb_for_each_entry_safe
 * @brief       安全正向遍历红黑树并获取宿主结构体（可删除当前节点）
 * @details     预存下一个宿主结构体指针到 n，允许在循环体内安全删除 pos。
 * @param       pos:    循环变量，当前节点的宿主结构体指针
 * @param       n:      临时变量，预存下一个节点的宿主结构体指针
 * @param       root:   红黑树根指针
 * @param       member: rb_node 在宿主结构体中的成员名
 * @par 使用示例:
 * @code
 *   struct my_task *pos, *n;
 *   rb_for_each_entry_safe(pos, n, &root, node) {
 *       rb_delete(&root, &pos->node);
 *       free(pos);
 *   }
 * @endcode
 */
#define rb_for_each_entry_safe(pos, n, root, member) \
    for (pos = rb_entry_safe(rb_first(root), typeof(*pos), member), \
         n = pos ? rb_entry_safe(rb_next(&pos->member), typeof(*pos), member) : NULL; \
         pos; \
         pos = n, \
         n = pos ? rb_entry_safe(rb_next(&pos->member), typeof(*pos), member) : NULL)


#ifdef __cplusplus
}
#endif

#endif
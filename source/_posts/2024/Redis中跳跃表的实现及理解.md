---
title: Redis中跳跃表的实现及理解
date: 2024-10-01 13:57:44
tags:
- 数据结构与算法
categories:
- C++
---

# 跳跃表的主要特点包括

*   有序性：跳跃表中的元素是有序的，可以快速地进行范围查询。
*   概率性：跳跃表的高度是随机决定的，这使得它在平均情况下具有对数时间复杂度。
*   动态性：跳跃表可以在运行时动态地添加和删除元素，而不需要重新构建整个结构。
*   空间效率：相比于平衡树，跳跃表的空间效率更高，因为它不需要存储指向父节点的指针。

在 Redis 中，跳跃表被用于实现有序集合，它允许用户添加、删除、更新和查询元素，并且可以按照分数对元素进行排序。

![Redis的zskiplist](https://nullcc.github.io/assets/images/post_imgs/redis_data_structure_10.png "Redis的zskiplist")

**数据结构定义：**\
Redis 中的跳跃表由 zskiplistNode 和 zskiplist 两个结构体定义。zskiplistNode 表示跳跃表的节点，包含成员对象 obj、分值 score、后退指针 backward 以及层 level 信息；zskiplist 表示跳跃表本身，包含头尾节点指针、长度和层高信息。

**节点层级：**\
跳跃表的每个节点可以有多个层，称为索引层，每个索引层包含一个前向指针 forward 和跨度 span。层高是随机生成的，遵循幂次定律，最大层高为 32。

**创建跳跃表：**\
使用 zslCreate 函数创建一个新的跳跃表，初始化层高为 1，长度为 0，并创建头节点，头节点的层高为 32，其余节点的层高根据需要动态生成。

**插入节点：**\
插入操作首先确定新节点的层高，然后从高层向低层搜索插入位置，并更新 update 数组，该数组记录所有需要调整的前置节点。接着，创建新节点，并根据 update 数组和 rank 数组更新跨度和前向指针。

**查找操作：**\
查找操作从高层开始，沿着链表前进；遇到大于目标值的节点时下降到下一层，继续查找。经过的所有节点的跨度之和即为目标节点的排位（rank）。

**删除节点：**\
删除操作根据分值和对象找到待删除节点，并更新相关节点的前向指针和跨度。如果节点在多层中存在，需要逐层删除。

**性能分析：**\
跳跃表支持平均 O(logN)、最坏 O(N) 复杂度的节点查找，且实现比平衡树简单。在有序集合中，跳跃表可以处理元素数量较多或元素成员较长的情况。

**Redis 应用场景：**\
Redis 使用跳跃表实现有序集合键，特别是当集合中的元素数量较多或元素的成员是较长的字符串时。跳跃表也用于 Redis 集群节点中的内部数据结构。

**跳跃表的优点：**\
跳跃表的优点包括支持快速的查找操作，以及在实现上相对简单。它通过维护多个层级的链表来提高查找效率，且可以顺序性地批量处理节点。

**跳跃表的实现细节：**\
跳跃表的实现细节包括节点的创建、插入、删除、搜索等操作，以及维护跳跃表的最大层高和节点数量等信息。具体实现可以参考 Redis 源码中的server.h和t\_zset.c 文件。

基本数据结构：

```c
typedef struct zskiplist { //跳表的元数据信息
    struct zskiplistNode *header, *tail; //头指针和尾指针
    unsigned long length; //跳表的长度,即除头节点之外的节点总数
    int level; //层高,即除头节点之外的节点层高的最大值
} zskiplist;

typedef struct zskiplistNode { //跳表的节点
    sds ele; //有序集合中成员的名字
    double score; //有序集合中成员的分值
    struct zskiplistNode *backward; //后退指针
    struct zskiplistLevel {
        struct zskiplistNode *forward; //指向下一个节点的指针
        unsigned long span; //跨度,或者说叫距离,跨过3个节点span是4
    } level[]; //节点中层这个概念通过柔性数组实现,每一层是数组中的一个元素
} zskiplistNode;

int zslRandomLevel(void) {
    int level = 1;
    while ((random()&0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
        level += 1;
    return (level<ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}
```

主要操作：

创建节点和跳表：

```c
//创建跳表
zskiplist *zslCreate(void) {
    int j;
    //声明跳表元数据节点
    zskiplist *zsl;
    
    //为了存储元数据的节点分配内存
    zsl = zmalloc(sizeof(*zsl));
    zsl->level = 1;
    zsl->length = 0;
    //创建zsl的头节点指针指向刚创建的跳表头节点,层数为32层,分数为0,不存储数据
    zsl->header = zzslCreateNode(ZSKIPLIST_MAXLEVEL,0,NULL);
    //从0到63初始化头节点每一层的前进指针为空且跨度为0
    for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
        zsl->header->level[j].forward = NULL;
        zsl->header->level[j].span = 0;
    }
    //设置头节点的后退指针为空
    zsl->header->backward = NULL;
    //设置跳表的尾节点为空,因为现在还没有节点,只有一个头节点
    zsl->tail = NULL;
    return zsl;
}

//创建跳表节点
zskiplistNode *zslCreateNode(int level, double score, sds ele) {
    //为跳表节点声明空间,大小为zskiplistNode加上用来描述层级的level数组的大小
    zskiplistNode *zn =
        zmalloc(sizeof(*zn)+level*sizeof(struct zskiplistLevel));
    //设置分数和数据
    zn->score = score;
    zn->ele = ele;
    return zn;
}
```

插入节点：

```c
#define ZSKIPLIST_P 0.25
#define ZSKIPLIST_MAXLEVEL 32

int zslRandomLevel(void) {
    int level = 1;
    //左边是随机生成一个值,取低16位,相当于从1到65535这种取值,右边是0.25乘以65535,所以就是有四分之一的几率进到循环里面让层高加1
    while ((random()&0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
        level += 1;
    //如果层高大于等于32了,就用32,反之用随机加出来的值
    return (level<ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}



/* Insert a new node in the skiplist. Assumes the element does not already
 * exist (up to the caller to enforce that). The skiplist takes ownership
 * of the passed SDS string 'ele'. */
zskiplistNode *zslInsert(zskiplist *zsl, double score, sds ele) {
    //update数组存储的是每一层中要被插入位置的前一个节点
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    //rank数组存储的是update节点在所有节点中的排名,计算方式是跨度的累加
    unsigned int rank[ZSKIPLIST_MAXLEVEL];
    int i, level;

    serverAssert(!isnan(score));
    
    //x指向头节点
    x = zsl->header;
    //从跳表当前最高层开始,数组从0开始而level从1开始所以这里减个1
    for (i = zsl->level-1; i >= 0; i--) {
        //如果当前是最高层则rank[i]为0否则等于上一层的rank值
        rank[i] = i == (zsl->level-1) ? 0 : rank[i+1];
        //如果当前层当前节点的下一个节点不为空且下一个节点的分值比要插入的节点小,如果分值相同使用sdscmp对字典序进行比较
        while (x->level[i].forward &&
                (x->level[i].forward->score < score ||
                    (x->level[i].forward->score == score &&
                    sdscmp(x->level[i].forward->ele,ele) < 0)))
        {
            //rank需要加上当前节点的跨度
            rank[i] += x->level[i].span;
            //向前走一步
            x = x->level[i].forward;
        }
        //update对应的是要插入位置的前一个节点
        update[i] = x;
    }
    //生成该节点的层数
    level = zslRandomLevel();
    //如果生成的高度比当前跳表的高度高
    if (level > zsl->level) {
        //从当前最高层的上一层开始,一直处理到到新生成的高度
        for (i = zsl->level; i < level; i++) {
            //因为要比其他所有节点的层都要高,所以当前层从当前位置到头节点之间没有其他节点,此时rank[i]的值为头节点到头节点的距离,所以是0.由于update数组代表插入位置的前一个节点,,所以update[i]为头节点且跨度为跳表长度
            rank[i] = 0;
            update[i] = zsl->header;
            update[i]->level[i].span = zsl->length;
        }
        //更新跳表高度
        zsl->level = level;
    }
    //将x指向新建的节点
    x = zslCreateNode(level,score,ele);
    //遍历当前所有的层
    for (i = 0; i < level; i++) {
        //更新要插入节点的每一层的前进指针为插入节点前一个节点的前进指针
        x->level[i].forward = update[i]->level[i].forward;
        //更新要插入节点的前一个节点每一层的前进指针指向新建节点
        update[i]->level[i].forward = x;
        
        //这两句更新跨度的抽出去在下面单独说
        x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }
    //如果新建的节点层级比前一个节点矮的话,那么需要把前一个节点高出部分层级的跨度都加一,因为相当于多跨了一个节点
    for (i = level; i < zsl->level; i++) {
        update[i]->level[i].span++;
    }
    
    x->backward = (update[0] == zsl->header) ? NULL : update[0];
    //如果插入节点最底层指针的前进指针不为空,说明当前节点不是被插入了跳表尾部
    if (x->level[0].forward)
        x->level[0].forward->backward = x;
    else
        //否则把跳表尾节点的指针更新为x
        zsl->tail = x;
    //插入完成后把跳表的长度加1
    zsl->length++;
    //返回被插入的节点
    return x;
}
```

查找节点：

```c
/* Find the rank for an element by both score and key.
 * Returns 0 when the element cannot be found, rank otherwise.
 * Note that the rank is 1-based due to the span of zsl->header to the
 * first element. */
unsigned long zslGetRank(zskiplist *zsl, double score, sds ele) {
    zskiplistNode *x;
    unsigned long rank = 0;
    int i;
    
    //把x指向头节点
    x = zsl->header;
    //从最高层开始遍历
    for (i = zsl->level-1; i >= 0; i--) {
        //如果下一个节点不为空且下一个节点分数比当前节点小或者分数相等但是小于等于下一个元素的字典序
        while (x->level[i].forward &&
            (x->level[i].forward->score < score ||
                (x->level[i].forward->score == score &&
                sdscmp(x->level[i].forward->ele,ele) <= 0))) {
            //利用span更新排名
            rank += x->level[i].span;
            //指向下一个同层元素
            x = x->level[i].forward;
        }

        /* x might be equal to zsl->header, so test if obj is non-NULL */
        //因为有可能跳表里没有元素,所以这里最后的x可能是头节点,于是这里判了一下空防止是头节点,并且元素相等的话就返回计算好的排名
        if (x->ele && sdscmp(x->ele,ele) == 0) {
            return rank;
        }
    }
    //能走到这的话说明元素不存在,返回0
    return 0;
}


/* Finds an element by its rank. The rank argument needs to be 1-based. */
zskiplistNode* zslGetElementByRank(zskiplist *zsl, unsigned long rank) {
    zskiplistNode *x;
    unsigned long traversed = 0;
    int i;

    x = zsl->header;
    for (i = zsl->level-1; i >= 0; i--) {
        while (x->level[i].forward && (traversed + x->level[i].span) <= rank)
        {
            traversed += x->level[i].span;
            x = x->level[i].forward;
        }
        if (traversed == rank) {
            return x;
        }
    }
    return NULL;
}
```

删除节点：

```c
int zslDelete(zskiplist *zsl, double score, sds ele, zskiplistNode **node) {
    //删除也需要记录前一个节点
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;
    
    //x指向头节点
    x = zsl->header;
    //从最高层开始遍历
    for (i = zsl->level-1; i >= 0; i--) {
        //循环方式和插入一样,直到找到要删除节点的前一个节点,然后用update存下来
        while (x->level[i].forward &&
                (x->level[i].forward->score < score ||
                    (x->level[i].forward->score == score &&
                     sdscmp(x->level[i].forward->ele,ele) < 0)))
        {
            x = x->level[i].forward;
        }
        update[i] = x;
    }
    
    //x现在指向的是最下面一层的update节点,这里往前走了一步也就是变成了插入节点的最下面一层
    x = x->level[0].forward;
    //如果插入节点最下面一层存在并且分数能对得上并且元素值也能对得上
    if (x && score == x->score && sdscmp(x->ele,ele) == 0) {
        //调用zslDeleteNode删除这个节点
        zslDeleteNode(zsl, x, update);
        if (!node)
            zslFreeNode(x);
        else
            *node = x;
        return 1;
    }
    return 0;
}

//实际删除节点的方法
void zslDeleteNode(zskiplist *zsl, zskiplistNode *x, zskiplistNode **update) {
    int i;
    //从最下面一层到最上面一层开始遍历
    for (i = 0; i < zsl->level; i++) {
        //如果要删除节点的前一个节点的前进指针是要删除节点的话
        if (update[i]->level[i].forward == x) {
            //前一个节点的span需要加上被删除节点的span再加1
            update[i]->level[i].span += x->level[i].span - 1;
            //把前一个节点的前进指针设置成要删除节点的前进指针
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            //走到else说明update的层高比插入节点高,且现在正在处理高出的那部分层,这时候直接把span减1即可
            update[i]->level[i].span -= 1;
        }
    }
    //如果被删除节点的前进指针存在的话
    if (x->level[0].forward) {
        //设置被删除节点的下一个节点的后退指针指向被删除节点的前一个节点
        x->level[0].forward->backward = x->backward;
    } else {
        //不存在的话说明被删除节点是尾节点,需要更新跳表的尾节点为被删除节点的前一个节点
        zsl->tail = x->backward;
    }
    //因为删除掉的节点可能是层高最高的节点,所以需要调整层高,这里的做法是从当最高层开始遍历,如果发现头节点之后直接是空,说明这一层是空,这时候需要把层高减1
    while(zsl->level > 1 && zsl->header->level[zsl->level-1].forward == NULL)
        zsl->level--;
    //跳表的长度减1
    zsl->length--;
}

// 删除跳表
void zslFree(zskiplist *zsl) {
    //获取头节点最下面一层的指针
    zskiplistNode *node = zsl->header->level[0].forward, *next;
    //释放头节点
    zfree(zsl->header);
    //从第一个节点开始,向后往后一个一个释放内存
    while(node) {
        next = node->level[0].forward;
        zslFreeNode(node);
        node = next;
    }
    zfree(zsl);
}
```

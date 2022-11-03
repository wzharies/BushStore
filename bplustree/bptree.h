/**
 * @file lbtree.h
 * @author  Shimin Chen <shimin.chen@gmail.com>, Jihang Liu, Leying Chen
 * @version 1.0
 *
 * @section LICENSE
 *
 * TBD
 *
 * @section DESCRIPTION
 *
 *
 * The class implements the LB+-Tree. 
 *
 * Non-leaf nodes are in DRAM.  They are normal B+-Tree nodes.
 * Leaf nodes are in NVM.
 */

#ifndef _LBTREE_H
#define _LBTREE_H

/* ---------------------------------------------------------------------- */

#include "tree.h"
#include "util/coding.h"
#include "leveldb/slice.h"
#include "leveldb/iterator.h"

namespace leveldb {
/* ---------------------------------------------------------------------- */

/* In a non-leaf, there are NON_LEAF_KEY_NUM keys and NON_LEAF_KEY_NUM+1
 * child pointers.
 */
#define NON_LEAF_KEY_NUM (NONLEAF_SIZE / (KEY_SIZE + POINTER_SIZE) - 1)

/* In a leaf, there are 16B header, 14x16B entries, 2x8B sibling pointers.
 */
#if LEAF_SIZE != 256
#error "LB+-Tree requires leaf node size to be 256B."
#endif

#define LEAF_KEY_NUM (40)
#define LEAF_VALUE_NUM (40)

// at most 1 of the following 2 macros may be defined
//#define NONTEMP
#define UNLOCK_AFTER

/* ---------------------------------------------------------------------- */
/**
 * Pointer8B defines a class that can be assigned to either bnode or bleaf.
 */
class Pointer8B
{
public:
    unsigned long long value; /* 8B to contain a pointer */

public:
    Pointer8B() {}

    Pointer8B(const void *ptr)
    {
        value = (unsigned long long)ptr;
    }

    Pointer8B(const Pointer8B &p)
    {
        value = p.value;
    }

    Pointer8B &operator=(const void *ptr)
    {
        value = (unsigned long long)ptr;
        return *this;
    }
    Pointer8B &operator=(const Pointer8B &p)
    {
        value = p.value;
        return *this;
    }

    bool operator==(const void *ptr)
    {
        bool result = (value == (unsigned long long)ptr);
        return result;
    }
    bool operator==(const Pointer8B &p)
    {
        bool result = (value == p.value);
        return result;
    }

    operator void *() { return (void *)value; }
    operator char *() { return (char *)value; }
    operator struct bnode *() { return (struct bnode *)value; }
    operator struct bleaf *() { return (struct bleaf *)value; }
    operator struct kPage *() { return (struct kPage *)value; }
    operator unsigned long long() { return value; }

    bool isNull(void) { return (value == 0); }

    void print(void) { printf("%llx\n", value); }

}; // Pointer8B

/**
 *  An IdxEntry consists of a key and a pointer.
 */
typedef struct IdxEntry
{
    key_type k;
    Pointer8B ch;
} IdxEntry;

/**
 *  bnodeMeta: the 8B meta data in Non-leaf node
 */
typedef struct bnodeMeta
{             /* 8B */
    int lock; /* lock bit for concurrency control */
    int num;  /* number of keys */
} bnodeMeta;

/**
 * bnode: non-leaf node
 *
 *   metadata (i.e. k(0))
 *
 *      k(1) .. k(NON_LEAF_KEY_NUM)
 *
 *   ch(0), ch(1) .. ch(NON_LEAF_KEY_NUM)
 */
class bnode
{
public:
    IdxEntry ent[NON_LEAF_KEY_NUM + 1];

public:
    //1-15
    key_type &k(int idx) { return ent[idx].k; }
    Pointer8B &ch(int idx) { return ent[idx].ch; }

    char *chEndAddr(int idx)
    {
        return (char *)&(ent[idx].ch) + sizeof(Pointer8B) - 1;
    }

    int &num(void) { return ((bnodeMeta *)&(ent[0].k))->num; }
    int &lock(void) { return ((bnodeMeta *)&(ent[0].k))->lock; }

    key_type &kBegin() { return ent[1].k; }
    key_type &kEnd() { return ent[num() - 1].k; }
    int search(key_type &key) {
        // TODO
        return 0;
    }

}; // bnode


class kPage{
public:
    uint64_t max_key;
    uint64_t bitmap : LEAF_KEY_NUM;
    uint64_t lock : 1;
    uint64_t alt : 1;
    uint64_t nums : 8;
    unsigned char finger[LEAF_KEY_NUM]; //指纹
    uint32_t pointer[LEAF_KEY_NUM]; //vPage地址，4k对齐，后12位不存储
    unsigned char index[LEAF_KEY_NUM]; //在vpage的第几个
    char keys[];
    leveldb::Slice k(size_t index){
        return leveldb::Slice(keys + index * 4 + 1, 2);
    }
    void setk(size_t index, leveldb::Slice key){
        keys[index * 3] = key.size();
        keys[index * 3 + 1] = key[0];
        keys[index * 3 + 2] = key[1];
    }
};

class vPage{
public:
    uint32_t total_num;
    uint32_t alloc_num;
    uint64_t bitmap;
    uint32_t offset[LEAF_VALUE_NUM];
    char kvs[];
    leveldb::Slice v(size_t index){
        char* start = (char *)(this + offset[index]);
        uint32_t v_len = leveldb::DecodeFixed32(start);
        return leveldb::Slice(start + 4, v_len);
    }
    void setv(size_t index, uint32_t off, leveldb::Slice value){
        memcpy(this + off, value.data(), value.size());
        offset[index] = off;
    }
};

/* ---------------------------------------------------------------------- */

class treeMeta
{
public:
    int root_level; // leaf: level 0, parent of leaf: level 1
    Pointer8B tree_root;
    bleaf **first_leaf; // on NVM
    int max_size;
    int cur_size;

    key_type compaction_key;
    key_type min_key; //L0compaction的时候需要
    key_type max_key;
    std::vector<void*> pages; //only on L0，记录最底层的page地址，方便merge
    void* addr; // ony on L0;方便直接delete

public:
    treeMeta(Pointer8B root, int level){
        tree_root = root;
        root_level = level;
    }
    treeMeta(Pointer8B root, int level, key_type min_key, key_type max_key, std::vector<void*> pages, void* addr)
        : tree_root(root), root_level(level), min_key(min_key), max_key(max_key), pages(pages) {};

    // treeMeta(void *nvm_address, int size, bool recover = false)
    // {
    //     root_level = 0;
    //     tree_root = NULL;
    //     first_leaf = (bleaf **)nvm_address;
    //     max_size = size;

    //     if (!recover)
    //         setFirstLeaf(NULL);
    // }

    // void setFirstLeaf(bleaf *leaf)
    // {
    //     *first_leaf = leaf;
    //     clwb(first_leaf);
    //     sfence();
    // }

}; // treeMeta

/* ---------------------------------------------------------------------- */

class lbtree : public tree
{
public: // root and level
    treeMeta *tree_meta;

public:
    lbtree(treeMeta *meta) : tree_meta(meta){};
    lbtree(Pointer8B tree_root, int level){
        tree_meta = new treeMeta(tree_root, level);
    }
    lbtree(void *nvm_address, bool recover = false)
    {
        tree_meta = new treeMeta(nvm_address, recover);
        if (!tree_meta)
        {
            perror("new");
            exit(1);
        }
    }

    ~lbtree()
    {
        delete tree_meta;
    }

// private:
//     int bulkloadSubtree(keyInput *input, int start_key, int num_key,
//                         float bfill, int target_level,
//                         Pointer8B pfirst[], int n_nodes[]);

//     int bulkloadToptree(Pointer8B ptrs[], key_type keys[], int num_key,
//                         float bfill, int cur_level, int target_level,
//                         Pointer8B pfirst[], int n_nodes[]);

//     void getMinMaxKey(bleaf *p, key_type &min_key, key_type &max_key);

//     void getKeyPtrLevel(Pointer8B pnode, int pnode_level, key_type left_key,
//                         int target_level, Pointer8B ptrs[], key_type keys[], int &num_nodes,
//                         bool free_above_level_nodes);

//     // sort pos[start] ... pos[end] (inclusively)
//     void qsortBleaf(bleaf *p, int start, int end, int pos[]);

public:
    int bulkload(int keynum, keyInput *input, float bfill);

    void randomize(Pointer8B pnode, int level);
    void randomize()
    {
        srand48(12345678);
        randomize(tree_meta->tree_root, tree_meta->root_level);
    }

    void *seek(key_type key, int *pos);

    void *lookup(key_type key, int *pos);

    // void *get_recptr(void *p, int pos)
    // {
    //     return ((bleaf *)p)->ch(pos);
    // }

    // insert (key, ptr)
    void insert(key_type key, void *ptr);

    // delete key
    void del(key_type key);

    // // Range scan -- Author: Lu Baotong
    // int range_scan_by_size(const key_type& key,  uint32_t to_scan, char* result);
    // int range_scan_in_one_leaf(bleaf *lp, const key_type& key, uint32_t to_scan, std::pair<key_type, void*>* result);
    // int add_to_sorted_result(std::pair<key_type, void*>* result, std::pair<key_type, void*>* new_record, int total_size, int cur_idx);

    // Range Scan -- Author: George He
    int rangeScan(key_type key,  uint32_t scan_size, char* result);
    bleaf* lockSibling(bleaf* lp);

    double load_factor(){
        return 1.0 * tree_meta->cur_size / tree_meta->max_size;
    }

    void buildTree(leveldb::Iterator* iter);
    std::vector<std::vector<void *>> pickInput(int page_count, int* index_start_pos, key_type* start, key_type* end);
    std::vector<std::vector<void *>> getOverlapping(key_type start, key_type end, int* index_start_pos, int* page_count, key_type* ret_start, key_type* ret_end);
    void rangeDelete(std::vector<std::vector<void*>> pages, key_type start, key_type end);
    void rangeReplace(std::vector<std::vector<void*>> pages, std::vector<std::vector<void*>> new_pages, key_type start, key_type end);

private:
    void print(Pointer8B pnode, int level);
    void check(Pointer8B pnode, int level, key_type &start, key_type &end, bleaf *&ptr);
    void checkFirstLeaf(void);

public:
    void print()
    {
        print(tree_meta->tree_root, tree_meta->root_level);
    }

    void check(key_type *start, key_type *end)
    {
        bleaf *ptr = NULL;
        check(tree_meta->tree_root, tree_meta->root_level, *start, *end, ptr);
        checkFirstLeaf();
    }

    int level() { return tree_meta->root_level; }

}; // lbtree
}

// void initUseful();

// #ifdef VAR_KEY
// static int vkcmp(char* a, char* b) {
// /*
//     auto n = key_size_;
//     while(n--)
//         if( *a != *b )
//             return *a - *b;
//         else
//             a++,b++;
//     return 0;
// */
//     return memcmp(a, b, key_size_);
// }
// #endif
/* ---------------------------------------------------------------------- */
#endif /* _LBTREE_H */

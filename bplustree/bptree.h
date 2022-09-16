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

#include <libpmem.h>
#include "tree.h"
#include "table/pm_mem_alloc.h"
#include "util/coding.h"
#include "util/global.h"
#include "leveldb/slice.h"
#include "leveldb/iterator.h"
#include "bplustree/persist.h"

namespace leveldb {
/* ---------------------------------------------------------------------- */

/* In a non-leaf, there are NON_LEAF_KEY_NUM keys and NON_LEAF_KEY_NUM+1
 * child pointers.
 */
#define NON_LEAF_KEY_NUM (NONLEAF_SIZE / (KEY_SIZE + POINTER_SIZE) - 1) //15

/* In a leaf, there are 16B header, 14x16B entries, 2x8B sibling pointers.
 */
#if LEAF_SIZE != 256
#error "LB+-Tree requires leaf node size to be 256B."
#endif

#define LEAF_KEY_NUM (30)
#define LEAF_VALUE_NUM (60)

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

// bnode
struct kPage{
    uint64_t next;
    // uint64_t bitmap : LEAF_KEY_NUM;
    // uint64_t lock : 1;
    // uint64_t alt : 1;
    uint64_t nums;
    uint16_t finger[LEAF_KEY_NUM];     uint16_t index[LEAF_KEY_NUM];     uint32_t pointer[LEAF_KEY_NUM];     char keys[];
    key_type rawK(size_t index){
        return DecodeDBBenchFixed64(keys + index * 16);
    }
    leveldb::Slice k(size_t index) const {
        return leveldb::Slice(keys + index * 16, 16);
    }
    void setk(size_t index, leveldb::Slice key){
        // keys[index * 3] = key.size();
        assert(key.size() == 16);
        memcpy(keys + index * 16, key.data(), 16);
    }
    kPage* nextPage(){
        return (kPage*)next;
    }
    void setNext(void* p){
        next = (uint64_t)p;
    }
    bool isEqual(size_t index, const key_type& key){
        return key == DecodeDBBenchFixed64(keys + index * 16);
    }
    key_type minRawKey(){
        assert(nums > 0);
        return rawK(0);
    }
    key_type maxRawKey(){
        assert(nums > 0);
        return rawK(nums - 1);
    }
    key_type findLargeThen(key_type &key){
        assert(key < maxRawKey());
        for(int i = 0;i < nums; i++){
            if(rawK(i) > key){
                return rawK(i);
            }
        }
        return maxRawKey();
    }
    void remove(int start, int end){
        if(start >= end) return ;
        for(int i = end; i < nums; i++){
            finger[i - end + start] = finger[i];
            pointer[i - end + start] = pointer[i];
            index[i - end + start] = index[i];
            setk(i - end + start, k(i));
        }
                nums = nums- (end - start);
    }
};

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
    Pointer8B &ch(int idx) { assert(idx != 0); return ent[idx].ch; }

    char *chEndAddr(int idx)
    {
        return (char *)&(ent[idx].ch) + sizeof(Pointer8B) - 1;
    }

    int &num(void) { return ((bnodeMeta *)&(ent[0].k))->num; }
    int &lock(void) { return ((bnodeMeta *)&(ent[0].k))->lock; }
    void setKandCh(int idx, key_type& k, Pointer8B& ch){
        ent[idx].k = k;
        ent[idx].ch = ch;
    }
    void remove(int start, int end){
        if(start >= end) return ;
        for(int i = end; i <= num(); i++){
            k(i - end + start) = k(i);
            ch(i - end + start) = ch(i);
        }
                num() = num() - (end - start);
    }

    void setkandCheck(int index, const key_type& key){
                // if(index + 1 < num()){
        //     assert(k(index + 1) >= key);
        // }
        // if(index - 1 >= 1){
        //     assert(k(index - 1) <= key);
        // }
        k(index) = key;
    }
    bool full(){
        return num() == NON_LEAF_KEY_NUM;
    }
    void insert(int index, const key_type& key, const Pointer8B& value){
        for(int i = num(); i >= index; i--){
            k(i + 1) = k(i);
            ch(i + 1) = ch(i); 
        }
        k(index) = key;
        ch(index) = value;
        num()++;
    }

    key_type &kBegin() { assert(num() > 0); return ent[1].k; }
    key_type &kEnd() { assert(num() > 0); return ent[num()].k; }
    key_type kRealEnd() { assert(num() > 0); return ((kPage*)ent[num()].ch)->maxRawKey(); }
    int search(key_type &key) {
        int first = 1;
        int last = num() + 1;
        int mid;
        while(first < last){
            mid = first + (last - first)/2;
            if(k(mid) < key)
                first = mid + 1;
            else
                last = mid;
        }
        return first;
    }
    
    void sort(){
        std::sort(ent + 1, ent + num() + 1, [](const IdxEntry& ent1 ,const IdxEntry& ent2){
            return ent1.k < ent2.k;
        });
    }

    bool check(){
        assert(num() <= NON_LEAF_KEY_NUM);
        for(int i = 1; i < num(); i++){
            if(k(i) > k(i + 1)){
                return false;
            }
        }
        return true;
    }

};
constexpr int ONE_META_ENTRY_NUM = (256 - 8) / 4; 
constexpr int VPAGE_START_INDEX = 4;
constexpr int VPAGE_KEY_SIZE = 16;
struct MetaEntry{
    uint64_t bitmap;
    uint32_t offset[ONE_META_ENTRY_NUM];
};
// offset 0-4 : nums(4B) + capacity(4B) + next(8B);
struct vPage{
    MetaEntry meta[0];
    uint32_t& capacity(){
        return meta[0].offset[1];
    }
    uint32_t& nums(){
        return meta[0].offset[0];
    }
    double getCapacityUsage(){
        return ((double)nums()) / capacity();
    }
    uint32_t& offset(size_t index){
        // assert(VPAGE_START_INDEX <= index && index < capacity());
        size_t entryIndex = index / ONE_META_ENTRY_NUM;
        size_t metaIndex = index % ONE_META_ENTRY_NUM;
        return meta[entryIndex].offset[metaIndex];
    }
    char* offset_addr(size_t index){
        size_t entryIndex = index / ONE_META_ENTRY_NUM;
        size_t metaIndex = index % ONE_META_ENTRY_NUM;
        return (char*)(&meta[entryIndex].offset[metaIndex]);
    }
    void setNext(void* next){
        *(uint64_t*)(meta[0].offset + 2) = (uint64_t)next;
    }
    void* next(){
        return (void*)(*(uint64_t*)(meta[0].offset + 2));
    }
    // bool isFull(){
    //     return false;
    // }
    char* getValueAddr(size_t index){
        assert(VPAGE_START_INDEX <= index);
        // assert(VPAGE_START_INDEX <= index && index < capacity());
        size_t entryIndex = index / ONE_META_ENTRY_NUM;
        size_t metaIndex = index % ONE_META_ENTRY_NUM;
        return (char*)((char*)this + meta[entryIndex].offset[metaIndex]);
    }
    leveldb::Slice v(size_t index){
        char* start = getValueAddr(index);
        uint32_t v_len = leveldb::DecodeFixed32(start + VPAGE_KEY_SIZE);
        // assert(v_len == 1000);
        return leveldb::Slice(start + VPAGE_KEY_SIZE + 4, v_len);
    }
    bool isFull(int off, int index){
        if(256 * (index / 62 + 1) > off){
            return true;
        }
        return false;
    }
    uint32_t setkv(size_t index, uint32_t off, const leveldb::Slice& key, const leveldb::Slice& value, bool flush){
        assert(key.size() == 16);
        assert(off > VPAGE_KEY_SIZE + 4 + value.size());
        off -= (VPAGE_KEY_SIZE + 4 + value.size());
        memcpy((char*)this + off, key.data(), key.size());
        leveldb::EncodeFixed32((char*)this + off + VPAGE_KEY_SIZE, value.size());
        memcpy((char*)this + off + VPAGE_KEY_SIZE + 4, value.data(), value.size());
        offset(index) = off;
        setBitMap(index);
        return off;
    }
    void clrBitMap(int index){
        assert(VPAGE_START_INDEX <= index && index < capacity());
        assert(getBitMap(index));
        assert(nums() > 0);
        nums()--;
        size_t entryIndex = index / ONE_META_ENTRY_NUM;
        size_t metaIndex = index % ONE_META_ENTRY_NUM;
        meta[entryIndex].bitmap &= (~(1ULL << metaIndex));
    }
    void setBitMap(int index){
        // assert(VPAGE_START_INDEX <= index && index < capacity());
        // assert(!getBitMap(index));
        nums()++;
        size_t entryIndex = index / ONE_META_ENTRY_NUM;
        size_t metaIndex = index % ONE_META_ENTRY_NUM;
        meta[entryIndex].bitmap |= (1ULL << metaIndex);
    }
    char* metadata(int index){
        size_t entryIndex = index / ONE_META_ENTRY_NUM;
        return (char*)(&meta[entryIndex]);
    }

    bool getBitMap(int index){
        // assert(VPAGE_START_INDEX <= index && index < capacity());
        size_t entryIndex = index / ONE_META_ENTRY_NUM;
        size_t metaIndex = index % ONE_META_ENTRY_NUM;
        return (meta[entryIndex].bitmap >> metaIndex) & 1;
    }
};

// class vPage{
// public:
//     // 4 + 4 + 8 + 4 * n = 256
//     uint32_t total_num;
//     uint32_t alloc_num;
//     uint64_t bitmap;
//     uint32_t offset[LEAF_VALUE_NUM];
//     char kvs[]; //4B size + value;
//     char* getValueAddr(size_t index){
//         assert(index < alloc_num);
//         return (char *)((char*)this + offset[index]);
//     }
//     leveldb::Slice v(size_t index){
//         char* start = (char *)((char*)this + offset[index]);
//         uint32_t v_len = leveldb::DecodeFixed32(start);
//         return leveldb::Slice(start + 4, v_len);
//     }
//     uint32_t setv(size_t index, uint32_t off, leveldb::Slice value){
//         leveldb::EncodeFixed32((char*)this + off, value.size());
//         memcpy((char*)this + 4 + off, value.data(), value.size());
//         offset[index] = off;
//         return off + 4 + value.size();
//     }
//     void clrBitMap(int index){
//         assert(index < alloc_num);
//         assert(getBitMap(index));
//         bitmap = bitmap & (~(1ULL << index));
//     }

//     bool getBitMap(int index){
//         assert(index < alloc_num);
//         return (bitmap >> index) & 1;
//     }
// };

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
    key_type min_key;     key_type max_key;
    std::vector<void*> pages;     void* addr = nullptr;     int kPage_count; 
public:
    treeMeta(Pointer8B root, int level){
        tree_root = root;
        root_level = level;
    }
    treeMeta(Pointer8B root, int level, key_type min_key, key_type max_key, std::vector<void*> pages, void* addr, int kPage_count = 0)
        : tree_root(root), root_level(level), min_key(min_key), max_key(max_key), pages(pages), addr(addr), kPage_count(kPage_count) {};

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
    uint32_t fileNumber = 0;
    std::vector<std::vector<void*>> needFreeNodePages;
    std::vector<char*> needFreeKPages;
    std::vector<char*> needFreeVPages;
    PMMemAllocator *alloc;
    // std::atomic<int> reader_count = 0;

public:
    lbtree(treeMeta *meta, PMMemAllocator *alloc) : tree_meta(meta), alloc(alloc){};
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
        if (tree_meta->addr != nullptr) {
            assert(needFreeNodePages.size() == 0);
            if(!TEST_BPTREE_NVM){
                free(tree_meta->addr);
            }
        } else {
            for (int i = 1; i < needFreeNodePages.size(); i++) {
                for (int j = 0; j < needFreeNodePages[i].size(); j++) {
                  free(needFreeNodePages[i][j]);
                }
            }
        }
        for (auto& kpage : needFreeKPages) {
            alloc->freePage(kpage, key_t);
        }
        for (auto& vpage : needFreeVPages) {
            alloc->freePage(vpage, value_t);
        }
        delete tree_meta;
    }

    bool isInRange(const key_type& key){
        if(tree_meta->min_key <= key && key <= tree_meta->max_key){
            return true;
        }
        return false;
    }

    void reverseAndCheck();
    void checkIterator();

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
    // int bulkload(int keynum, keyInput *input, float bfill);

    // void randomize(Pointer8B pnode, int level);
    // void randomize()
    // {
    //     srand48(12345678);
    //     randomize(tree_meta->tree_root, tree_meta->root_level);
    // }

    // void *seek(key_type key, int *pos);

    // void *lookup(key_type key, int *pos);

    // // void *get_recptr(void *p, int pos)
    // // {
    // //     return ((bleaf *)p)->ch(pos);
    // // }

    // // insert (key, ptr)
    // void insert(key_type key, void *ptr);

    // // delete key
    // void del(key_type key);

    // // // Range scan -- Author: Lu Baotong
    // // int range_scan_by_size(const key_type& key,  uint32_t to_scan, char* result);
    // // int range_scan_in_one_leaf(bleaf *lp, const key_type& key, uint32_t to_scan, std::pair<key_type, void*>* result);
    // // int add_to_sorted_result(std::pair<key_type, void*>* result, std::pair<key_type, void*>* new_record, int total_size, int cur_idx);

    // // Range Scan -- Author: George He
    // int rangeScan(key_type key,  uint32_t scan_size, char* result);
    // bleaf* lockSibling(bleaf* lp);

    double load_factor(){
        return 1.0 * tree_meta->cur_size / tree_meta->max_size;
    }
    void* lookup(key_type key, int *pos);
    void buildTree(leveldb::Iterator* iter);
    kPage* getKpage(key_type key, bool begin);
    std::vector<std::vector<void *>> pickInput(int page_count, int* index_start_pos, key_type* start, key_type* end);
    std::vector<std::vector<void *>> getOverlapping(key_type start, key_type end, int* index_start_pos, int* page_count, key_type* ret_start, key_type* ret_end, kPage*& kBegin, kPage*& kEnd);
    // std::vector<std::vector<void *>> getOverlappingMulTask(std::vector<key_type> starts, key_type* begin, key_type* end, std::vector<int>& page_indexs, std::vector<int>& entry_indexs, std::vector<int>& page_counts, std::vector<int>& sst_index, key_type& ret_start, key_type& ret_end, key_type& new_begin);
    std::vector<std::vector<void *>> getOverlappingMulTask(key_type start_key, key_type end_key, int sst_count, key_type sst_start, key_type sst_end, key_type &new_start_key, int &sst_page_index, int &sst_page_end_index, kPage*& kBegin, kPage*& kEnd);
    void rangeDelete(std::vector<std::vector<void*>>& pages, key_type start, key_type end);
    void rangeReplace(std::vector<std::vector<void*>>& pages, std::vector<std::vector<void*>>& new_pages, key_type start, key_type end);

// private:
//     void print(Pointer8B pnode, int level);
//     void check(Pointer8B pnode, int level, key_type &start, key_type &end, bleaf *&ptr);
//     void checkFirstLeaf(void);

// public:
//     void print()
//     {
//         print(tree_meta->tree_root, tree_meta->root_level);
//     }

//     void check(key_type *start, key_type *end)
//     {
//         bleaf *ptr = NULL;
//         check(tree_meta->tree_root, tree_meta->root_level, *start, *end, ptr);
//         checkFirstLeaf();
//     }

//     int level() { return tree_meta->root_level; }

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

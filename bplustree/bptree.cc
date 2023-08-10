/**
 * @file indirect-arronly.cc
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
 * The class implements a Btree with indirect arrays only.  Each node contains
 * an indirect array.  The design aims to have good search performance and
 * efficient solution for update consistency on NVM memory.  However, the node
 * size is limited to up to 128B.
 */

#include <cstdlib>
#include "bptree.h"
#include "include/leveldb/options.h"
#include "table/pm_mem_alloc.h"
#include "util/global.h"
namespace leveldb {

void lbtree::buildTree(Iterator* iter){
    
}

std::tuple<key_type, key_type> reverseAndCheckNode(int level, bnode *p){
    key_type minV = UINT64_MAX;
    key_type maxV = 0;
    if(level == 1){
        kPage *next;
        assert(p->check());
        key_type preleft = -1;
        key_type preright = -1;
        for(int i = 1; i <= p->num(); i++){
            next = p->ch(i);
            key_type left = next->minRawKey();
            key_type right = next->maxRawKey();
            assert(p->k(i) <= left);
            if(preleft != -1){
                assert(preright <= left);
            }
            minV = std::min(minV, left);
            maxV = std::max(maxV, right);
            preleft = left; 
            preright = right;
        }
        assert(minV <= maxV);
        return std::make_tuple(minV, maxV);
    }else{
        bnode *next;
        assert(p->check());
        key_type preleft = -1;
        key_type preright = -1;
        for(int i = 1; i <= p->num(); i++){
            next = p->ch(i);
            auto [left, right] = reverseAndCheckNode(level - 1, next);
            if(preright != -1){
                assert(preright <= left);
            }
            minV = std::min(minV, left);
            maxV = std::max(maxV, right);
            preleft = left; 
            preright = right;
        }
        assert(minV <= maxV);
        return std::make_tuple(minV, maxV);
    }
}

void lbtree::reverseAndCheck() {
    bnode *p = tree_meta->tree_root;
    bnode *next;
    assert(p->check());
    key_type preleft = -1;
    key_type preright = -1;
    key_type minK = UINT64_MAX;
    key_type maxK = 0;
    for(int i = 1; i <= p->num(); i++){
        next = p->ch(i);
        auto [left, right] = reverseAndCheckNode(tree_meta->root_level - 1, next);
        if(preleft != -1){
            assert(preright <= left);
        }
        preleft = left;
        preright = right;
        minK = std::min(minK, preleft);
        maxK = std::max(maxK, preright);
    }
    printf("reverseAndCheck start :%lu, end :%lu\n", minK, maxK);
}

void lbtree::checkIterator(){
    int pageCount = 0;
    kPage* page = getKpage(tree_meta->min_key, true);
    assert(tree_meta->min_key <= page->minRawKey());
    printf("check star : %lu", page->minRawKey());
    pageCount++;
    kPage* nextKpage;
    while(page->nextPage() != nullptr){
        nextKpage = page->nextPage();
        assert(page->maxRawKey() <= nextKpage->minRawKey());
        page = nextKpage;
        pageCount++;
    }
    assert(page->maxRawKey() <= tree_meta->max_key);
    printf(" page count :%d, end star : %lu\n", pageCount, page->maxRawKey());
}

std::vector<std::vector<void *>> lbtree::pickInput(int page_count, int* index_start_pos, key_type* start, key_type* end)
{
    // record the path from root to leaf
    // parray[level] is a node on the path
    // child ppos[level] of parray[level] == parray[level-1]
    //
    Pointer8B parray[32]; // 0 .. root_level will be used
    short ppos[32];       // 1 .. root_level will be used
    int pnum[32];      // 0 .. root_level will be used
    key_type key = tree_meta->compaction_key;
    volatile long long sum;
    std::vector<std::vector<void *>> page_addr(tree_meta->root_level + 1);
    /* Part 1. get the positions for the compaction key */
    {
        bnode *p;
        //bleaf *lp;
        int i, t, m, b;
    #ifdef VAR_KEY
        int c;
    #endif

    Again2:
        // 1. RTM begin
        // if (_xbegin() != _XBEGIN_STARTED)
        // {
        //     // random backoff
        //     sum= 0;
        //     for (int i=(rdtsc() % 1024); i>0; i--) sum += i;
        //     goto Again2;
        // }

        // 2. search nonleaf nodes
        p = tree_meta->tree_root;

        for (i = tree_meta->root_level; i > 0; i--)
        {
        #ifdef PREFETCH
            // prefetch the entire node
            NODE_PREF(p);
        #endif

            // if the lock bit is set, abort
            if (p->lock())
            {
                // _xabort(3);
                goto Again2;
            }

            parray[i] = p;
            pnum[i] = p->num();
            page_addr[i].push_back((void*)p);

            // binary search to narrow down to at most 8 entries
            b = 1;
            t = p->num();
            while (b + 7 <= t)
            {
                m = (b + t) >> 1;
            #ifdef VAR_KEY
                c = vkcmp((char*)p->k(m), (char*)key);
                if (c > 0)
                    b = m + 1;
                else if (c < 0)
                    t = m - 1;
            #else
                if (key > p->k(m))
                    b = m + 1;
                else if (key < p->k(m))
                    t = m - 1;
            #endif
                else
                {
                    p = p->ch(m);
                    ppos[i] = m;
                    goto inner_done;
                }
            }

            // sequential search (which is slightly faster now)
            for (; b <= t; b++)
            #ifdef VAR_KEY
                if (vkcmp((char*)key, (char*)p->k(b)) > 0)
                    break;
            #else
                if (key < p->k(b))
                    break;
            #endif
            p = p->ch(b - 1);
            ppos[i] = b - 1;

        inner_done:;
        }
        *start = ((bnode *)parray[1])->k(ppos[1]);
        *index_start_pos = ppos[1];
        int start_pos = ppos[1];
        // return the result;
        while(page_count > 0){
            page_addr[1].push_back((void *)parray[1]);
                        if(page_count <= pnum[1] - start_pos + 1){
                short end_pos = start_pos + page_count - 1;
                kPage* page = (kPage*)((bnode *)parray[1])->ch(end_pos);
                *end = page->maxRawKey();
                break;
                //goto inner_done2;
            }else{
                short end_pos = pnum[1];
                kPage* page = (kPage*)((bnode *)parray[1])->ch(end_pos);
                *end = page->maxRawKey();
            }
            page_count -= (pnum[1] - start_pos + 1);
            start_pos = 1;

            int level;
                        for(level = 2; level < tree_meta->root_level; level++){
                                if(ppos[level] < pnum[level]){
                    ppos[level]++;
                                        while(level >= 2){
                        parray[level - 1] = ((bnode *)parray[level])->ch(ppos[level]);
                        page_addr[level - 1].push_back((void *)parray[level - 11]);
                        ppos[level - 1] = 1;
                        pnum[level - 1] = ((bnode *)parray[level - 1])->num();
                        level--;
                    }
                    break;
                }
            }
            if(level == tree_meta->root_level){
                break;
            }
        }
    //inner_done2:;

    }
    
    // 4. RTM commit
    // _xend();

    return page_addr;
}


kPage* lbtree::getKpage(key_type key, bool begin){
    if(tree_meta->min_key > key || key > tree_meta->max_key){
        return nullptr;
    }
    bnode *p;
    kPage *kp = nullptr;
    int i, t, m, b;
#ifdef VAR_KEY
    int c;
#endif

    uint16_t key_hash = hashcode2B(key);
    int ret_pos;

Again1:
    // 1. RTM begin
    // if (_xbegin() != _XBEGIN_STARTED)
    //     goto Again1;

    // 2. search nonleaf nodes
    p = tree_meta->tree_root;

    for (i = tree_meta->root_level; i > 0; i--)
    {
    #ifdef PREFETCH
        // prefetch the entire node
        NODE_PREF(p);
    #endif
        assert(p->lock() == 0 || p->lock() == 1);
        // if the lock bit is set, abort
        if (p->lock())
        {
            // _xabort(1);
            goto Again1;
        }

        // binary search to narrow down to at most 8 entries
        b = 1;
        t = p->num();
        while (b + 7 <= t)
        {
            m = (b + t) >> 1;
        #ifdef VAR_KEY
            c = vkcmp((char*)p->k(m), (char*)key);
            if (c > 0)
                b = m + 1;
            else if (c < 0)
                t = m - 1;
        #else
            if (key > p->k(m))
                b = m + 1;
            else if (key < p->k(m))
                t = m - 1;
        #endif
            else
            {
                p = p->ch(m);
                goto inner_done;
            }
        }

        // sequential search (which is slightly faster now)
        for (; b <= t; b++)
        #ifdef VAR_KEY
            if (vkcmp((char*)key, (char*)p->k(b)) > 0)
                break;
        #else
            if (key < p->k(b))
                break;
        #endif
        if(b == 1) b++;
        p = p->ch(b - 1);

    inner_done:;
    }

    // 3. search leaf node
    kp = (kPage *)p;
    if(!begin && kp->maxRawKey() < key){
        kp = kp->nextPage();
    }
    return kp;
}

std::vector<std::vector<void*>> lbtree::getOverlappingMulTask(
    key_type start_key, key_type end_key, int sst_count, key_type sst_start,
    key_type sst_end, key_type& new_start_key, int& sst_page_index,
    int& sst_page_end_index, kPage*& kBegin, kPage*& kEnd) {
    // record the path from root to leaf
    // parray[level] is a node on the path
    // child ppos[level] of parray[level] == parray[level-1]
    //
    Pointer8B parray[32];  // 0 .. root_level will be used
    short ppos[32];        // 1 .. root_level will be used
    int pnum[32];          // 0 .. root_level will be used
    key_type key = start_key;
    volatile long long sum;
    std::vector<std::vector<void*>> page_addr(tree_meta->root_level + 1);
    // std::vector<std::vector<bnode *>> bnodes(tree_meta->root_level + 1);
    /* Part 1. get the positions for the compaction key */
    {
        bnode* p;
        // bleaf *lp;
        int i, t, m, b;
#ifdef VAR_KEY
        int c;
#endif

    Again2:
        // 1. RTM begin
        // if (_xbegin() != _XBEGIN_STARTED)
        // {
        //     // random backoff
        //     sum= 0;
        //     for (int i=(rdtsc() % 1024); i>0; i--) sum += i;
        //     goto Again2;
        // }

        // 2. search nonleaf nodes
        p = tree_meta->tree_root;

        for (i = tree_meta->root_level; i > 0; i--) {
#ifdef PREFETCH
            // prefetch the entire node
            NODE_PREF(p);
#endif
            assert(p->lock() == 0 || p->lock() == 1);
            // if the lock bit is set, abort
            if (p->lock()) {
                // _xabort(3);
                goto Again2;
            }

            parray[i] = p;
            // if(i > 1){
            //     page_addr[i].push_back((void*)p);
            // }
            pnum[i] = p->num();

            // binary search to narrow down to at most 8 entries
            b = 1;
            t = p->num();
            while (b + 7 <= t) {
                m = (b + t) >> 1;
#ifdef VAR_KEY
                c = vkcmp((char*)p->k(m), (char*)key);
                if (c > 0)
                    b = m + 1;
                else if (c < 0)
                    t = m - 1;
#else
                if (key > p->k(m))
                    b = m + 1;
                else if (key < p->k(m))
                    t = m - 1;
#endif
                else {
                    p = p->ch(m);
                    ppos[i] = m;
                    goto inner_done1;
                }
            }

            // sequential search (which is slightly faster now)
            for (; b <= t; b++)
#ifdef VAR_KEY
                if (vkcmp((char*)key, (char*)p->k(b)) > 0) break;
#else
                if (key < p->k(b)) break;
#endif
            if (b == 1) b++;
            p = p->ch(b - 1);
            ppos[i] = b - 1;

        inner_done1:;
        }
        for (i = tree_meta->root_level; i > 0; i--) {
            page_addr[i].push_back((void*)parray[i]);
            // bnodes[i].push_back((bnode *)parray[i]);
        }
        kBegin = nullptr;
        kEnd = nullptr;
        
        if(start_key <= tree_meta->min_key){
            kBegin = nullptr;
                }else if(start_key == ((bnode *)parray[1])->k(ppos[1])){
            if(ppos[1] > 1){
                kBegin = ((bnode *)parray[1])->ch(ppos[1] - 1);
            }else{
                assert(start_key - 1 < start_key);
                kBegin = getKpage(start_key - 1, true);
                assert(kBegin != nullptr);
                assert(kBegin->nextPage() == ((bnode *)parray[1])->ch(ppos[1]));
            }
        }else if(start_key > ((bnode *)parray[1])->k(ppos[1])){
            kBegin = ((bnode *)parray[1])->ch(ppos[1]);
        }else{
            assert(false);
        }

        int need_task_count = TASK_COUNT - sst_count;
        int need_bnode_count = need_task_count * MAX_BNODE_NUM;
        sst_page_index = -1;
        sst_page_end_index = -1;

        // *ret_start = ((bnode *)parray[1])->k(ppos[1]);
        int cur_bnode_count = 0;
        int task_bnode_count = 0;
        bool write_seq = sst_count == 0 ? true : false;
        // int start_pos = ppos[1];
        //  return the result;
        /*
        we have two end，one is end_key，other is sst_end.
        if end_key > sst_end. we should record sst_end_index.
        if sst_end > end_key. sst_end_index = -1.
        if sst_end_index != -1, we read more bnode.
        */
        // assert(start_key <= sst_start);
        // assert(sst_start <= sst_end);
        while (true) {
            // 1. current bnode. bnode already in page_addr.
            key_type& begin = ((bnode*)parray[1])->kBegin();
            key_type& end = ((bnode*)parray[1])->kEnd();
            if (!write_seq && begin <= sst_start) {
                sst_page_index = cur_bnode_count;
            }
            if (!write_seq && begin <= sst_end) {
                sst_page_end_index = cur_bnode_count;
            }
            if (write_seq || sst_end <= begin) {
                task_bnode_count++;
                if (task_bnode_count >= need_bnode_count) {
                    // finish for bnode count.
                    new_start_key = ((bnode*)parray[1])->kRealEnd() + 1;
                    break;
                }
            }
            cur_bnode_count++;

            // 2. get next bnode
            int level;
            for (level = 2; level <= tree_meta->root_level; level++) {
                if (ppos[level] < pnum[level]) {
                    ppos[level]++;
                    while (level >= 2) {
                        parray[level - 1] =
                            ((bnode*)parray[level])->ch(ppos[level]);
                        // end_key may be the next sst_start. we can't exced
                        // them.
                        if (((bnode*)parray[level - 1])->kBegin() >= end_key) {
                          // finishe for end_key.
                          new_start_key = end_key;
                          goto inner_done2;
                        }
                        page_addr[level - 1].push_back(
                            (void*)parray[level - 1]);
                        // bnodes[level - 1].push_back((bnode *)parray[level -
                        // 1]);
                        ppos[level - 1] = 1;
                        pnum[level - 1] = ((bnode*)parray[level - 1])->num();
                        level--;
                    }
                    break;
                }
            }
            if (level == tree_meta->root_level + 1) {
                new_start_key = tree_meta->max_key + 1;
                break;
            }
        }
    inner_done2:;
        if(new_start_key > tree_meta->max_key){
            kEnd = nullptr;
        }else{
            // assert(new_start_key != -1);
            kEnd = getKpage(new_start_key, false);
        }
    }

    // 4. RTM commit
    // _xend();

    return page_addr;
}

// std::vector<std::vector<void*>> lbtree::getOverlappingMulTask(
//     std::vector<key_type> starts, key_type* begin, key_type* end,
//     std::vector<int>& page_indexs, std::vector<int>& entry_indexs,
//     std::vector<int>& page_counts, std::vector<int>& sst_index,
//     key_type& ret_start, key_type& ret_end, key_type& new_begin) {
//     // record the path from root to leaf
//     // parray[level] is a node on the path
//     // child ppos[level] of parray[level] == parray[level-1]
//     //
//     Pointer8B parray[32]; // 0 .. root_level will be used
//     short ppos[32];       // 1 .. root_level will be used
//     int pnum[32];      // 0 .. root_level will be used
//     key_type key = *begin;
//     volatile long long sum;
//     std::vector<std::vector<void *>> page_addr(tree_meta->root_level + 1);
//     // std::vector<std::vector<bnode *>> bnodes(tree_meta->root_level + 1);
//     /* Part 1. get the positions for the compaction key */
//     {
//         bnode *p;
//         //bleaf *lp;
//         int i, t, m, b;
//     #ifdef VAR_KEY
//         int c;
//     #endif

//     Again2:
//         // 1. RTM begin
//         // if (_xbegin() != _XBEGIN_STARTED)
//         // {
//         //     // random backoff
//         //     sum= 0;
//         //     for (int i=(rdtsc() % 1024); i>0; i--) sum += i;
//         //     goto Again2;
//         // }

//         // 2. search nonleaf nodes
//         p = tree_meta->tree_root;

//         for (i = tree_meta->root_level; i > 0; i--)
//         {
//         #ifdef PREFETCH
//             // prefetch the entire node
//             NODE_PREF(p);
//         #endif
//             assert(p->lock() == 0 || p->lock() == 1);
//             // if the lock bit is set, abort
//             if (p->lock())
//             {
//                 // _xabort(3);
//                 goto Again2;
//             }

//             parray[i] = p;
//             // if(i > 1){
//             //     page_addr[i].push_back((void*)p);
//             // }
//             pnum[i] = p->num();

//             // binary search to narrow down to at most 8 entries
//             b = 1;
//             t = p->num();
//             while (b + 7 <= t)
//             {
//                 m = (b + t) >> 1;
//             #ifdef VAR_KEY
//                 c = vkcmp((char*)p->k(m), (char*)key);
//                 if (c > 0)
//                     b = m + 1;
//                 else if (c < 0)
//                     t = m - 1;
//             #else
//                 if (key > p->k(m))
//                     b = m + 1;
//                 else if (key < p->k(m))
//                     t = m - 1;
//             #endif
//                 else
//                 {
//                     p = p->ch(m);
//                     ppos[i] = m;
//                     goto inner_done1;
//                 }
//             }

//             // sequential search (which is slightly faster now)
//             for (; b <= t; b++)
//             #ifdef VAR_KEY
//                 if (vkcmp((char*)key, (char*)p->k(b)) > 0)
//                     break;
//             #else
//                 if (key < p->k(b))
//                     break;
//             #endif
//             if(b == 1) b++;
//             p = p->ch(b - 1);
//             ppos[i] = b - 1;

//         inner_done1:;
//         }
//         for (i = tree_meta->root_level; i > 0; i--) {
//             page_addr[i].push_back((void *)parray[i]);
//             // bnodes[i].push_back((bnode *)parray[i]);
//         }
//         const int maxBnodeCount = 50;
//         const int maxTaskCount = 8;
//         int page_index = 0;
//         int cur_kpage_count = 0;
//         int cur_bnode_count = 0;
//         int cur_page_index = 0;
//         int task_count = 0;
//         key_type cur_end = 0;
//         int cur_end_index = 0;
//         bool inOrOutBnodeRange = false;
//         int cur_sst_index = 0;
//         bool isSeqWrite = false;
//         if(starts.empty()){
//             isSeqWrite = true;
//         }else{
//             cur_end = starts.front();
//             cur_end_index++;
//         }
//         ret_start = ((bnode *)parray[1])->k(ppos[1]);

//         page_indexs.push_back(0);
//         entry_indexs.push_back(ppos[1]);
//         sst_index.push_back(-1);
//         //int start_pos = ppos[1];
//         // return the result;
//         auto divideSeqWrite = [&](){
//             //             if (*end > ((bnode*)parray[1])->k(pnum[1]) || task_count < maxTaskCount) {
//                 //                 if (cur_bnode_count >= maxBnodeCount) {
//                     //                     page_indexs.push_back(cur_page_index);
//                     entry_indexs.push_back(1);
//                     page_counts.push_back(cur_kpage_count);
//                     sst_index.push_back(-1);
//                     //                     cur_kpage_count = 0;
//                     cur_bnode_count = 0;

//                     task_count++;
//                 }
//                 //                 cur_kpage_count += (pnum[1] - ppos[1] + 1);
//                 cur_bnode_count++;
//                 ret_end = ((bnode*)parray[1])->k(pnum[1]);
//             //             }else{
//                 // page_counts.push_back(cur_kpage_count);
//                 // new_begin = 
//             }
//         };
//         while(true){
//             if(isSeqWrite){
//                 divideSeqWrite();
//             }

//             //             if (cur_end > ((bnode*)parray[1])->k(pnum[1])) {
//                 //                 if (inOrOutBnodeRange && cur_bnode_count >= maxBnodeCount) {
//                     //                     page_indexs.push_back(cur_page_index);
//                     entry_indexs.push_back(1);
//                     page_counts.push_back(cur_kpage_count);
//                     sst_index.push_back(-1);
//                     //                     cur_kpage_count = 0;
//                     cur_bnode_count = 0;
//                 }
//                 //                 cur_kpage_count += (pnum[1] - ppos[1] + 1);
//                 cur_bnode_count++;
//                 ret_end = ((bnode*)parray[1])->k(pnum[1]);
//                 //             } else {
//                 //                 if (inOrOutBnodeRange) {
//                     inOrOutBnodeRange = false;
//                     sst_index.back() = cur_sst_index++;

//                     cur_kpage_count += (pnum[1] - ppos[1] + 1);
//                     cur_bnode_count++;
//                     ret_end = ((bnode*)parray[1])->k(pnum[1]);
                    
//                     if(cur_end_index >= starts.size())
//                         // TODO is end.
//                         break;
//                     else
//                         cur_end = starts[cur_end_index++];
//                 } else {
//                     for (short start_pos = ppos[1]; start_pos <= pnum[1];
//                          start_pos++) {
//                         // for(short start_pos = pnum[1] - 1; start_pos >=
//                         // ppos[1]; start_pos--){
//                         //                         if (cur_end <= ((bnode*)parray[1])->k(start_pos)) {
//                           //                           if (start_pos > 1) {
//                             //                             cur_kpage_count += (start_pos - ppos[1]);
//                             cur_bnode_count++;
//                             ret_end = ((bnode*)parray[1])->k(start_pos - 1);
//                             //                             page_indexs.push_back(cur_page_index);
//                             entry_indexs.push_back(start_pos);
//                             page_counts.push_back(cur_kpage_count);
//                             //                             cur_kpage_count = 0;
//                             cur_bnode_count = 0;
//                             //                             cur_kpage_count += (pnum[1] - start_pos + 1);
//                             cur_bnode_count++;
//                             ret_end = ((bnode*)parray[1])->k(pnum[1]);

//                             //                           } else {
//                             //                             page_indexs.push_back(cur_page_index);
//                             entry_indexs.push_back(1);
//                             page_counts.push_back(cur_kpage_count);
//                             //                             cur_kpage_count = 0;
//                             cur_bnode_count = 0;
//                             //                             cur_kpage_count += (pnum[1] - ppos[1] + 1);
//                             cur_bnode_count++;
//                             ret_end = ((bnode*)parray[1])->k(pnum[1]);
//                           }
//                           goto inner_done2;
//                         }
//                     }
//                 }
//             }
//             cur_page_index++;

//             int level;
//             for(level = 2; level <= tree_meta->root_level; level++){
//                 if(ppos[level] < pnum[level]){
//                     ppos[level]++;
//                     while(level >= 2){
//                         parray[level - 1] = ((bnode *)parray[level])->ch(ppos[level]);
//                         if(((bnode *)parray[level - 1])->kBegin() > *end){
//                             goto inner_done2;
//                         }
//                         page_addr[level - 1].push_back((void *)parray[level - 1]);
//                         // bnodes[level - 1].push_back((bnode *)parray[level - 1]);
//                         ppos[level - 1] = 1;
//                         pnum[level - 1] = ((bnode *)parray[level - 1])->num();
//                         level--;
//                     }
//                     break;
//                 }
//             }
//             if(level == tree_meta->root_level + 1){
//                 break;
//             }
//         }
//     inner_done2:;
//     }
    
//     // 4. RTM commit
//     // _xend();

//     return page_addr;
// }

std::vector<std::vector<void *>> lbtree::getOverlapping(key_type start, key_type end, int* index_start_pos, int* page_count, key_type* ret_start, key_type* ret_end, kPage*& kBegin, kPage*& kEnd){
    // record the path from root to leaf
    // parray[level] is a node on the path
    // child ppos[level] of parray[level] == parray[level-1]
    //
    Pointer8B parray[32]; // 0 .. root_level will be used
    short ppos[32];       // 1 .. root_level will be used
    int pnum[32];      // 0 .. root_level will be used
    key_type key = start;
    volatile long long sum;
    std::vector<std::vector<void *>> page_addr(tree_meta->root_level + 1);
    // std::vector<std::vector<bnode *>> bnodes(tree_meta->root_level + 1);
    /* Part 1. get the positions for the compaction key */
    {
        bnode *p;
        //bleaf *lp;
        int i, t, m, b;
    #ifdef VAR_KEY
        int c;
    #endif

    Again2:
        // 1. RTM begin
        // if (_xbegin() != _XBEGIN_STARTED)
        // {
        //     // random backoff
        //     sum= 0;
        //     for (int i=(rdtsc() % 1024); i>0; i--) sum += i;
        //     goto Again2;
        // }

        // 2. search nonleaf nodes
        p = tree_meta->tree_root;

        for (i = tree_meta->root_level; i > 0; i--)
        {
        #ifdef PREFETCH
            // prefetch the entire node
            NODE_PREF(p);
        #endif
            assert(p->lock() == 0 || p->lock() == 1);
            // if the lock bit is set, abort
            if (p->lock())
            {
                // _xabort(3);
                goto Again2;
            }

            parray[i] = p;
            // if(i > 1){
            //     page_addr[i].push_back((void*)p);
            // }
            pnum[i] = p->num();

            // binary search to narrow down to at most 8 entries
            b = 1;
            t = p->num();
            while (b + 7 <= t)
            {
                m = (b + t) >> 1;
            #ifdef VAR_KEY
                c = vkcmp((char*)p->k(m), (char*)key);
                if (c > 0)
                    b = m + 1;
                else if (c < 0)
                    t = m - 1;
            #else
                if (key > p->k(m))
                    b = m + 1;
                else if (key < p->k(m))
                    t = m - 1;
            #endif
                else
                {
                    p = p->ch(m);
                    ppos[i] = m;
                    goto inner_done1;
                }
            }

            // sequential search (which is slightly faster now)
            for (; b <= t; b++)
            #ifdef VAR_KEY
                if (vkcmp((char*)key, (char*)p->k(b)) > 0)
                    break;
            #else
                if (key < p->k(b))
                    break;
            #endif
            if(b == 1) b++;
            p = p->ch(b - 1);
            ppos[i] = b - 1;

        inner_done1:;
        }
        for (i = tree_meta->root_level; i > 0; i--) {
            page_addr[i].push_back((void *)parray[i]);
            // bnodes[i].push_back((bnode *)parray[i]);
        }
        kBegin = nullptr;
        kEnd = nullptr;

        *ret_start = ((bnode *)parray[1])->k(ppos[1]);
        *index_start_pos = ppos[1];
        // assert(*ret_start <= start);
                if(start <= tree_meta->min_key){
            kBegin = nullptr;
                }else if(start == ((bnode *)parray[1])->k(ppos[1])){
            if(ppos[1] > 1){
                kBegin = ((bnode *)parray[1])->ch(ppos[1] - 1);
            }else{
                assert(start - 1 < start);
                kBegin = getKpage(start - 1, true);
                assert(kBegin != nullptr);
                assert(kBegin->nextPage() == ((bnode *)parray[1])->ch(ppos[1]));
            }
        }else if(start > ((bnode *)parray[1])->k(ppos[1])){
            kBegin = ((bnode *)parray[1])->ch(ppos[1]);
        }else{
            assert(false);
        }

        // if(*ret_start < start){
        //     kBegin
        // }
        //int start_pos = ppos[1];
        // return the result;
        while(true){
            if(end >= ((bnode *)parray[1])->k(pnum[1])){
                (*page_count) += (pnum[1] - ppos[1] + 1);
                *ret_end = ((bnode *)parray[1])->k(pnum[1]);
            }else{
                                for(short start_pos = ppos[1]; start_pos <= pnum[1]; start_pos++){
                //for(short start_pos = pnum[1] - 1; start_pos >= ppos[1]; start_pos--){
                    if(end < ((bnode *)parray[1])->k(start_pos)){
                        if(start_pos > 1){
                            *ret_end = ((bnode *)parray[1])->k(start_pos - 1);
                            (*page_count) += (start_pos - ppos[1]);
                        }
                        goto inner_done2;
                    }
                }
            }

            int level;
            for(level = 2; level <= tree_meta->root_level; level++){
                if(ppos[level] < pnum[level]){
                    ppos[level]++;
                    while(level >= 2){
                        parray[level - 1] = ((bnode *)parray[level])->ch(ppos[level]);
                        if(((bnode *)parray[level - 1])->kBegin() > end){
                            goto inner_done2;
                        }
                        page_addr[level - 1].push_back((void *)parray[level - 1]);
                        // bnodes[level - 1].push_back((bnode *)parray[level - 1]);
                        ppos[level - 1] = 1;
                        pnum[level - 1] = ((bnode *)parray[level - 1])->num();
                        level--;
                    }
                    break;
                }
            }
            if(level == tree_meta->root_level + 1){
                break;
            }
        }
    inner_done2:;
        if(end >= tree_meta->max_key){
            kEnd = nullptr;
        }else{
            assert(end < end + 1);
            kEnd = getKpage(end + 1, false);
        }
    }
    // 4. RTM commit
    // _xend();

    return page_addr;
}

void moveNode(bnode *src, bnode* dst, int src_start){
    for(int i = src_start; i <= src->num(); i++){
        dst->k(i - src_start + 1) = src->k(i);
        dst->ch(i - src_start + 1) = src->ch(i);
    }
    dst->num() = (src->num() - src_start + 1);
    src->num() -= dst->num();
}

void freeAfter(std::vector<std::vector<void*>> pages, int idx){
    for(int j = idx; j < pages.size(); j++){
        for(int k = 0; k < pages[j].size(); k++){
            free((bnode*)pages[j][k]);
        }
    }
}
bnode* splitNode(bnode* node, int idx){
    bnode* newNode = (bnode*)calloc(1, 256);
    moveNode(node, newNode, idx);
    return newNode;
}
bnode* splitNode(bnode* node, int start, int end, int skip){
    bnode* newNode = (bnode*)calloc(1, 256);
    int newCount = skip + node->num() - end + 1;
    assert(newCount < NON_LEAF_KEY_NUM);
    for(int i = end; i <= node->num(); i++){
        newNode->k(i - end + skip + 1) = node->k(i);
        newNode->ch(i - end + skip + 1) = node->ch(i);
    }
    node->num() = start - 1;
    newNode->num() = newCount;
    return newNode;
}


void lbtree::rangeDelete(std::vector<std::vector<void*>>& pages, key_type start, key_type end){
    bool endIsDeleted = true;
    key_type new_start;
        for(int i = 1; i < pages.size(); i++){
        bnode *node = (bnode*)pages[i][0];
                if(pages[i].size() == 1){
                        int first = node->search(start);
            for(int last = node->num(); last >= first; last--){
                if(node->k(last) <= end){
                    if(endIsDeleted){
                        if(i == 1 && (((kPage*)node->ch(last))->maxRawKey() > end)){
                            new_start = ((kPage*)node->ch(last))->findLargeThen(end);
                            node->setkandCheck(last, new_start);
                        }else{
                            last = last + 1;
                        }
                    }else{
                        node->setkandCheck(last, new_start);
                    }
                    node->remove(first, last);
                    break;
                }
            }
        }else{
                                    node->num() = node->search(start) - 1;
            if(node->num() == 0){
                free(node);
            }
            for(int j = 1; j < pages[i].size() - 1; j++){
                free(pages[i][j]);
            }
                        node = (bnode*)pages[i].back();
            for(int last = node->num(); last >= 1; last--){
                if(node->k(last) <= end){
                    if(endIsDeleted){
                        if(i == 1 && (((kPage*)node->ch(last))->maxRawKey() > end)){
                            new_start = ((kPage*)node->ch(last))->findLargeThen(end);
                            node->setkandCheck(last, new_start);
                        }else{
                            last = last + 1;
                        }                    
                    }else{
                        node->setkandCheck(last, new_start);
                    }
                    node->remove(1, last);
                    break;
                }
            }
        }
        if(node->num() == 0){
            free(node);
            endIsDeleted = true;
        }else{
            new_start = node->kBegin();
            endIsDeleted = false;
        }
    }
}


void lbtree::rangeReplace(std::vector<std::vector<void*>>& pages, std::vector<std::vector<void*>>& new_pages, key_type start, key_type end){
    bool endIsDeleted = true;
    key_type new_start;
    bool rootInserted = false;
    IdxEntry newSplit = {.k=0, .ch=0};
    IdxEntry newRoot = {.k=0, .ch=0};
    bool needOldRoot = false;
        for(int i = 1; i < pages.size(); i++){
        bnode *node = (bnode*)pages[i][0];
                if(pages[i].size() == 1){
            // start <= first, last <= end
                        int first = node->search(start);
            int last;
            for(last = node->num(); last >= first; last--){
                if(node->k(last) <= end){
                    if(endIsDeleted){
                        if(i == 1 && (((kPage*)node->ch(last))->maxRawKey() > end)){
                            new_start = ((kPage*)node->ch(last))->findLargeThen(end);
                            node->setkandCheck(last, new_start);
                        }else{
                            last = last + 1;
                        }
                    }else{
                        node->setkandCheck(last, new_start);
                    }
                    // node->remove(first, last);
                    break;
                }
            }
            if(first > last) last = first;
                        if(first == 1){
                                if(i == pages.size() - 1){
                    needOldRoot = true;
                }
                                if(i <= new_pages.size() - 1){
                                        if(i == new_pages.size() - 1){
                        bnode * new_node = (bnode *)new_pages.back().front();
                        newRoot.k = new_node->k(1);
                        newRoot.ch = (void*)new_node;
                    }
                    if(newSplit.ch != 0){
                        if(last > 1){
                            node->setKandCh(1, newSplit.k, newSplit.ch);
                            node->remove(2, last);
                            newSplit.ch = 0;
                        }else{
                            bnode *nextSplit = (bnode*)calloc(1, 256);
                            nextSplit->setKandCh(1, newSplit.k, newSplit.ch);
                            nextSplit->num() = 1;
                            newSplit.ch = (void*)nextSplit;
                        }
                    }else{
                        node->remove(1, last);
                    }
                }else if(newRoot.ch != 0){
                    if(newSplit.ch != 0){
                        if(last > 2){
                            node->setKandCh(1, newRoot.k, newRoot.ch);
                            node->setKandCh(2, newSplit.k, newSplit.ch);
                            newRoot.ch = 0; newSplit.ch = 0;
                            node->remove(3, last);
                        }else{
                            bnode *nextSplit = (bnode*)calloc(1, 256);
                            nextSplit->setKandCh(1, newRoot.k, newRoot.ch);
                            nextSplit->setKandCh(2, newSplit.k, newSplit.ch);
                            nextSplit->num() = 2;
                            newSplit.k = newRoot.k;
                            newSplit.ch = (void*)nextSplit;
                            newRoot.ch = 0;
                            node->remove(1, last);
                        }
                    }else{
                        if(last > 1){
                            node->setKandCh(1, newRoot.k, newRoot.ch);
                            node->remove(2, last);
                            newRoot.ch = 0;
                        }else{
                            bnode *nextRoot = (bnode*)calloc(1, 256);
                            nextRoot->setKandCh(1, newRoot.k, newRoot.ch);
                            nextRoot->num() = 1;
                            newRoot.ch = (void*)nextRoot;
                        }
                    }
                }else if(newSplit.ch != 0){
                    if(last > 1){
                        node->setKandCh(1, newSplit.k, newSplit.ch);
                        node->remove(2, last);
                        newSplit.ch = 0;
                    }else{
                        bnode *nextSplit = (bnode*)calloc(1, 256);
                        nextSplit->setKandCh(1, newSplit.k, newSplit.ch);
                        nextSplit->num() = 1;
                        newSplit.ch = (void*)nextSplit;
                    }
                }else{
                    node->remove(first, last);
                }
                        }else if(last == node->num() + 1){
                                if(i == pages.size() - 1){
                    needOldRoot = true;
                }
                                if(i < new_pages.size() - 1){
                                        if(newSplit.ch != 0){
                        bnode *nextSplit = (bnode*)calloc(1, 256);
                        nextSplit->setKandCh(1, newSplit.k, newSplit.ch);
                        nextSplit->num() = 1;
                        newSplit.ch = (void*)nextSplit;
                    }
                    node->remove(first, last);
                }else if(i == new_pages.size() - 1){
                                        bnode* rootNode = (bnode*)new_pages.back().front();
                    if(newSplit.ch != 0 && !rootNode->full()){
                        rootNode->insert(rootNode->num() + 1, newSplit.k, newSplit.ch);
                        newSplit.ch = 0;
                    }
                    newRoot.k = rootNode->k(1);
                    newRoot.ch = (void*)rootNode;
                    node->remove(first, last);
                }else if(newRoot.ch != 0){
                                        if(newSplit.ch != 0){
                        if(first < NON_LEAF_KEY_NUM){
                            if(first == last) node->num() += 2;
                            node->setKandCh(first, newRoot.k, newRoot.ch);
                            node->setKandCh(first + 1,newSplit.k, newSplit.ch);
                            node->remove(first + 2, last);
                            newRoot.ch = 0; newSplit.ch = 0;
                        }else{
                            bnode *nextSplit = (bnode*)calloc(1, 256);
                            nextSplit->setKandCh(1, newRoot.k, newRoot.ch);
                            nextSplit->setKandCh(2, newSplit.k, newSplit.ch);
                            nextSplit->num() = 2;
                                                        newRoot.ch = (void*)nextSplit;
                            newSplit.ch = 0;
                            node->remove(first, last);
                        }
                    }else{
                        if(first <= NON_LEAF_KEY_NUM){
                            if(first == last) node->num() += 1;
                            node->setKandCh(first, newRoot.k, newRoot.ch);
                            node->remove(first + 1, last);
                            newRoot.ch = 0;
                        }else{
                            bnode *nextSplit = (bnode*)calloc(1, 256);
                            nextSplit->setKandCh(1, newRoot.k, newRoot.ch);
                            nextSplit->num() = 1;
                            newRoot.ch = (void*)nextSplit;
                            node->remove(first, last);
                        }
                    }
                }else if(newSplit.ch != 0){
                    if(first <= NON_LEAF_KEY_NUM){
                        if(first == last) node->num() += 1;
                        node->setKandCh(first, newSplit.k, newSplit.ch);
                        node->remove(first + 1, last);
                        newSplit.ch = 0;
                    }else{
                        bnode *nextSplit = (bnode*)calloc(1, 256);
                        nextSplit->setKandCh(1, newSplit.k, newSplit.ch);
                        nextSplit->num() = 1;
                        newSplit.ch = (void*)nextSplit;
                        node->remove(first, last);
                    }
                }else{
                    node->remove(first, last);
                }
            }else{
                bnode* newNode = nullptr;
                                if(i < new_pages.size() - 1){
                                        if(newSplit.ch != 0){
                        newNode = splitNode(node, first, last, 1);
                        newNode->setKandCh(1, newSplit.k, newSplit.ch);
                        newSplit.ch = 0;
                    }else{
                        newNode = splitNode(node, first, last, 0);
                    }
                                }else if(i == new_pages.size() - 1){
                    assert(new_pages.back().size() == 1);
                    bnode * new_node = (bnode *)new_pages.back().front();
                                        int left = NON_LEAF_KEY_NUM - (first - 1);                      int splitCount = newSplit.ch != 0 ? 1 : 0;
                    if(new_node->num() + splitCount> left){                         newNode = splitNode(node, first, last, new_node->num() + splitCount - left);
                    }else{
                        newNode = splitNode(node, first, last, 0);
                    }
                    for(int j = 1; j <= new_node->num(); j++){
                        if(!node->full()){
                            node->insert(node->num()+1, new_node->k(j), new_node->ch(j));
                        }else{
                            newNode->setKandCh(j - left, new_node->k(j), new_node->ch(j));
                        }
                    }
                    if(newSplit.ch != 0){
                        if(!node->full()){
                            node->insert(node->num()+1, newSplit.k, newSplit.ch);
                        }else{
                            newNode->setKandCh(new_node->num() + 1 - left, newSplit.k, newSplit.ch);
                        }
                        newSplit.ch = 0;
                    }
                }else if(newRoot.ch != 0){
                                        if(newSplit.ch != 0){
                                                                        if(last - first >= 2){
                            node->setKandCh(first, newRoot.k, newRoot.ch);
                            node->setKandCh(first + 1, newSplit.k, newSplit.ch);
                            node->remove(first + 2, last);

                        }else if(last > 2){
                            newNode = splitNode(node, first, last, 2);
                            newNode->setKandCh(1, newRoot.k, newRoot.ch);
                            newNode->setKandCh(2, newSplit.k, newSplit.ch);
                        }else{
                                                        assert(false);
                        }
                        newRoot.ch = 0;
                        newSplit.ch = 0;
                    }else{
                                                if(last - first >= 1){
                            node->setKandCh(first, newRoot.k, newRoot.ch);
                            node->remove(first + 1, last);
                        }else{
                            assert(last == first);
                            if(!node->full()){
                                node->insert(first, newRoot.k, newRoot.ch);
                            }else{
                                newNode = splitNode(node, first, last, 1);
                                newNode->setKandCh(1, newRoot.k, newRoot.ch);
                            }
                        }
                        newRoot.ch = 0;
                    }
                }else if(newSplit.ch != 0){
                    if(last > first){
                                                node->setKandCh(first, newSplit.k, newSplit.ch);
                        node->remove(first + 1, last);
                    }else{
                                                assert(last == first);
                        if(!node->full()){
                            node->insert(first, newSplit.k, newSplit.ch);
                        }else{
                            newNode = splitNode(node, first, last, 1);
                            newNode->setKandCh(1, newSplit.k, newSplit.ch);
                        }
                    }
                    newSplit.ch = 0;
                }
                if(newNode != nullptr){
                    newSplit.k = newNode->k(1);
                    newSplit.ch = (void*)newNode;
                    if(i == pages.size() - 1){
                        needOldRoot = true;
                    }
                }
            }
        }else{
                        node->num() = node->search(start) - 1;
            assert(node->num() >= 0);
            if(node->num() == 0){
                free(node);
            }else if(newRoot.ch != 0 && !node->full()){
                node->insert(node->num() + 1, newRoot.k, newRoot.ch);
                newRoot.ch = 0;
            }
                        for(int j = 1; j < pages[i].size() - 1; j++){
                free(pages[i][j]);
            }
                        node = (bnode*)pages[i].back();
            int last;
            for(last = node->num(); last >= 1; last--){
                if(node->k(last) <= end){
                    if(endIsDeleted){
                        if(i == 1 && (((kPage*)node->ch(last))->maxRawKey() > end)){
                            new_start = ((kPage*)node->ch(last))->findLargeThen(end);
                            node->setkandCheck(last, new_start);
                        }else{
                            last = last + 1;
                        }
                    }else{
                        node->setkandCheck(last, new_start);
                    }
                    break;
                }
            }
            if(last > 1){
                if(newRoot.ch != 0){
                    node->setKandCh(1, newRoot.k, newRoot.ch);
                    node->remove(2, last);
                    newRoot.ch = 0;
                }else{
                    node->remove(1, last);
                }
            }
                                    if(i == new_pages.size() - 1){
                bnode* new_node = (bnode*)new_pages.back().front();
                newRoot.k = new_node->k(1);
                newRoot.ch = Pointer8B(new_node);
                        }else if(newRoot.ch != 0){
                bnode* newNode = (bnode*)calloc(1, 256);
                newNode->insert(1, newRoot.k, newRoot.ch);
                newRoot.ch = (void*)newNode;
            }
        }
        if(node->num() == 0){
            free(node);
            endIsDeleted = true;
        }else{
            new_start = node->kBegin();
            endIsDeleted = false;
        }
    }
        if(new_pages.size() <= pages.size() && (newRoot.ch != 0 || newSplit.ch != 0)){
        bnode* new_root = (bnode*)calloc(1, 256);
        int cur = 1;
                if(needOldRoot){
            bnode* oldRoot = (bnode*)pages.back().front();
            new_root->insert(cur++, oldRoot->k(1), (void*)oldRoot);
        }
                if(newRoot.ch != 0){
            new_root->insert(cur++, newRoot.k, newRoot.ch);
        }
                if(newSplit.ch != 0){
            new_root->insert(cur++, newSplit.k, newSplit.ch);
        }
        new_root->sort();
        tree_meta->tree_root = new_root;
        tree_meta->root_level++;
        }else if(new_pages.size() > pages.size()){
                bnode* firstNode = (bnode*)new_pages[pages.size()].front();
        bnode* lastNode = (bnode*)new_pages[pages.size()].back();
        if(needOldRoot){
            bnode* oldRoot = (bnode*)pages.back().front();
            if(firstNode->k(1) > oldRoot->k(1)){
                firstNode->insert(1, oldRoot->k(1), (void*)oldRoot);
            }else if(lastNode->kEnd() < oldRoot->k(1)){
                lastNode->insert(lastNode->num() + 1, oldRoot->k(1), (void*)oldRoot);;
            }else{
                assert(false);
            }
        }
        if(newSplit.ch != 0){
            assert(newSplit.k > lastNode->kEnd());
            lastNode->insert(lastNode->num() + 1, newSplit.k, newSplit.ch);
        }
        tree_meta->tree_root = new_pages.back().back();
        tree_meta->root_level = new_pages.size() - 1;
    }
}



/* ----------------------------------------------------------------- *
 look up
 * ----------------------------------------------------------------- */

/* leaf is level 0, root is level depth-1 */

void *lbtree::lookup(key_type key, int *pos)
{
    if(tree_meta->min_key > key || key > tree_meta->max_key){
        return nullptr;
    }
    bnode *p;
    vPage *vp = nullptr;
    kPage *kp = nullptr;
    int vIndex;
    int i, t, m, b;
#ifdef VAR_KEY
    int c;
#endif

    uint16_t key_hash = hashcode2B(key);
    int ret_pos;

Again1:
    // 1. RTM begin
    // if (_xbegin() != _XBEGIN_STARTED)
    //     goto Again1;

    // 2. search nonleaf nodes
    p = tree_meta->tree_root;

    for (i = tree_meta->root_level; i > 0; i--)
    {
    #ifdef PREFETCH
        // prefetch the entire node
        NODE_PREF(p);
    #endif
        assert(p->lock() == 0 || p->lock() == 1);
        // if the lock bit is set, abort
        if (p->lock())
        {
            // _xabort(1);
            goto Again1;
        }

        // binary search to narrow down to at most 8 entries
        b = 1;
        t = p->num();
        while (b + 7 <= t)
        {
            m = (b + t) >> 1;
        #ifdef VAR_KEY
            c = vkcmp((char*)p->k(m), (char*)key);
            if (c > 0)
                b = m + 1;
            else if (c < 0)
                t = m - 1;
        #else
            if (key > p->k(m))
                b = m + 1;
            else if (key < p->k(m))
                t = m - 1;
        #endif
            else
            {
                p = p->ch(m);
                goto inner_done;
            }
        }

        // sequential search (which is slightly faster now)
        for (; b <= t; b++)
        #ifdef VAR_KEY
            if (vkcmp((char*)key, (char*)p->k(b)) > 0)
                break;
        #else
            if (key < p->k(b))
                break;
        #endif
        if(b == 1){
            return nullptr;
        }
        p = p->ch(b - 1);

    inner_done:;
    }

    // 3. search leaf node
    kp = (kPage *)p;

#ifdef PREFETCH
    // prefetch the entire node
    LEAF_PREF(kp);
#endif
    // if the lock bit is set, abort
    // if (kp->lock)
    // {
    //     // _xabort(2);
    //     goto Again1;
    // }
    uint64_t ret_addr;
        for(int i = 0; i < kp->nums; i++){
        if(kp->finger[i] == key_hash && kp->isEqual(i, key)){   
            *pos = i;
            vp = (vPage* )(getAbsoluteAddr(((uint64_t)kp->pointer[i]) << 12));
            vIndex = kp->index[i];
            return vp->getValueAddr(vIndex);
        }
    }
    
    // 4. RTM commit
    // _xend();

    return nullptr;
}

// /* ------------------------------------- *
//    quick sort the keys in leaf node
//  * ------------------------------------- */

// // pos[] will contain sorted positions
// void lbtree::qsortBleaf(bleaf *p, int start, int end, int pos[])
// {
//     if (start >= end)
//         return;

//     int pos_start = pos[start];
//     key_type key = p->k(pos_start); // pivot
//     int l, r;

//     l = start;
//     r = end;
// #ifdef VAR_KEY
//     while (l < r)
//     {
//         while ((l < r) && (vkcmp((char*)p->k(pos[r]), (char*)key) < 0))
//             r--;
//         if (l < r)
//         {
//             pos[l] = pos[r];
//             l++;
//         }
//         while ((l < r) && (vkcmp((char*)p->k(pos[l]), (char*)key) >= 0))
//             l++;
//         if (l < r)
//         {
//             pos[r] = pos[l];
//             r--;
//         }
//     }
// #else
//     while (l < r)
//     {
//         while ((l < r) && (p->k(pos[r]) > key))
//             r--;
//         if (l < r)
//         {
//             pos[l] = pos[r];
//             l++;
//         }
//         while ((l < r) && (p->k(pos[l]) <= key))
//             l++;
//         if (l < r)
//         {
//             pos[r] = pos[l];
//             r--;
//         }
//     }
// #endif
//     pos[l] = pos_start;
//     qsortBleaf(p, start, l - 1, pos);
//     qsortBleaf(p, l + 1, end, pos);
// }

// /* ---------------------------------------------------------- *
 
//  insertion: insert (key, ptr) pair into unsorted_leaf_bmp
 
//  * ---------------------------------------------------------- */

// void lbtree::insert(key_type key, void *ptr)
// {
//     // record the path from root to leaf
//     // parray[level] is a node on the path
//     // child ppos[level] of parray[level] == parray[level-1]
//     //
//     Pointer8B parray[32]; // 0 .. root_level will be used
//     short ppos[32];       // 1 .. root_level will be used
//     bool isfull[32];      // 0 .. root_level will be used

//     unsigned char key_hash = hashcode1B(key);
//     volatile long long sum;

//     /* Part 1. get the positions to insert the key */
//     {
//         bnode *p;
//         bleaf *lp;
//         int i, t, m, b;
//     #ifdef VAR_KEY
//         int c;
//     #endif

//     Again2:
//         // 1. RTM begin
//         if (_xbegin() != _XBEGIN_STARTED)
//         {
//             // random backoff
//             sum= 0;
//             for (int i=(rdtsc() % 1024); i>0; i--) sum += i;
//             goto Again2;
//         }

//         // 2. search nonleaf nodes
//         p = tree_meta->tree_root;

//         for (i = tree_meta->root_level; i > 0; i--)
//         {
//         #ifdef PREFETCH
//             // prefetch the entire node
//             NODE_PREF(p);
//         #endif

//             // if the lock bit is set, abort
//             if (p->lock())
//             {
//                 _xabort(3);
//                 goto Again2;
//             }

//             parray[i] = p;
//             isfull[i] = (p->num() == NON_LEAF_KEY_NUM);

//             // binary search to narrow down to at most 8 entries
//             b = 1;
//             t = p->num();
//             while (b + 7 <= t)
//             {
//                 m = (b + t) >> 1;
//             #ifdef VAR_KEY
//                 c = vkcmp((char*)p->k(m), (char*)key);
//                 if (c > 0)
//                     b = m + 1;
//                 else if (c < 0)
//                     t = m - 1;
//             #else
//                 if (key > p->k(m))
//                     b = m + 1;
//                 else if (key < p->k(m))
//                     t = m - 1;
//             #endif
//                 else
//                 {
//                     p = p->ch(m);
//                     ppos[i] = m;
//                     goto inner_done;
//                 }
//             }

//             // sequential search (which is slightly faster now)
//             for (; b <= t; b++)
//             #ifdef VAR_KEY
//                 if (vkcmp((char*)key, (char*)p->k(b)) > 0)
//                     break;
//             #else
//                 if (key < p->k(b))
//                     break;
//             #endif
//             p = p->ch(b - 1);
//             ppos[i] = b - 1;

//         inner_done:;
//         }

//         // 3. search leaf node
//         lp = (bleaf *)p;

//     #ifdef PREFETCH
//         // prefetch the entire node
//         LEAF_PREF(lp);
//     #endif
//         // if the lock bit is set, abort
//         if (lp->lock)
//         {
//             _xabort(4);
//             goto Again2;
//         }

//         parray[0] = lp;

//         // SIMD comparison
//         // a. set every byte to key_hash in a 16B register
//         __m128i key_16B = _mm_set1_epi8((char)key_hash);

//         // b. load meta into another 16B register
//         __m128i fgpt_16B = _mm_load_si128((const __m128i *)lp);

//         // c. compare them
//         __m128i cmp_res = _mm_cmpeq_epi8(key_16B, fgpt_16B);

//         // d. generate a mask
//         unsigned int mask = (unsigned int)
//             _mm_movemask_epi8(cmp_res); // 1: same; 0: diff

//         // remove the lower 2 bits then AND bitmap
//         mask = (mask >> 2) & ((unsigned int)(lp->bitmap));

//         // search every matching candidate
//         while (mask)
//         {
//             int jj = bitScan(mask) - 1; // next candidate
//         #ifdef VAR_KEY
//             if (vkcmp((char*)lp->k(jj), (char*)key) == 0)
//             { // found: do nothing, return
//                 _xend();
//                 return;
//             }
//         #else
//             if (lp->k(jj) == key)
//             { // found: do nothing, return
//                 _xend();
//                 return;
//             }
//         #endif
//             mask &= ~(0x1 << jj); // remove this bit
//             /*  UBSan: implicit conversion from int -33 to unsigned int 
//                 changed the value to 4294967263 (32-bit, unsigned)      */
//         } // end while

//         // 4. set lock bits before exiting the RTM transaction
//         lp->lock = 1;

//         isfull[0] = lp->isFull();
//         if (isfull[0])
//         {
//             for (i = 1; i <= tree_meta->root_level; i++)
//             {
//                 p = parray[i];
//                 p->lock() = 1;
//                 if (!isfull[i])
//                     break;
//             }
//         }

//         // 5. RTM commit
//         _xend();

//     } // end of Part 1

//     /* Part 2. leaf node */
//     {
//         bleaf *lp = parray[0];
//         bleafMeta meta = *((bleafMeta *)lp);

//         /* 1. leaf is not full */
//         if (!isfull[0])
//         {
//             #if !defined(NVMPOOL_REAL) || !defined(UNLOCK_AFTER)
//             meta.v.lock = 0; // clear lock in temp meta
//             #endif

//             // 1.1 get first empty slot
//             uint16_t bitmap = meta.v.bitmap;
//             int slot = bitScan(~bitmap) - 1;

//             // 1.2 set leaf.entry[slot]= (k, v);
//             // set fgpt, bitmap in meta
//             lp->k(slot) = key;
//             lp->ch(slot) = ptr;
//             meta.v.fgpt[slot] = key_hash;
//             bitmap |= (1 << slot);

//             // 1.3 line 0: 0-2; line 1: 3-6; line 2: 7-10; line 3: 11-13
//             // in line 0?
//             if (slot < 3)
//             {
//                 // 1.3.1 write word 0
//                 meta.v.bitmap = bitmap;

//                 #if defined(NVMPOOL_REAL) && defined(NONTEMP) 
//                 lp->setWord0_temporal(&meta);
//                 #else
//                 lp->setWord0(&meta);
//                 #endif
//                 // 1.3.2 flush
//                 #ifdef NVMPOOL_REAL
//                 clwb(lp);
//                 sfence();
//                 #endif

//                 #if defined(NVMPOOL_REAL) && defined(UNLOCK_AFTER)
//                 ((bleafMeta *)lp)->v.lock = 0;
//                 #endif

//                 return;
//             }

//             // 1.4 line 1--3
//             else
//             {
//             #ifdef ENTRY_MOVING
//                 int last_slot = last_slot_in_line[slot];
//                 int from = 0;
//                 for (int to = slot + 1; to <= last_slot; to++)
//                 {
//                     if ((bitmap & (1 << to)) == 0)
//                     {
//                         // 1.4.1 for each empty slot in the line
//                         // copy an entry from line 0
//                         lp->ent[to] = lp->ent[from];
//                         meta.v.fgpt[to] = meta.v.fgpt[from];
//                         bitmap |= (1 << to);
//                         bitmap &= ~(1 << from);
//                         from++;
//                     }
//                 }
//             #endif

//                 // 1.4.2 flush the line containing slot
//                 #ifdef NVMPOOL_REAL
//                 clwb(&(lp->k(slot)));
//                 sfence();
//                 #endif

//                 // 1.4.3 change meta and flush line 0
//                 meta.v.bitmap = bitmap;
//                 #if defined(NVMPOOL_REAL) && defined(NONTEMP) 
//                 lp->setBothWords_temporal(&meta);
//                 #else
//                 lp->setBothWords(&meta);
//                 #endif
                
//                 #ifdef NVMPOOL_REAL
//                 clwb(lp);
//                 sfence();
//                 #endif

//                 #if defined(NVMPOOL_REAL) && defined(UNLOCK_AFTER)
//                 ((bleafMeta *)lp)->v.lock = 0;
//                 #endif

//                 return;
//             }
//         } // end of not full

//         /* 2. leaf is full, split */

//         // 2.1 get sorted positions
//         int sorted_pos[LEAF_KEY_NUM];
//         for (int i = 0; i < LEAF_KEY_NUM; i++)
//             sorted_pos[i] = i;
//         qsortBleaf(lp, 0, LEAF_KEY_NUM - 1, sorted_pos);

//         // 2.2 split point is the middle point
//         int split = (LEAF_KEY_NUM / 2); // [0,..split-1] [split,LEAF_KEY_NUM-1]
//         key_type split_key = lp->k(sorted_pos[split]);

//         // 2.3 create new node
//         bleaf *newp = (bleaf *)nvmpool_alloc_node(LEAF_SIZE);

//         // 2.4 move entries sorted_pos[split .. LEAF_KEY_NUM-1]
//         uint16_t freed_slots = 0;
//         for (int i = split; i < LEAF_KEY_NUM; i++)
//         {
//             newp->ent[i] = lp->ent[sorted_pos[i]];
//             newp->fgpt[i] = lp->fgpt[sorted_pos[i]];

//             // add to freed slots bitmap
//             freed_slots |= (1 << sorted_pos[i]);
//         }
//         newp->bitmap = (((1 << (LEAF_KEY_NUM - split)) - 1) << split);
//         newp->lock = 0;
//         newp->alt = 0;

//         // remove freed slots from temp bitmap
//         meta.v.bitmap &= ~freed_slots;

//         newp->next[0] = lp->next[lp->alt];
//         lp->next[1 - lp->alt] = newp;

//         // set alt in temp bitmap
//         meta.v.alt = 1 - lp->alt;

//         // 2.5 key > split_key: insert key into new node
//     #ifdef VAR_KEY
//         if (vkcmp((char*)key, (char*)split_key) < 0)
//     #else
//         if (key > split_key)
//     #endif
//         {
//             newp->k(split - 1) = key;
//             newp->ch(split - 1) = ptr;
//             newp->fgpt[split - 1] = key_hash;
//             newp->bitmap |= 1 << (split - 1);

//             if (tree_meta->root_level > 0)
//                 meta.v.lock = 0; // do not clear lock of root
//         }
    
//         // 2.6 clwb newp, clwb lp line[3] and sfence
//         #ifdef NVMPOOL_REAL
//         LOOP_FLUSH(clwb, newp, LEAF_LINE_NUM);
//         clwb(&(lp->next[0]));
//         sfence();
//         #endif

//         // 2.7 clwb lp and flush: NVM atomic write to switch alt and set bitmap
//         lp->setBothWords(&meta);
//         #ifdef NVMPOOL_REAL
//         clwb(lp);
//         sfence();
//         #endif

//         // 2.8 key < split_key: insert key into old node
//     #ifdef VAR_KEY
//         if (vkcmp((char*)key, (char*)split_key) >= 0)
//     #else
//         if (key <= split_key)
//     #endif
//         {

//             // note: lock bit is still set
//             if (tree_meta->root_level > 0)
//                 meta.v.lock = 0; // do not clear lock of root

//             // get first empty slot
//             uint16_t bitmap = meta.v.bitmap;
//             int slot = bitScan(~bitmap) - 1;

//             // set leaf.entry[slot]= (k, v);
//             // set fgpt, bitmap in meta
//             lp->k(slot) = key;
//             lp->ch(slot) = ptr;
//             meta.v.fgpt[slot] = key_hash;
//             bitmap |= (1 << slot);

//             // line 0: 0-2; line 1: 3-6; line 2: 7-10; line 3: 11-13
//             // in line 0?
//             if (slot < 3)
//             {
//                 // write word 0
//                 meta.v.bitmap = bitmap;
//                 lp->setWord0(&meta);
//                 // flush
//                 #ifdef NVMPOOL_REAL
//                 clwb(lp);
//                 sfence();
//                 #endif
//             }
//             // line 1--3
//             else
//             {
//             #ifdef ENTRY_MOVING
//                 int last_slot = last_slot_in_line[slot];
//                 int from = 0;
//                 for (int to = slot + 1; to <= last_slot; to++)
//                 {
//                     if ((bitmap & (1 << to)) == 0)
//                     {
//                         // for each empty slot in the line
//                         // copy an entry from line 0
//                         lp->ent[to] = lp->ent[from];
//                         meta.v.fgpt[to] = meta.v.fgpt[from];
//                         bitmap |= (1 << to);
//                         bitmap &= ~(1 << from);
//                         from++;
//                     }
//                 }
//             #endif

//                 // flush the line containing slot
//                 #ifdef NVMPOOL_REAL
//                 clwb(&(lp->k(slot)));
//                 sfence();
//                 #endif

//                 // change meta and flush line 0
//                 meta.v.bitmap = bitmap;
//                 lp->setBothWords(&meta);
//                 #ifdef NVMPOOL_REAL
//                 clwb(lp);
//                 sfence();
//                 #endif
//             }
//         }

//         key = split_key;
//         ptr = newp;
//         /* (key, ptr) to be inserted in the parent non-leaf */

//     } // end of Part 2

//     /* Part 3. nonleaf node */
//     {
//         bnode *p, *newp;
//         int n, i, pos, r, lev, total_level;

// #define LEFT_KEY_NUM ((NON_LEAF_KEY_NUM) / 2)
// #define RIGHT_KEY_NUM ((NON_LEAF_KEY_NUM)-LEFT_KEY_NUM)

//         total_level = tree_meta->root_level;
//         lev = 1;

//         while (lev <= total_level)
//         {

//             p = parray[lev];
//             n = p->num();
//             pos = ppos[lev] + 1; // the new child is ppos[lev]+1 >= 1

//             /* if the non-leaf is not full, simply insert key ptr */

//             if (n < NON_LEAF_KEY_NUM)
//             {
//                 for (i = n; i >= pos; i--)
//                     p->ent[i + 1] = p->ent[i];

//                 p->k(pos) = key;
//                 p->ch(pos) = ptr;
//                 p->num() = n + 1;
//                 #ifdef NVMPOOL_REAL
//                 sfence();
//                 #endif

//                 // unlock after all changes are globally visible
//                 p->lock() = 0;
//                 return;
//             }

//             /* otherwise allocate a new non-leaf and redistribute the keys */
//             newp = (bnode *)mempool_alloc_node(NONLEAF_SIZE);

//             /* if key should be in the left node */
//             if (pos <= LEFT_KEY_NUM)
//             {
//                 for (r = RIGHT_KEY_NUM, i = NON_LEAF_KEY_NUM; r >= 0; r--, i--)
//                 {
//                     newp->ent[r] = p->ent[i];
//                 }
//                 /* newp->key[0] actually is the key to be pushed up !!! */
//                 for (i = LEFT_KEY_NUM - 1; i >= pos; i--)
//                     p->ent[i + 1] = p->ent[i];

//                 p->k(pos) = key;
//                 p->ch(pos) = ptr;
//             }
//             /* if key should be in the right node */
//             else
//             {
//                 for (r = RIGHT_KEY_NUM, i = NON_LEAF_KEY_NUM; i >= pos; i--, r--)
//                 {
//                     newp->ent[r] = p->ent[i];
//                 }
//                 newp->k(r) = key;
//                 newp->ch(r) = ptr;
//                 r--;
//                 for (; r >= 0; r--, i--)
//                 {
//                     newp->ent[r] = p->ent[i];
//                 }
//             } /* end of else */

//             key = newp->k(0);
//             ptr = newp;

//             p->num() = LEFT_KEY_NUM;
//             if (lev < total_level)
//                 p->lock() = 0; // do not clear lock bit of root
//             newp->num() = RIGHT_KEY_NUM;
//             newp->lock() = 0;

//             lev++;
//         } /* end of while loop */

//         /* root was splitted !! add another level */
//         newp = (bnode *)mempool_alloc_node(NONLEAF_SIZE);

//         newp->num() = 1;
//         newp->lock() = 1;
//         newp->ch(0) = tree_meta->tree_root;
//         newp->ch(1) = ptr;
//         newp->k(1) = key;
//         #ifdef NVMPOOL_REAL
//         sfence(); // ensure new node is consistent
//         #endif

//         void *old_root = tree_meta->tree_root;
//         tree_meta->root_level = lev;
//         tree_meta->tree_root = newp;
//         #ifdef NVMPOOL_REAL
//         sfence(); // tree root change is globablly visible
//         #endif    // old root and new root are both locked

//         // unlock old root
//         if (total_level > 0)
//         { // previous root is a nonleaf
//             ((bnode *)old_root)->lock() = 0;
//         }
//         else
//         { // previous root is a leaf
//             ((bleaf *)old_root)->lock = 0;
//         }

//         // unlock new root
//         newp->lock() = 0;

//         return;

// #undef RIGHT_KEY_NUM
// #undef LEFT_KEY_NUM
//     }
// }

// /* ---------------------------------------------------------- *
 
//  deletion
 
//  lazy delete - insertions >= deletions in most cases
//  so no need to change the tree structure frequently
 
//  So unless there is no key in a leaf or no child in a non-leaf, 
//  the leaf and non-leaf won't be deleted.
 
//  * ---------------------------------------------------------- */
// void lbtree::del(key_type key)
// {
//     // record the path from root to leaf
//     // parray[level] is a node on the path
//     // child ppos[level] of parray[level] == parray[level-1]
//     //
//     Pointer8B parray[32];    // 0 .. root_level will be used
//     short ppos[32];          // 0 .. root_level will be used
//     bleaf *leaf_sibp = NULL; // left sibling of the target leaf

//     unsigned char key_hash = hashcode1B(key);
//     volatile long long sum;

//     /* Part 1. get the positions to insert the key */
//     {
//         bnode *p;
//         bleaf *lp;
//         int i, t, m, b;

//     Again3:
//         // 1. RTM begin
//         if (_xbegin() != _XBEGIN_STARTED)
//         {
//             // random backoff
//             // sum= 0;
//             // for (int i=(rdtsc() % 1024); i>0; i--) sum += i;
//             goto Again3;
//         }

//         // 2. search nonleaf nodes
//         p = tree_meta->tree_root;

//         for (i = tree_meta->root_level; i > 0; i--)
//         {
//         #ifdef PREFETCH
//             // prefetch the entire node
//             NODE_PREF(p);
//         #endif

//             // if the lock bit is set, abort
//             if (p->lock())
//             {
//                 _xabort(5);
//                 goto Again3;
//             }

//             parray[i] = p;

//             // binary search to narrow down to at most 8 entries
//             b = 1;
//             t = p->num();
//             while (b + 7 <= t)
//             {
//                 m = (b + t) >> 1;
//                 if (key > p->k(m))
//                     b = m + 1;
//                 else if (key < p->k(m))
//                     t = m - 1;
//                 else
//                 {
//                     p = p->ch(m);
//                     ppos[i] = m;
//                     goto inner_done;
//                 }
//             }

//             // sequential search (which is slightly faster now)
//             for (; b <= t; b++)
//                 if (key < p->k(b))
//                     break;
//             p = p->ch(b - 1);
//             ppos[i] = b - 1;

//         inner_done:;
//         }

//         // 3. search leaf node
//         lp = (bleaf *)p;

//     #ifdef PREFETCH
//         // prefetch the entire node
//         LEAF_PREF(lp);
//     #endif

//         // if the lock bit is set, abort
//         if (lp->lock)
//         {
//             _xabort(6);
//             goto Again3;
//         }

//         parray[0] = lp;

//         // SIMD comparison
//         // a. set every byte to key_hash in a 16B register
//         __m128i key_16B = _mm_set1_epi8((char)key_hash);

//         // b. load meta into another 16B register
//         __m128i fgpt_16B = _mm_load_si128((const __m128i *)lp);

//         // c. compare them
//         __m128i cmp_res = _mm_cmpeq_epi8(key_16B, fgpt_16B);

//         // d. generate a mask
//         unsigned int mask = (unsigned int)
//             _mm_movemask_epi8(cmp_res); // 1: same; 0: diff

//         // remove the lower 2 bits then AND bitmap
//         mask = (mask >> 2) & ((unsigned int)(lp->bitmap));

//         // search every matching candidate
//         i = -1;
//         while (mask)
//         {
//             int jj = bitScan(mask) - 1; // next candidate

//             if (lp->k(jj) == key)
//             { // found: good
//                 i = jj;
//                 break;
//             }

//             mask &= ~(0x1 << jj); // remove this bit
//         }                         // end while

//         if (i < 0)
//         { // not found: do nothing
//             _xend();
//             return;
//         }

//         ppos[0] = i;

//         // 4. set lock bits before exiting the RTM transaction
//         lp->lock = 1;

//         if (lp->num() == 1)
//         {

//             // look for its left sibling
//             for (i = 1; i <= tree_meta->root_level; i++)
//             {
//                 if (ppos[i] >= 1)
//                     break;
//             }

//             if (i <= tree_meta->root_level)
//             {
//                 p = parray[i];
//                 p = p->ch(ppos[i] - 1);
//                 i--;

//                 for (; i >= 1; i--)
//                 {
//                     p = p->ch(p->num());
//                 }

//                 leaf_sibp = (bleaf *)p;
//                 if (leaf_sibp->lock)
//                 {
//                     _xabort(7);
//                     goto Again3;
//                 }

//                 // lock leaf_sibp
//                 leaf_sibp->lock = 1;
//             }

//             // lock affected ancestors
//             for (i = 1; i <= tree_meta->root_level; i++)
//             {
//                 p = (bnode *)parray[i];
//                 p->lock() = 1;

//                 if (p->num() >= 1)
//                     break; // at least 2 children, ok to stop
//             }
//         }

//         // 5. RTM commit
//         _xend();

//     } // end of Part 1

//     /* Part 2. leaf node */
//     {
//         bleaf *lp = parray[0];

//         /* 1. leaf contains more than one key */
//         /*    If this leaf node is the root, we cannot delete the root. */
//         if ((lp->num() > 1) || (tree_meta->root_level == 0))
//         {
//             bleafMeta meta = *((bleafMeta *)lp);

//             #if !defined(NVMPOOL_REAL) || !defined(UNLOCK_AFTER)
//             meta.v.lock = 0;                  // clear lock in temp meta
//             #endif

//             meta.v.bitmap &= ~(1 << ppos[0]); // mark the bitmap to delete the entry
//             #if defined(NVMPOOL_REAL) && defined(NONTEMP) 
//             lp->setWord0_temporal(&meta);
//             #else
//             lp->setWord0(&meta);
//             #endif
            
//             #ifdef NVMPOOL_REAL
//             clwb(lp);
//             sfence();
//             #endif

//             #if defined(NVMPOOL_REAL) && defined(UNLOCK_AFTER)
//             ((bleafMeta *)lp)->v.lock = 0;
//             #endif

//             return;

//         } // end of more than one key

//         /* 2. leaf has only one key: remove the leaf node */

//         /* if it has a left sibling */
//         if (leaf_sibp != NULL)
//         {
//             // remove it from sibling linked list
//             leaf_sibp->next[leaf_sibp->alt] = lp->next[lp->alt];
//             #ifdef NVMPOOL_REAL
//             clwb(&(leaf_sibp->next[0]));
//             sfence();
//             #endif

//             leaf_sibp->lock = 0; // lock bit is not protected.
//                                  // It will be reset in recovery
//         }

//         /* or it is the first child, so let's modify the first_leaf */
//         else
//         {
//             tree_meta->setFirstLeaf(lp->next[lp->alt]); // the method calls clwb+sfence
//         }

//         // free the deleted leaf node
//         nvmpool_free_node(lp);

//     } // end of Part 2

//     /* Part 3: non-leaf node */
//     {
//         bnode *p, *sibp, *parp;
//         int n, i, pos, r, lev;

//         lev = 1;

//         while (1)
//         {
//             p = parray[lev];
//             n = p->num();
//             pos = ppos[lev];

//             /* if the node has more than 1 children, simply delete */
//             if (n > 0)
//             {
//                 if (pos == 0)
//                 {
//                     p->ch(0) = p->ch(1);
//                     pos = 1; // move the rest
//                 }
//                 for (i = pos; i < n; i++)
//                     p->ent[i] = p->ent[i + 1];
//                 p->num() = n - 1;
//                 #ifdef NVMPOOL_REAL
//                 sfence();
//                 #endif
//                 // all changes are globally visible now

//                 // root is guaranteed to have 2 children
//                 if ((p->num() == 0) && (lev >= tree_meta->root_level)) // root
//                     break;

//                 p->lock() = 0;
//                 return;
//             }

//             /* otherwise only 1 ptr */
//             mempool_free_node(p);

//             lev++;
//         } /* end of while */

//         // p==root has 1 child? so delete the root
//         tree_meta->root_level = tree_meta->root_level - 1;
//         tree_meta->tree_root = p->ch(0); // running transactions will abort
//         #ifdef NVMPOOL_REAL
//         sfence();
//         #endif

//         mempool_free_node(p);
//         return;
//     }
// }

// // Continue to add new record to result array, and shift the cur_idx
// // cur_idx points to a position that are empty
// // Return the cur_idx
// // inline int lbtree::add_to_sorted_result(std::pair<key_type, void*>* result, std::pair<key_type, void*>* new_record, int total_size, int cur_idx){
// //     if (cur_idx >= total_size)
// //     {
// //       if (result[total_size - 1].first < new_record->first)
// //       {
// //         return cur_idx;
// //       }
// //       cur_idx = total_size - 1; // Remove the last element
// //     }

// //     // Start the insertion sort
// //     int j = cur_idx - 1;
// //     while((j >= 0) && (result[j].first > new_record->first)){
// //       result[j + 1] = result[j];
// //       --j;
// //     }

// //     result[j + 1] = *new_record;
// //     ++cur_idx;
// //     return cur_idx;
// //   }

// // // Range scan in one node -- Author: Lu Baotong
// // int lbtree::range_scan_in_one_leaf(bleaf *lp, const key_type& key, uint32_t to_scan, std::pair<key_type, void*>* result){
// //     unsigned int mask = (unsigned int)(lp->bitmap);
// //     int cur_idx = 0;
// //     std::pair<key_type, void*> new_record;

// //     while (mask) {
// //         int jj = bitScan(mask)-1;  // next candidate
// //         if (lp->k(jj) >= key) { // found
// //             new_record.first = lp->k(jj);
// //             new_record.second = lp->ch(jj);
// //             // Add KV to the result array and matain its sort order
// //             cur_idx = add_to_sorted_result(result, &new_record, to_scan, cur_idx);
// //         }
// //         mask &= ~(0x1<<jj);  // remove this bit
// //     } // end while

// //     /*
// //     for(int i = 0; i < cur_idx; i++){
// //         std::cout << "key " << i << " = " << result[i].first << std::endl;
// //     }*/

// //     return cur_idx;
// // }

// // // Range query, first get the lock of node, and then atomically access each node in range
// // int lbtree::range_scan_by_size(const key_type& key,  uint32_t to_scan, char* my_result)
// // {   
// //     std::pair<key_type, void*> *result = reinterpret_cast<std::pair<key_type, void*> *>(my_result);
// //     bnode *p;
// //     bleaf *lp;
// //     int i,t,m,b;
// //     int result_idx;
    
// //     unsigned char key_hash= hashcode1B(key);
// //     int ret_pos;
    
// // Again1:
// //     // 1. RTM begin
// //     result_idx = 0; // The idx of result array
// //     if(_xbegin() != _XBEGIN_STARTED) goto Again1;

// //     // 2. search nonleaf nodes
// //     p = tree_meta->tree_root;
    
// //     for (i=tree_meta->root_level; i>0; i--) {
        
// //         // prefetch the entire node
// //         NODE_PREF(p);

// //         // if the lock bit is set, abort
// //         if (p->lock()) {_xabort(1); goto Again1;}
        
// //         // binary search to narrow down to at most 8 entries
// //         b=1; t=p->num();
// //         while (b+7<=t) {
// //             m=(b+t) >>1;
// //             if (key > p->k(m)) b=m+1;
// //             else if (key < p->k(m)) t = m-1;
// //             else {p=p->ch(m); goto inner_done;}
// //         }
        
// //         // sequential search (which is slightly faster now)
// //         for (; b<=t; b++)
// //             if (key < p->k(b)) break;
// //         p = p->ch(b-1);
        
// //     inner_done: ;
// //     }
    
// //     // 3. search leaf node
// //     lp= (bleaf *)p;

// //     // prefetch the entire node
// //     LEAF_PREF (lp);

// //     // 4. real range query
// //     auto remaining_scan = to_scan;
// //     auto cur_result = result;
// //     while((remaining_scan != 0) && (lp != nullptr)){
// //         // if the lock bit is set, abort
// //         if (lp->lock) {_xabort(2); goto Again1;}
// //         auto cur_scan = range_scan_in_one_leaf(lp, key, remaining_scan, cur_result);
// //         remaining_scan = remaining_scan - cur_scan;
// //         cur_result = cur_result + cur_scan;
// //         lp = lp->next[0]; // BT(FIX ME), the next pointer is not always this
// //     }

// //     // 5. RTM commit
// //     _xend();

// //     return (to_scan - remaining_scan);
// // }

// static int compareFunc(const void *a, const void *b)
// {
//     key_type tt = (((IdxEntry *)a)->k - ((IdxEntry *)b)->k);
//     return ((tt > 0) ? 1 : ((tt < 0) ? -1 : 0));
// }

// int lbtree::rangeScan(key_type key,  uint32_t scan_size, char* result)
// {
//     bnode *p;
//     bleaf *lp, *np = nullptr;
//     int i, t, m, b, jj;
//     unsigned int mask;
//     // volatile long long sum;
//     std::vector<IdxEntry> vec;
//     vec.reserve(scan_size);

// Again1: // find target leaf and lock it
//     // 1. RTM begin
//     if (_xbegin() != _XBEGIN_STARTED)
//     {
//         // Commented backoff because it will cause infinite abort in mempool mode
//         // sum= 0;
//         // for (int i=(rdtsc() % 1024); i>0; i--) sum += i;
//         goto Again1;
//     }
//     // 2. search nonleaf nodes
//     p = tree_meta->tree_root;
//     for (i = tree_meta->root_level; i > 0; i--)
//     {
//     #ifdef PREFETCH
//         // prefetch the entire node
//         NODE_PREF(p);
//     #endif
//         // if the lock bit is set, abort
//         if (p->lock())
//         {
//             _xabort(1);
//             std::this_thread::sleep_for(std::chrono::nanoseconds(1));
//             goto Again1;
//         }
//         // binary search to narrow down to at most 8 entries
//         b = 1;
//         t = p->num();
//         while (b + 7 <= t)
//         {
//             m = (b + t) >> 1;
//             if (key > p->k(m))
//                 b = m + 1;
//             else if (key < p->k(m))
//                 t = m - 1;
//             else
//             {
//                 p = p->ch(m);
//                 goto inner_done;
//             }
//         }
//         // sequential search (which is slightly faster now)
//         for (; b <= t; b++)
//             if (key < p->k(b))
//                 break;
//         p = p->ch(b - 1);
//     inner_done:;
//     }
//     lp = (bleaf *)p;
// #ifdef PREFETCH
//     // prefetch the entire node
//     LEAF_PREF(lp);
// #endif
//     // if the lock bit is set, abort
//     if (lp->lock)
//     {
//         _xabort(2);
//         std::this_thread::sleep_for(std::chrono::nanoseconds(1));
//         goto Again1;
//     }
//     lp->lock = 1;
//     // 4. RTM commit
//     _xend();

//     while (lp)
//     {
//         mask = (unsigned int)(lp->bitmap);
//         while (mask) {
//             jj = bitScan(mask)-1;  // next candidate
//             if (lp->k(jj) >= key) { // found
//                 vec.push_back(lp->ent[jj]);
//             }
//             mask &= ~(0x1<<jj);  // remove this bit
//         } // end while
//         if (vec.size() >= scan_size)
//             break;
//         np = lockSibling(lp);
//         lp->lock = 0;
//         lp = np;
//     }
//     if (lp)
//         lp->lock = 0;
//     qsort(vec.data(), vec.size(), sizeof(IdxEntry), compareFunc);
//     memcpy(result, vec.data(), vec.size() * sizeof(IdxEntry));
//     return vec.size() > scan_size? scan_size : vec.size();
// }

// bleaf* lbtree::lockSibling(bleaf* lp)
// {
//     // volatile long long sum;
//     bleaf * np = lp->nextSibling();
//     if (!np)
//         return NULL;
// Again2: // find and lock next sibling if necessary
//     if (_xbegin() != _XBEGIN_STARTED)
//     {
//         // sum= 0;
//         // for (int i=(rdtsc() % 1024); i>0; i--) sum += i;
//         goto Again2;
//     }
//     if (np->lock)
//     {
//         _xabort(2);
//         std::this_thread::sleep_for(std::chrono::nanoseconds(1));
//         goto Again2;
//     }
//     np->lock = 1;
//     _xend();
//     return np;
// }

// /* ----------------------------------------------------------------- *
//  randomize
//  * ----------------------------------------------------------------- */

// void lbtree::randomize(Pointer8B pnode, int level)
// {
//     int i;

//     if (level > 0)
//     {
//         bnode *p = pnode;
//         for (int i = 0; i <= p->num(); i++)
//         {
//             randomize(p->ch(i), level - 1);
//         }
//     }
//     else
//     {
//         bleaf *lp = pnode;

//         int pos[LEAF_KEY_NUM];
//         int num = 0;

//         // 1. get all entries
//         unsigned short bmp = lp->bitmap;
//         for (int i = 0; i < LEAF_KEY_NUM; i++)
//         {
//             if (bmp & (1 << i))
//             {
//                 pos[num++] = i;
//             }
//         }

//         // 2. randomly shuffle the entries
//         for (int i = 0; i < num * 2; i++)
//         {
//             int aa = (int)(drand48() * num); // [0,num-1]
//             int bb = (int)(drand48() * num);

//             if (aa != bb)
//             {
//                 swap(lp->fgpt[pos[aa]], lp->fgpt[pos[bb]]);
//                 swap(lp->ent[pos[aa]], lp->ent[pos[bb]]);
//             }
//         }
//     }
// }

// /* ----------------------------------------------------------------- *
//  print
//  * ----------------------------------------------------------------- */
// void lbtree::print(Pointer8B pnode, int level)
// {
//     if (level > 0)
//     {
//         bnode *p = pnode;

//         printf("%*cnonleaf lev=%d num=%d\n", 10 + level * 4, '+', level, p->num());

//         print(p->ch(0), level - 1);
//         for (int i = 1; i <= p->num(); i++)
//         {
//             printf("%*c%lld\n", 10 + level * 4, '+', p->k(i));
//             print(p->ch(i), level - 1);
//         }
//     }
//     else
//     {
//         bleaf *lp = pnode;

//         unsigned short bmp = lp->bitmap;
//         for (int i = 0; i < LEAF_KEY_NUM; i++)
//         {
//             if (bmp & (1 << i))
//             {
//                 printf("[%2d] hash=%02x key=%lld\n", i, lp->fgpt[i], lp->k(i));
//             }
//         }

//         bleaf *pnext = lp->nextSibling();
//         if (pnext != NULL)
//         {
//             int first_pos = bitScan(pnext->bitmap) - 1;
//             printf("->(%lld)\n", pnext->k(first_pos));
//         }
//         else
//             printf("->(null)\n");
//     }
// }

// /* ----------------------------------------------------------------- *
//  check structure integrity
//  * ----------------------------------------------------------------- */

// /**
//  * get min and max key in the given leaf p
//  */
// void lbtree::getMinMaxKey(bleaf *p, key_type &min_key, key_type &max_key)
// {
//     unsigned short bmp = p->bitmap;
//     max_key = MIN_KEY;
//     min_key = MAX_KEY;

//     for (int i = 0; i < LEAF_KEY_NUM; i++)
//     {
//         if (bmp & (1 << i))
//         {
//             if (p->k(i) > max_key)
//                 max_key = p->k(i);
//             if (p->k(i) < min_key)
//                 min_key = p->k(i);
//         }
//     }
// }

// void lbtree::checkFirstLeaf(void)
// {
//     // get left-most leaf node
//     bnode *p = tree_meta->tree_root;
//     for (int i = tree_meta->root_level; i > 0; i--)
//         p = p->ch(0);

//     if ((bleaf *)p != *(tree_meta->first_leaf))
//     {
//         printf("first leaf %p != %p\n", *(tree_meta->first_leaf), p);
//         exit(1);
//     }
// }

// /**
//  * recursively check the subtree rooted at pnode
//  *
//  * If it encounters an error, the method will print an error message and exit.
//  *
//  * @param pnode   the subtree root
//  * @param level   the level of pnode
//  * @param start   return the start key of this subtree
//  * @param end     return the end key of this subtree
//  * @param ptr     ptr is the leaf before this subtree. 
//  *                Upon return, ptr is the last leaf of this subtree.
//  */
// void lbtree::check(Pointer8B pnode, int level, key_type &start, key_type &end, bleaf *&ptr)
// {
//     if (pnode.isNull())
//     {
//         printf("level %d: null child pointer\n", level + 1);
//         exit(1);
//     }

//     if (level == 0)
//     { // leaf node
//         bleaf *lp = pnode;

//         if (((unsigned long long)lp) % 256 != 0)
//         {
//             printf("leaf(%p): not aligned at 256B\n", lp);
//             exit(1);
//         }

//         // check number of keys
//         if (lp->num() < 1)
//         { // empty node!
//             printf("leaf(%p): empty\n", lp);
//             exit(1);
//         }

//         // get min max
//         getMinMaxKey(lp, start, end);

//         // check fingerprints
//         unsigned short bmp = lp->bitmap;
//         for (int i = 0; i < LEAF_KEY_NUM; i++)
//         {
//             if (bmp & (1 << i))
//             {
//                 if (hashcode1B(lp->k(i)) != lp->fgpt[i])
//                 {
//                     printf("leaf(%lld): hash code for %lld is wrong\n", start, lp->k(i));
//                     exit(1);
//                 }
//             }
//         }

//         // check lock bit
//         if (lp->lock != 0)
//         {
//             printf("leaf(%lld): lock bit == 1\n", start);
//             exit(1);
//         }

//         // check sibling pointer
//         if ((ptr) && (ptr->nextSibling() != lp))
//         {
//             printf("leaf(%lld): sibling broken from previous node\n", start);
//             fflush(stdout);

//             /* output more info */
//             bleaf *pp = (bleaf *)ptr;
//             key_type ss, ee;
//             getMinMaxKey(pp, ss, ee);
//             printf("previous(%lld - %lld) -> ", ss, ee);

//             pp = pp->nextSibling();
//             if (pp == NULL)
//             {
//                 printf("nil\n");
//             }
//             else
//             {
//                 getMinMaxKey(pp, ss, ee);
//                 printf("(%lld - %lld)\n", ss, ee);
//             }

//             exit(1);
//         }

//         ptr = lp;
//     }

//     else
//     { // nonleaf node
//         key_type curstart, curend;
//         int i;
//         bleaf *curptr;

//         bnode *p = pnode;

//         if (((unsigned long long)p) % 64 != 0)
//         {
//             printf("nonleaf level %d(%p): not aligned at 64B\n", level, p);
//             exit(1);
//         }

//         // check num of keys
//         if (p->num() < 0)
//         {
//             printf("nonleaf level %d(%p): num<0\n", level, p);
//             exit(1);
//         }

//         // check child 0
//         curptr = ptr;
//         check(p->ch(0), level - 1, curstart, curend, curptr);
//         start = curstart;
//         if (p->num() >= 1 && curend >= p->k(1))
//         {
//             printf("nonleaf level %d(%lld): key order wrong at child 0\n", level, p->k(1));
//             exit(1);
//         }

//         // check child 1..num-1
//         for (i = 1; i < p->num(); i++)
//         {
//             check(p->ch(i), level - 1, curstart, curend, curptr);
//             if (!(p->k(i) <= curstart && curend < p->k(i + 1)))
//             {
//                 printf("nonleaf level %d(%lld): key order wrong at child %d(%lld)\n",
//                        level, p->k(1), i, p->k(i));
//                 exit(1);
//             }
//         }

//         // check child num (when num>=1)
//         if (i == p->num())
//         {
//             check(p->ch(i), level - 1, curstart, curend, curptr);
//             if (curstart < p->k(i))
//             {
//                 printf("nonleaf level %d(%lld): key order wrong at last child %d(%lld)\n",
//                        level, p->k(1), i, p->k(i));
//                 exit(1);
//             }
//         }
//         end = curend;

//         // check lock bit
//         if (p->lock() != 0)
//         {
//             printf("nonleaf level %d(%lld): lock bit is set\n", level, p->k(1));
//             exit(1);
//         }

//         ptr = curptr;
//     }
// }

// /* ------------------------------------------------------------------------- */
// /*                              driver                                       */
// /* ------------------------------------------------------------------------- */
// tree *initTree(void *nvm_addr, bool recover)
// {
//     tree *mytree = new lbtree(nvm_addr, recover);
//     return mytree;
// }
}
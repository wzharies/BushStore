// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "include/leveldb/options.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

#include "pm_table_builder.h"
#include "pm_mem_alloc.h"
#include "libpmem.h"
#include "util/global.h"

namespace leveldb {

// #define FNV1_PRIME_32 0x01000193
// #define FNV1_BASE_32 2166136261U

/* FNV-1a core implementation returning a 32 bit checksum over the first
 * LEN bytes in INPUT.  HASH is the checksum over preceding data (if any).
 */
// inline uint32_t fnv1a_32(const void *input, size_t len) {
//     uint32_t hash = FNV1_BASE_32;
//     const unsigned char *data = input;
//     const unsigned char *end = data + len;

//     for (; data != end; ++data){
//         hash ^= *data;
//         hash *= FNV1_PRIME_32;
//     }
//     return hash;
// }

void* PMTableBuilder::mallocBnode(){
    if(node_mem_ != nullptr){
        cur_node_index_++;
        return (void *)(node_mem_ + (cur_node_index_ - 1) * 256);
    }else{
        return calloc(1, 256);
    }
}

void* PMTableBuilder::mallocKpage(){
    if(node_mem_ != nullptr){
        void* ret = mallocKpages_.back();
        mallocKpages_.pop_back();
        return ret;
    }else{
        return pm_alloc_->mallocPage(key_t);
    }
}

// void* PMTableBuilder::mallocVpage(){
//     if(node_mem_ != nullptr){
//         void* ret = mallocVpages_.back();
//         mallocVpages_.pop_back();
//         return ret;
//     }else{
//         return pm_alloc_->mallocPage(value_t);
//     }
// }

void PMTableBuilder::initPreMalloc(uint64_t kvNums){
    int kPageCount = (kvNums + LEAF_KEY_NUM - 1) / LEAF_KEY_NUM;
    // int vPageCount = (kvNums + LEAF_VALUE_NUM - 1) / LEAF_VALUE_NUM;
    mallocKpages_ = pm_alloc_->mallocPage(key_t, kPageCount);
    // mallocVpages_ = pm_alloc_->mallocPage(value_t, vPageCount);
}

PMTableBuilder::PMTableBuilder(PMMemAllocator* pm_alloc, char* node_mem, uint64_t kvNums)\
     : pm_alloc_(pm_alloc), node_mem_(node_mem), cur_node_index_(0), kPage_count_(0), max_key_(0){
    write_.setPMAllocator(pm_alloc);
    key_buf_ = (kPage*)calloc(1, max_size);
    // value_buf_ = (vPage*)calloc(1, max_size);
    key_offset_ = 256;
    // value_offset_ = 256;
    pages.resize(32);
    leftPages.resize(32);
    initPreMalloc(kvNums);
    // value_page_ = (vPage*)mallocVpage();
    // used_pm_ += pm_alloc_->vPage_size_;
}

PMTableBuilder::~PMTableBuilder(){
    free(key_buf_);
    // free(value_buf_);
    if(!mallocKpages_.empty()){
        std::cout<<"k: "<<mallocKpages_.size()<<std::endl;
        for(auto& page : mallocKpages_){
            pm_alloc_->freePage((char*)page, key_t);
        }
    }
    // if(!mallocVpages_.empty()){
    //     std::cout<<"v: "<<mallocVpages_.size()<<std::endl;
    //     for(auto& page : mallocVpages_){
    //         pm_alloc_->freePage((char*)page, value_t);
    //     }
    // }
    //TODO value_page_如果为空是否需要delete(应该已经解决)
}

void PMTableBuilder::flush_kpage(){
    // kPage* page = pm_alloc_->palloc(key_offset_);
    kPage* new_page = (kPage*)mallocKpage();
    if(last_kpage_ != nullptr){
        last_kpage_->setNext(new_page);
    }
    if(firstPage == nullptr){
        firstPage = new_page;
    }
    last_kpage_ = new_page;
    // used_pm_ += pm_alloc_->kPage_size_;
    pages[0].push_back(new_page);
    if(pm_alloc_->options_.use_pm_){
        pmem_memcpy_persist(new_page, key_buf_, key_offset_);
    }else {
        memcpy(new_page, key_buf_, key_offset_);
    }
    kPage_count_++;
    memset(key_buf_, 0, key_offset_);
    key_offset_ = 256;
    key_type lastKey = new_page->rawK(0);
    Pointer8B lastAddr = (void*)new_page;
    for(int i = 1; i < 32;i++){
        max_level_ = std::max(max_level_, i);
        if(leftPages[i] == nullptr){
            // leftPages[i] = (bnode*)calloc(1, 256);
            leftPages[i] = (bnode*)mallocBnode();
        }
        //key是下一层page的第一个key
        leftPages[i]->k(leftPages[i]->num() + 1) = lastKey;
        //value是下一层page的地址
        leftPages[i]->ch(leftPages[i]->num() + 1) = lastAddr;
        leftPages[i]->num()++;

        if(leftPages[i]->num() != NON_LEAF_KEY_NUM - 2){
            break;
        }

        //如果这个page装满了, 则放入vector中
        //leftPages[i] = (bnode*)realloc(256);
        pages[i].push_back((void*)leftPages[i]);
        lastKey = leftPages[i]->kBegin();
        lastAddr = Pointer8B(leftPages[i]);
        leftPages[i] = nullptr;
    }
}

// void PMTableBuilder::flush_vpage(){
//     if(pm_alloc_->options_.use_pm_){
//         pmem_memcpy_persist(value_page_, value_buf_, value_offset_);
//     }else{
//         memcpy(value_page_, value_buf_, value_offset_);
//     }
//     memset(value_buf_, 0, value_offset_);
//     value_offset_ = 256;
// }

//这种为重写的情况，传入的时候保证pointer为相对地址
void PMTableBuilder::add(const Slice& key, uint32_t pointer, uint16_t index){
    // key_count_++;
    key_type key64 = DecodeDBBenchFixed64(key.data());
    assert(max_key_ <= key64);
    key_buf_->finger[key_buf_->nums] = hashcode2B(key64);;
    key_buf_->pointer[key_buf_->nums] = pointer;
    key_buf_->setk(key_buf_->nums, key);
    key_buf_->index[key_buf_->nums++] = index;
    // key_buf_->max_key = key64;
    max_key_ = key64;
    key_offset_ += (key.size());
    // std::cout<<key_buf_->max_key<<std::endl;
    if(key_buf_->nums == LEAF_KEY_NUM){
        key_buf_->bitmap = (1ULL << key_buf_->nums) - 1;
        flush_kpage();
    }
}

//这种为重写的情况，传入的时候保证pointer为相对地址
void PMTableBuilder::add(const Slice& key, uint16_t finger, uint32_t pointer, uint16_t index){
    // key_count_++;
    key_type key64 = DecodeDBBenchFixed64(key.data());
    assert(max_key_ <= key64);
    if(key64 == 2738038ULL){
        printf("temp\n");
    }
    key_buf_->finger[key_buf_->nums] = finger;
    key_buf_->pointer[key_buf_->nums] = pointer;
    key_buf_->setk(key_buf_->nums, key);
    key_buf_->index[key_buf_->nums++] = index;
    // key_buf_->max_key = key64;
    max_key_ = key64;
    key_offset_ += (key.size());
    // std::cout<<key_buf_->max_key<<std::endl;
    if(key_buf_->nums == LEAF_KEY_NUM){
        key_buf_->bitmap = (1ULL << key_buf_->nums) - 1;
        flush_kpage();
    }
}

//这种为新写入的情况，需要保证传入的pointer为相对地址
void PMTableBuilder::add(const Slice& key, const Slice& value, uint16_t finger){
    // key_count_++;

    auto [pointer, index] = write_.writeValue(key, value);

    key_type key64 = DecodeDBBenchFixed64(key.data());
    assert(max_key_ <= key64);
    max_key_ = key64;
    key_buf_->finger[key_buf_->nums] = finger;
    // key_buf_->pointer[key_buf_->nums] = (value_buf_ >> 12);
    // uint32_t temp = getRelativeAddr(value_page_) >> 12;
    //assert(temp!=1112352);
    // key_buf_->pointer[key_buf_->nums] = (reinterpret_cast<uint64_t>(getRelativeAddr(value_page_)) >> 12);
    key_buf_->pointer[key_buf_->nums] = pointer;
    key_buf_->setk(key_buf_->nums, key);
    key_buf_->index[key_buf_->nums++] = index;
    // key_buf_->max_key = key64;
    // std::cout<<key_buf_->max_key<<std::endl;
    max_key_ = key64;
    key_offset_ += (key.size());

    // value_offset_ = value_buf_->setv(value_buf_->alloc_num++, value_offset_, value);

    if(key_buf_->nums == LEAF_KEY_NUM){
        key_buf_->bitmap = (1ULL << key_buf_->nums) - 1;
        flush_kpage();
    }
    // if(value_buf_->alloc_num == LEAF_VALUE_NUM){
    //     value_buf_->total_num = value_buf_->alloc_num;
    //     value_buf_->bitmap = (1ULL << value_buf_->alloc_num) - 1;
    //     flush_vpage();
    //     value_page_ = (vPage*)mallocVpage();;
    //     used_pm_ += pm_alloc_->vPage_size_;
    // }
}

//flush的时候
void PMTableBuilder::add(const Slice& key, const Slice& value){
    add(key, value, hashcode1B(DecodeDBBenchFixed64(key.data())));
}

std::tuple<std::vector<std::vector<void *>>, kPage*, kPage*> PMTableBuilder::finish(std::shared_ptr<lbtree> &tree){
    //只要不为空就需要进行存储
    key_type lastKey = 0;
    Pointer8B lastAddr = Pointer8B(0);
    if(key_buf_->nums != 0){
        key_buf_->bitmap = (1ULL << key_buf_->nums) - 1;
        kPage* new_page = (kPage*)mallocKpage();
        lastPage = new_page;
        if (last_kpage_ != nullptr) {
            last_kpage_->setNext(new_page);
        }
        last_kpage_ = new_page;
        // used_pm_ += pm_alloc_->kPage_size_;
        pages[0].push_back(new_page);
        if(pm_alloc_->options_.use_pm_){
            pmem_memcpy_persist(new_page, key_buf_, key_offset_);
        }else {
            memcpy(new_page, key_buf_, key_offset_);
        }
        kPage_count_++;
        // memset(key_buf_, 0, key_offset_);
        // key_offset_ = 0;
        lastKey = new_page->rawK(0);
        lastAddr = Pointer8B(new_page);
    }else{
        lastPage = last_kpage_;
    }

    for(int i = 1; i <= max_level_;i++){
        max_level_ = std::max(max_level_, i);
        if(leftPages[i] == nullptr && lastKey != 0){
            //下一层有数据需要索引，但是这一层已经空了
            // leftPages[i] = (bnode*)calloc(1, 256);
            leftPages[i] = (bnode*)mallocBnode();
        }
        if(lastKey != 0){
            //不可能是满的，之前满的话是会立刻刷入的
            assert(!leftPages[i]->full());
            //key是下一层page的第一个key
            leftPages[i]->k(leftPages[i]->num() + 1) = lastKey;
            //value是下一层page的地址
            leftPages[i]->ch(leftPages[i]->num() + 1) = lastAddr;
            leftPages[i]->num()++;
        }

        if(leftPages[i] != nullptr){
            assert(leftPages[i]->num() != 0);
            //leftPages[i] = (bnode*)realloc(256);
            pages[i].push_back((void*)leftPages[i]);
            lastKey = leftPages[i]->kBegin();
            lastAddr = Pointer8B(leftPages[i]);
            //leftPages[i] = nullptr;
        }
    }
    write_.try_flush_vpage();

    // if(value_buf_->alloc_num != 0){
    //     value_buf_->total_num = value_buf_->alloc_num;
    //     value_buf_->bitmap = (1ULL << value_buf_->alloc_num) - 1;
    //     flush_vpage();
    // }else{
    //     pm_alloc_->freePage((char*)value_page_, key_t);
    //     used_pm_ -= pm_alloc_->kPage_size_;
    // }

    assert(kPage_count_ == pages[0].size());
    treeMeta* tree_meta = new treeMeta(Pointer8B(leftPages[max_level_]), max_level_, min_key_, max_key_, pages[1], node_mem_, kPage_count_);
    // tree_meta->cur_size = used_pm_;
    tree = std::make_shared<lbtree>(tree_meta, pm_alloc_);
    assert(pages[max_level_ + 1].size() == 0);
    pages.resize(max_level_ + 1);
    assert(pages.back().size() == 1);
    return std::make_tuple(pages, firstPage, lastPage);
}
void PMTableBuilder::setMaxKey(const Slice& key){
    max_key_ = DecodeDBBenchFixed64(key.data());
}
void PMTableBuilder::setMinKey(const Slice& key){
    min_key_ = DecodeDBBenchFixed64(key.data());
}
// void PMTableBuilder::Add(const Slice& key, const Slice& value, unsigned char finger){
//     uint64_t key_total_size = key.size_ + 8;
//     uint64_t value_total_size = key.size_ + value.size_ + 8 + 8;
//     uint32_t addr = raw_;
//     if((offset_ + total_size ) > nvm_cf_->GetSstableEachSize()){
//         printf("error:write l0 sstable size over!\n");
//         return;
//     }

//     //写入key 和 value的值到buf
//     std::string skey;
//     std::string key_value;
//     PutFixed64(&skey, key.size_);
//     skey.append(key.data_, key.size_);

//     key_value += skey;
//     PutFixed64(&key_value, value.size_);
//     key_value.append(value.data_, value.size_);

//     memcpy(key_buf_ + key_offset_ + 256, skey.c_str(), key_total_size);
//     memcpy(value_buf_ + value_offset_ + 256, key_value.c_str(), value_total_size);

//     //写入key和value的索引到buf
//     assert(keys_num_ < 256);
//     addr = addr + keys_num;

//     EncodeFixed16(key_buf_ + keys_num_ * 2, fnv1a_32(key.data_, key.size_));
//     EncodeFixed32(key_buf_ + 84 + keys_num_ * 4, addr);
//     EncodeFixed16(value_buf_ + values_num_ * 2, value_offset_);

//     key_offset_ += key_total_size;
//     value_offset_ += value_total_size;
//     keys_num_++;
//     values_num_++;
//     if(keys_num_ > 256 / 6){
//         flush_key();
//     }
//     if(values_num_ > (256 - 16) / 2) {
//         flush_value();
//     }
// }
}  // namespace leveldb

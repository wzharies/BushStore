
#pragma once
#include<memory>
#include<string>
#include<vector>
#include "include/slice.h"
#include "table_meta.h"
#include "pm_mem_alloc.h"

namespace leveldb{

class PMTableBuilder{
public:
    PMTableBuilder(PMMemAllocator* pm_alloc);
    ~PMTableBuilder();
    void Add(const Slice& key, const Slice& value);
    Status Finish();

    // uint64_t GetFileSize(){
    //     return offset_;
    // }

private:
    flush_kpage();
    flush_vpage();

    PMMemAllocator* pm_alloc_;
    char* key_raw_; //pm
    char* value_raw_; //pm
    char* key_buf_; //dram
    char* value_buf_; //dram
    
    std::vector<uint16_t> fingers_;
    std::vector<uint32_t> slots_;
    std::vector<uint16_t> offsets_;
    
    uint64_t keys_num_;
    uint64_t values_num_;
    uint64_t key_offset_;
    uint64_t value_offset_;

    uint64_t begin_offset_;
    uint64_t keys_meta_size_;
};

}
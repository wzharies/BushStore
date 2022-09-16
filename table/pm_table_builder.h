
#pragma once
#include<memory>
#include<string>
#include<vector>
#include "include/slice.h"
#include "table_meta.h"

namespace leveldb{

class PMTableBuilder{
public:
    PMTableBuilder(NvmCfModule* nvm_cf,
                   FileEntry* file,
                   char* raw);
    ~PMTableBuilder();
    void Add(const Slice& key, const Slice& value);
    Status Finish();

    uint64_t GetFileSize(){
        return offset_;
    }




private:
    //NvmCfModule * nvm_cf_;
    //FileEntry* file_;
    char* raw_;
    char* buf_;
    std::vector<KeysMetadata> first_indexs_;
    std::vector<uint16_t> fingers_;
    std::vector<uint32_t> slots_;
    uint64_t keys_num_;
    uint64_t offset_;
    uint64_t begin_offset_;
    uint64_t keys_meta_size_;
};

}
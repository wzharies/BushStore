#ifndef STORAGE_LEVELDB_TABLE_PM_TABLE_BUILDER_H_
#define STORAGE_LEVELDB_TABLE_PM_TABLE_BUILDER_H_
#include<memory>
#include<string>
#include<vector>
#include "leveldb/slice.h"
#include "table_meta.h"
#include "pm_mem_alloc.h"
#include "bplustree/bptree.h"
namespace leveldb{

//8Byte to 1Byte
static inline unsigned char hashcode1B(key_type x)
{
   x ^= x >> 32;
   x ^= x >> 16;
   x ^= x >> 8;
   return (unsigned char)(x & 0x0ffULL);
}

class PMTableBuilder{
public:
    PMTableBuilder(PMMemAllocator* pm_alloc, char * node_mem = nullptr);
    ~PMTableBuilder();
    void add(const Slice& key, unsigned char finger, uint32_t pointer, unsigned char index);
    void add(const Slice& key, const Slice& value, unsigned char finger);
    void add(const Slice& key, const Slice& value);
    void setMaxKey(const Slice& key);
    void setMinKey(const Slice& key);
    std::vector<std::vector<void *>> finish(std::shared_ptr<lbtree> &tree);

    // uint64_t GetFileSize(){
    //     return offset_;
    // }
    int cur_node_index_;


private:
    void flush_kpage();
    void flush_vpage();
    void* mallocBnode();

    PMMemAllocator* pm_alloc_;
    char* node_mem_;
    // char* key_raw_; //pm
    // char* value_raw_; //pm
    //std::vector<void *> kPages;
    std::vector<std::vector<void *>> pages; // 0 : kPage 、1-n : bnode
    std::vector<bnode*> leftPages;
    kPage* key_buf_; //dram
    vPage* value_buf_; //dram
    vPage* value_page_; //pm
    int max_level_ = 0;
    
    // std::vector<uint16_t> fingers_;
    // std::vector<uint32_t> slots_;
    // std::vector<uint16_t> offsets_;
    
    // uint64_t keys_num_;
    // uint64_t values_num_;
    uint64_t key_offset_;
    uint64_t value_offset_;
    key_type max_key_;
    key_type min_key_;
    // uint64_t key_count_ = 0;

    uint64_t used_pm_;
    uint64_t kPage_count_;

    // uint64_t begin_offset_;
    // uint64_t keys_meta_size_;
};

}
#endif
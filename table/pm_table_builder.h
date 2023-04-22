#ifndef STORAGE_LEVELDB_TABLE_PM_TABLE_BUILDER_H_
#define STORAGE_LEVELDB_TABLE_PM_TABLE_BUILDER_H_
#include<memory>
#include<string>
#include<vector>
#include "leveldb/slice.h"
#include "table_meta.h"
#include "pm_mem_alloc.h"
#include "bplustree/bptree.h"
#include "util/global.h"
namespace leveldb{

constexpr int VPAGE_CAPACITY = 512 * 1024;
class vPageWriteDirect {
public:
  vPageWriteDirect(){
  }
  ~vPageWriteDirect(){
    if(value_nums_ > 4){ 
      flush_vpage();
    }else{
      pm_alloc_->freePage((char*)page_pm_, value_t);
    }
    // TODO page_pm_ need free.
  }
  void setPMAllocator(PMMemAllocator* allocator){
    pm_alloc_ = allocator;
    page_pm_ = (vPage*)pm_alloc_->mallocPage(value_t);
    page_pm_->nums() = 0;
    page_pm_->setNext(nullptr);
  }
  // void try_flush_vpage(){
  //   if(value_nums_ > 4){ 
  //     flush_vpage();
  //   }
  // }
  void flush_vpage(){
    assert(page_pm_->nums() + 4 == value_nums_);
    page_pm_->capacity() = value_nums_;
    vPage* next_page_pm_ = (vPage*)pm_alloc_->mallocPage(value_t);
    page_pm_->setNext(next_page_pm_);
    assert(page_pm_->next() == next_page_pm_);
    page_pm_ = next_page_pm_;
    page_pm_->nums() = 0;
    page_pm_->setNext(nullptr);
    value_offset_ = VPAGE_CAPACITY;
    value_nums_ = 4;
  }
  std::tuple<uint32_t, uint16_t> writeValue(const Slice& key, const Slice& value){
    assert(page_pm_ != nullptr);
    assert(key.size() == 8);
    if(page_pm_->isFull(value_offset_ - (key.size() + 4 + value.size()), value_nums_)){
      flush_vpage();
    }
    uint32_t pointer = (reinterpret_cast<uint64_t>(getRelativeAddr(page_pm_)) >> 12);
    value_offset_ = page_pm_->setkv(value_nums_, value_offset_, key, value);
    assert(value_nums_ >= 4);
    return std::make_tuple(pointer, value_nums_++);
  }
private: 
  PMMemAllocator* pm_alloc_ = nullptr;
  vPage* page_pm_ = nullptr;
  int value_offset_ = VPAGE_CAPACITY;
  int value_nums_ = 4;
};

class vPageWrite {
public:
  vPageWrite(){
    page_buffer_ = (vPage*)calloc(1, max_size);
  }
  ~vPageWrite(){
    if(value_nums_ > 4){ 
      flush_vpage();
    }else{
      pm_alloc_->freePage(page_pm_, value_t);
    }
    free(page_buffer_);
    // TODO page_pm_ need free.
  }
  void setPMAllocator(PMMemAllocator* allocator){
    pm_alloc_ = allocator;
    page_pm_ = (char*)pm_alloc_->mallocPage(value_t);
  }
  void try_flush_vpage(){
    if(value_nums_ > 4){ 
      flush_vpage();
    }
  }
  void flush_vpage(){
    assert(page_buffer_->nums() + 4 == value_nums_);
    page_buffer_->capacity() = value_nums_;
    if(pm_alloc_->options_.use_pm_){
        pmem_memcpy_persist(page_pm_, page_buffer_, VPAGE_CAPACITY);
    }else{
        memcpy(page_pm_, page_buffer_, VPAGE_CAPACITY);
    }
    memset(page_buffer_, 0, value_offset_);
    value_offset_ = VPAGE_CAPACITY;
    value_nums_ = 4;
  }
  std::tuple<uint32_t, uint16_t> writeValue(const Slice& key, const Slice& value){
    assert(page_pm_ != nullptr);
    assert(key.size() == 8);
    if(page_buffer_->isFull(value_offset_ - (key.size() + 4 + value.size()), value_nums_)){
      flush_vpage();
      page_pm_ = (char*)pm_alloc_->mallocPage(value_t);
    }
    uint32_t pointer = (reinterpret_cast<uint64_t>(getRelativeAddr(page_pm_)) >> 12);
    value_offset_ = page_buffer_->setkv(value_nums_, value_offset_, key, value);
    assert(value_nums_ >= 4);
    return std::make_tuple(pointer, value_nums_++);
  }
private: 
  PMMemAllocator* pm_alloc_ = nullptr;
  vPage* page_buffer_ = nullptr;
  char* page_pm_ = nullptr;
  int value_offset_ = VPAGE_CAPACITY;
  int value_nums_ = 4;
};

class PMTableBuilder{
public:
    PMTableBuilder(PMMemAllocator* pm_alloc, char * node_mem = nullptr, uint64_t kvNums = 0);
    ~PMTableBuilder();
    void add(const Slice& key, uint16_t finger, uint32_t pointer, uint16_t index);
    void add(const Slice& key, uint32_t pointer, uint16_t index);
    void add(const Slice& key, const Slice& value, uint16_t finger);
    void add(const Slice& key, const Slice& value);
    void setMaxKey(const Slice& key);
    void setMinKey(const Slice& key);
    std::tuple<std::vector<std::vector<void *>>, kPage*, kPage*> finish(std::shared_ptr<lbtree> &tree);
    void initPreMalloc(uint64_t kvNums);

    // uint64_t GetFileSize(){
    //     return offset_;
    // }
    int cur_node_index_;


private:
    void flush_kpage();
    // void flush_vpage();
    void* mallocBnode();
    void* mallocKpage();
    // void* mallocVpage();

    PMMemAllocator* pm_alloc_;
    vPageWrite write_;
    char* node_mem_;
    std::vector<void*> mallocKpages_;
    std::vector<void*> mallocVpages_;
    // char* key_raw_; //pm
    // char* value_raw_; //pm
    //std::vector<void *> kPages;
    std::vector<std::vector<void *>> pages; // 0 : kPage 、1-n : bnode
    std::vector<bnode*> leftPages;
    kPage* key_buf_; //dram
    kPage* last_kpage_ = nullptr;
    kPage* firstPage = nullptr;
    kPage* lastPage = nullptr;
    // vPage* value_buf_; //dram
    // vPage* value_page_; //pm
    int max_level_ = 0;
    
    // std::vector<uint16_t> fingers_;
    // std::vector<uint32_t> slots_;
    // std::vector<uint16_t> offsets_;
    
    // uint64_t keys_num_;
    // uint64_t values_nums_;
    uint64_t key_offset_;
    // uint64_t value_offset_;
    key_type max_key_;
    key_type min_key_;
    // uint64_t key_count_ = 0;

    // uint64_t used_pm_;
    uint64_t kPage_count_;

    // uint64_t begin_offset_;
    // uint64_t keys_meta_size_;
};

}
#endif
#ifndef STORAGE_LEVELDB_TABLE_PM_MEM_ALLOC_H_
#define STORAGE_LEVELDB_TABLE_PM_MEM_ALLOC_H_
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <mutex>
#include <deque>
#include "include/leveldb/options.h"
#include "leveldb/slice.h"

#include "bitmap.h"
#include "libpmem.h"
#include "table_meta.h"

namespace leveldb {

enum PageType {
  key_t,
  value_t
};

extern uint64_t base_addr;

inline uint64_t getRelativeAddr(void* addr){
  return (uint64_t)addr - base_addr;
}
inline void* getAbsoluteAddr(void* addr){
  return (void* )((uint64_t)addr + base_addr);
}
inline uint64_t getRelativeAddr(uint64_t addr){
  return addr - base_addr;
}
inline void* getAbsoluteAddr(uint64_t addr){
  return (void* )(addr + base_addr);
}

// PageType getFixedSize(size_t page_size_){

// }

inline uint64_t ceilBitMapSize(uint64_t page_count, uint64_t page_size){
  return (4 + page_count / 8 + page_size - 1) / page_size * page_size;
}

// Extent内存分为
// n * [ 4Bytes(nums) + m bit ] (PageCount) + PageSize * PageCount = total;
inline uint64_t calPageCount(uint64_t total, uint64_t page_size){
    uint64_t page_count = (total - 64) * 8 / (page_size * 8 + 1);
    // 求出bitmap的大小对pageSize取整的大小
    while(true){
      uint64_t ceilBitmapSize = ceilBitMapSize(page_count, page_size);
      if(ceilBitmapSize + page_size * page_count <= total){
        return page_count;
      }
      page_count--;
    }
}


class PMExtent {
public:
  //新建一个Extent
  PMExtent(uint64_t page_count, uint64_t page_size, char* pmem_addr) :
    pmem_addr_(pmem_addr), page_count_(page_count), page_size_(page_size), last_empty_(0) {
    used_count_ = 0;
    uint64_t ceilBitmapSize = ceilBitMapSize(page_count, page_size);
    page_start_addr_ = pmem_addr + ceilBitmapSize;
    // if(page_count + 64 < 4096 * 8){
    //   page_start_addr_ = pmem_addr_ + 4096;
    // }else{
    //   page_start_addr_ =
    //       pmem_addr_ +
    //       ((page_count + 64) % (4096 * 8) == 0 ? (page_count + 64) / 8 : ((page_count + 64) / 8 + 4096) / 4096 * 4096);
    // }
    bitmap_ = (Bitmap*)pmem_addr;
    bitmap_->nums_ = page_count_;
    for(int i = 0; i < bitmap_->nums_; i++){
      char* addr = page_start_addr_ + i * page_size_;
      free_lists_.push_back(addr);
    }
    assert(pmem_addr_ + bitmap_->nums_ / 8 + 8 < page_start_addr_);
  }
  std::vector<void*> getNewPage(uint64_t kvCount){
    std::vector<void*> res;
    while(kvCount-- && !free_lists_.empty()){
      char* ret = free_lists_.front();
      free_lists_.pop_front();
      int index = (ret - page_start_addr_) / page_size_;
      bitmap_->set(index);
      used_count_++;
      res.push_back(ret);
    }
    return res;
  }
  char* getNewPage() {
    // bitmap_->getEmpty(last_empty_);
    // bitmap_->set(last_empty_);
    // used_count_++;
    // char* ret = page_start_addr_ + last_empty_ * page_size_;
    // last_empty_ = (last_empty_ + 1) % bitmap_->nums_;
    // if(last_empty_ == 0){
    //   std::cout<< "allocator is reallocating." << std::endl;
    // }
    // if( (uint64_t)ret == (uint64_t)0x555589cfb800){
    //   std::cout<<"malloc"<<std::endl;
    // }

    if(free_lists_.empty()){
      return nullptr;
    }
    char* ret = free_lists_.front();
    free_lists_.pop_front();
    int index = (ret - page_start_addr_) / page_size_;
    bitmap_->set(index);
    used_count_++;
    return ret;
  }
  void freePage(char* page_addr) {
    // if( (uint64_t)page_addr == (uint64_t)0x555589cfb800){
    //   std::cout<<"free"<<std::endl;
    // }
    char* addr = page_addr;
    free_lists_.push_back(addr);
    bitmap_->clr((addr - page_start_addr_) / page_size_);
    used_count_--;
    assert(used_count_ >= 0);
  }
  bool isFull() { return used_count_ == page_count_; }
  ~PMExtent() {}

  uint64_t page_count_;
  uint64_t used_count_;
  uint64_t page_size_;
  uint64_t extent_id_;
  Bitmap* bitmap_;
  size_t last_empty_;
  char* pmem_addr_;// extent起始地址
  char* page_start_addr_;// extent起始地址 + bitmap地址
  std::deque<char*> free_lists_;
};

class PMMemAllocator {
 public:
  PMMemAllocator(const Options& options_);
  ~PMMemAllocator();
  const void* PmAlloc(size_t pm_len);
  void* mallocPage(PageType type);
  std::vector<void*> mallocPage(PageType type, uint64_t count);
  void freePage(char* addr, PageType type);
  uint64_t GetKpageSize() { return kPage_size_; }
  uint64_t GetVpageSize() { return vPage_size_; }
  void Sync();
  double getMemoryUsabe(){
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t total = 0;
    uint64_t used = 0;
    for(int i = 0; i < pages.size();i++){
      total += pages[i]->page_count_ * pages[i]->page_size_;
      used += pages[i]->used_count_ * pages[i]->page_size_;
    }
    total += (options_.pm_size_ / options_.extent_size_ - pages.size()) * options_.extent_size_;
    return 1.0 * used / total;
  }
  const Options& options_;
  uint64_t kPage_size_;
  uint64_t vPage_size_;
 private:
  PMExtent* NewExtent(PageType type);
  uint64_t SuitablePageSize(uint64_t page_size);
  uint64_t new_extent_id_;
  std::vector<PMExtent*> Kpage_;
  std::vector<PMExtent*> Vpage_;
  std::vector<PMExtent*> pages;
  uint64_t kPage_slot_count_;
  uint64_t kPage_count_;
  uint64_t vPage_slot_count_;
  uint64_t vPage_count_;

  size_t mapped_len_;
  int is_pmem_;
  std::mutex mutex_;
};

}  // namespace leveldb
#endif
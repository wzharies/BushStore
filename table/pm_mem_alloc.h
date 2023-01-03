#ifndef STORAGE_LEVELDB_TABLE_PM_MEM_ALLOC_H_
#define STORAGE_LEVELDB_TABLE_PM_MEM_ALLOC_H_
#include <memory>
#include <string>
#include <vector>

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

class PMExtent {
public:
  //新建一个Extent
  PMExtent(uint64_t page_count, uint64_t page_size, char* pmem_addr) :
    pmem_addr_(pmem_addr), page_count_(page_count), page_size_(page_size), last_empty_(0) {
    used_count_ = 0;
    if(page_count < 4096 * 8){
      page_start_addr_ = pmem_addr_ + 4096;
    }else{
      page_start_addr_ =
          pmem_addr_ +
          (page_count % 8 == 0 ? page_count / 8 : (page_count + 8) / 8);
    }
    bitmap_ = (Bitmap*)pmem_addr;
    bitmap_->nums_ = page_count_;
  }
  char* getNewPage() {
    bitmap_->getEmpty(last_empty_);
    bitmap_->set(last_empty_);
    used_count_++;
    char* ret = page_start_addr_ + last_empty_ * page_size_;
    last_empty_ = (last_empty_ + 1) % bitmap_->nums_;
    return ret;
  }
  void freePage(char* page_addr) {
    bitmap_->clr((page_addr - page_start_addr_) / page_size_);
    used_count_--;
  }
  bool isFull() { return used_count_ == page_count_; }
  ~PMExtent() {delete bitmap_;}

 private:
  uint64_t extent_id_;
  uint64_t page_count_;
  uint64_t page_size_;
  uint64_t used_count_;
  Bitmap* bitmap_;
  size_t last_empty_;
  char* pmem_addr_;// extent起始地址
  char* page_start_addr_;// extent起始地址 + bitmap地址
};

class PMMemAllocator {
 public:
  PMMemAllocator(const Options& options_);
  ~PMMemAllocator();
  const void* PmAlloc(size_t pm_len);
  void* mallocPage(PageType type);
  void freePage(char* addr, PageType type);
  uint64_t GetKpageSize() { return kPage_size_; }
  uint64_t GetVpageSize() { return vPage_size_; }
  void Sync();
  const Options& options_;
 private:
  PMExtent* NewExtent(PageType type);
  uint64_t SuitablePageSize(uint64_t page_size);
  uint64_t new_extent_id_;
  std::vector<PMExtent*> Kpage_;
  std::vector<PMExtent*> Vpage_;
  PMExtent* pages[8];
  uint64_t kPage_slot_count_;
  uint64_t kPage_size_;
  uint64_t kPage_count_;
  uint64_t vPage_slot_count_;
  uint64_t vPage_size_;
  uint64_t vPage_count_;

  size_t mapped_len_;
  int is_pmem_;
};

}  // namespace leveldb
#endif
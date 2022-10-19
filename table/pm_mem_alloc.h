
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "leveldb/options.h"
#include "leveldb/slice.h"

#include "bitmap.h"
#include "libpmem.h"
#include "table_meta.h"

namespace leveldb {

enum ExtentType {
  key_t,
  value_t
}

class PMExtent {
  PMExtent(uint64_t page_count, uint64_t page_size, uint64_t extent_id) {
    std::string path =
        Options.pm_path + "extent" + std::to_string(extent_id) + ".edb";
    /* create a pmem file and memory map it */
    if ((pmem_addr_ =
             pmem_map_file(path, Options.extent_size_, PMEM_FILE_CREATE, 0666,
                           &mapped_len_, &is_pmem_)) == NULL) {
      perror("pmem_map_file");
      exit(1);
    }
    page_count_ = page_count;
    page_size_ = page_size;
    used_count_ = 0;
    page_start_addr_ =
        pmem_addr_ +
        (page_count % 8 == 0 ? page_count / 8 : (page_count + 8) / 8);
    bitmap_ = new Bitmap(page_count, pmem_addr_);
  }
  void Sync() {
    if (is_pmem_)
      pmem_persist(pmem_addr_, mapped_len_);
    else
      pmem_msync(pmem_addr_, mapped_len_);
  }
  char* getNewPage() {
    return page_start_addr_ + bitmap_->getEmpty() * page_size_;
    used_count_++;
  }
  void freePage(char* page_addr) {
    bitmap_->clr((page_addr - page_start_addr_) / page_size_);
    used_count_--;
  }
  bool isFull() { return used_count_ == page_count_; }
  ~PMExtent();

 private:
  uint64_t extent_id_;
  uint64_t page_count_;
  uint64_t page_size_;
  uint64_t used_count_;
  Bitmap* bitmap_;
  size_t mapped_len_;
  char* pmem_addr_;
  char* page_start_addr_;
  int is_pmem_;
};

class PMMemAllocator {
 public:
  PMMemAllocator();
  ~PMMemAllocator();
  const void* PmAlloc(size_t pm_len);
  char* GetNewPage(ExtentType type);
  uint64_t GetKpageSize() { return kPage_size_; }
  uint64_t GetVpageSize() { return vPage_size_; }

 private:
  PMExtent* NewExtent(ExtentType type);
  uint64_t SuitablePageSize(uint64_t page_size);
  uint64_t new_extent_id_;
  std::vector<PMExtent*> Kpage_;
  std::vector<PMExtent*> Vpage_;
  uint64_t kPage_slot_count_;
  uint64_t kPage_size_;
  uint64_t kPage_count_;
  uint64_t vPage_slot_count_;
  uint64_t vPage_size_;
  uint64_t vPage_count_;
};

}  // namespace leveldb
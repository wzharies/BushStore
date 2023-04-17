#ifndef STORAGE_LEVELDB_BPLUSTREE_BP_ITERATOR_H_
#define STORAGE_LEVELDB_BPLUSTREE_BP_ITERATOR_H_

#include <vector>

#include "table/pm_mem_alloc.h"

#include "bplustree/bptree.h"
namespace leveldb {
class BP_Iterator : public Iterator {
 public:
  BP_Iterator();
  BP_Iterator(PMMemAllocator* alloc, lbtree* tree1, std::vector<void*>& pages,
              int start_pos, int kpage_count, bool autoClearVpage = false, int start_index_page = 0)
      : alloc_(alloc),
        tree1_(tree1),
        pages_(pages),
        pos_index_(start_pos),
        kpage_count_(kpage_count),
        autoClearVpage(autoClearVpage),
        cur_index_page_(start_index_page) {
    index_page_ = (bnode*)pages_[cur_index_page_];
    kpage_ = (kPage*)index_page_->ch(pos_index_);
    pos_data_ = 0;
    valid_ = kpage_count_ == 0 ? false : true;
    kPages_.reserve(kpage_count);
    kPages_.push_back(kpage_);
  }

  void setKeyStartAndEnd(Slice startk, Slice endk,
                         const Comparator* comparator) {
    startKey = startk;
    endKey = endk;
    comparator_ = comparator;
    needTrim = true;
  }
  void setKeyStartAndEnd(key_type startk, key_type endk,
                         const Comparator* comparator) {
    EncodeFixed64Reverse(cstartKey, startk);
    EncodeFixed64Reverse(cendKey, endk);
    startKey = Slice(cstartKey, 8);
    endKey = Slice(cendKey, 8);
    comparator_ = comparator;
    needTrim = true;
  }

  void setStart(key_type startk){
    EncodeFixed64Reverse(cstartKey, startk);
    startKey = Slice(cstartKey, 8);
  }

  void setEnd(key_type endk){
    EncodeFixed64Reverse(cendKey, endk);
    endKey = Slice(cendKey, 8);
  }

  ~BP_Iterator() override {
    // releaseKpage();
  }

  void releaseKpage() {
    // std::cout<< "free kPage from : " <<
    // ((kPage*)kPages_.front())->minRawKey() <<" to :" <<
    // ((kPage*)kPages_.back())->maxRawKey() <<std::endl;
    if (kPages_.size() == 1 && !skipKpage) {
      // 可能性很小，懒得写了
      ((kPage*)kPages_[0])->remove(start_pos_index_, pos_data_);
      if (((kPage*)kPages_[0])->nums == 0) {
        alloc_->freePage((char*)kPages_[0], key_t);
      }
      return;
    }
    if (kPages_.size() > 1) {
      if (skipKpage == 0) {
        ((kPage*)kPages_[0])->nums = start_pos_index_;
        if (((kPage*)kPages_[0])->nums == 0) {
          alloc_->freePage((char*)kPages_[0], key_t);
        }
      } else if (skipKpage == 1 && start_pos_index_ == 0) {
        // do nothing
      } else {
        std::cout << "fault case" << std::endl;
        assert(false);
      }
    }
    for (int i = 1; i < kPages_.size() - 1; i++) {
      alloc_->freePage((char*)kPages_[i], key_t);
    }
    if (kPages_.size() > 1) {
      ((kPage*)kPages_.back())->remove(0, pos_data_);
      if (((kPage*)kPages_.back())->nums == 0) {
        alloc_->freePage((char*)kPages_.back(), key_t);
      }
    }
  }

  bool Valid() const override {
    return valid_ && (!needTrim || endKey.empty() ||
                      comparator_->Compare(key(), endKey) < 0);
  }

  void SeekToFirst() override {
    if (needTrim) {
      while (Valid()) {
        if (startKey.empty() || comparator_->Compare(key(), startKey) > 0) {
          break;
        }
        NextInternal();
      }
    }
    start_pos_index_ = pos_data_;
    skipKpage = kPages_.size() - 1;
  };

  void SeekToLast() override{};

  void Seek(const Slice& target) override {}

  void NextInternal() {
    pos_data_++;
    // 超过kpage索引范围
    if (pos_data_ == kpage_->nums) {
      // 超过bnode的索引范围
      kpage_count_--;
      if (pos_index_ < index_page_->num()) {
        pos_index_++;
      } else {
        cur_index_page_++;
        if (cur_index_page_ >= pages_.size()) {
          valid_ = false;
          return;
        }
        index_page_ = (bnode*)pages_[cur_index_page_];
        pos_index_ = 1;
      }
      if (kpage_count_ == 0) {
        valid_ = false;
        return;
      }
      kpage_ = (kPage*)index_page_->ch(pos_index_);
      kPages_.push_back(kpage_);
      pos_data_ = 0;
    }
  }

  void Next() override {
    if (autoClearVpage) {
      clrValue();
    }
    NextInternal();
  }

  void Prev() override {}

  Slice key() const override { return kpage_->k(pos_data_); }

  uint16_t finger() { return kpage_->finger[pos_data_]; }

  uint32_t pointer() const { return kpage_->pointer[pos_data_]; }

  uint16_t index() const { return kpage_->index[pos_data_]; }

  Slice value() const override {
    // TODO 也许可以优化？
    vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    return addr->v(index());
  }

  void clrValue() {
    vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    addr->clrBitMap(index());
    // addr->bitmap = addr->bitmap & (~(1ULL << index()));
    if (addr->nums() == 0) {
      // TODO vPage需要删除,最好把pmALloc设置为全局变量或者单例
      //  std::cout<< "free : " << addr <<std::endl;
      alloc_->freePage((char*)addr, value_t);
    }
  }

  Status status() const override { return Status::OK(); }
  void movekPageTolbtree() { tree1_->needFreeKPages = kPages_; }

 private:
  PMMemAllocator* alloc_;
  lbtree* tree1_;
  std::vector<void*>& pages_;
  std::vector<void*> kPages_;
  int kpage_count_;  // 几个kpage，即bnode中kv的数量

  int cur_index_page_;  // 第几个bnode
  bnode* index_page_;
  kPage* kpage_;
  int pos_index_;  // bnode的索引
  int pos_data_;   // kpage内部的索引
  bool valid_ = false;
  bool autoClearVpage = false;
  // 闭区间，由于getOverlap的精度只有bnode级别，但是但是多出的kpage不能在迭代器中生效，需要头尾做裁剪。
  bool needTrim = false;
  const Comparator* comparator_;
  Slice startKey;
  Slice endKey;
  char cstartKey[12];
  char cendKey[12];
  int start_pos_index_ = -1;
  int skipKpage = 0;
  int end_pos_index_ = -1;
};

}  // namespace leveldb

#endif
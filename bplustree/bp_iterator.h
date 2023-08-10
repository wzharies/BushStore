#ifndef STORAGE_LEVELDB_BPLUSTREE_BP_ITERATOR_H_
#define STORAGE_LEVELDB_BPLUSTREE_BP_ITERATOR_H_

#include <vector>

#include "table/pm_mem_alloc.h"

#include "bplustree/bptree.h"
#include "include/leveldb/iterator.h"
namespace leveldb {

class IteratorBTree : public Iterator{
 public:
  IteratorBTree() {}
  IteratorBTree(const Iterator&) = delete;
  IteratorBTree& operator=(const Iterator&) = delete;
  virtual ~IteratorBTree() {}
  virtual bool Valid() const = 0;
  virtual void SeekToFirst() = 0;
  virtual void SeekToLast() = 0;
  virtual void Seek(const Slice& target) = 0;
  virtual void Next() = 0;
  virtual void Prev() = 0;
  virtual uint16_t finger() const = 0;
  virtual uint32_t pointer() const = 0;
  virtual uint16_t index() const = 0;
  virtual Slice key() const = 0;
  virtual Slice value() const = 0;
  virtual double getCapacityUsage() = 0;
  virtual void clrValue() = 0;
  virtual void movePage() = 0;
  virtual Status status() const = 0;
};

class BP_Iterator_Read : public IteratorBTree{
public:
  BP_Iterator_Read(std::shared_ptr<lbtree> tree, PMMemAllocator* alloc = nullptr, bool readOnly = true, std::mutex *mutex = nullptr) : alloc_(alloc), readOnly_(readOnly), tree_(tree), mutex_(mutex) { }
  ~BP_Iterator_Read() {
    if(mutex_ != nullptr){
      mutex_->unlock();
    }
  }

  // An iterator is either positioned at a key/value pair, or
  // not valid.  This method returns true iff the iterator is valid.
  bool Valid() const override {
    return valid;
  }

  // Position at the first key in the source.  The iterator is Valid()
  // after this call iff the source is not empty.
  void SeekToFirst() override {
    if(mutex_ != nullptr){
      mutex_->lock();
    }
    key_type key = tree_->tree_meta->min_key;
    curPage_ = tree_->getKpage(key, true);
    if(curPage_ == nullptr){
      valid = false;
      return ;
    }
    if (!readOnly_) {
      needFreeKPages_.push_back((char*)curPage_);
    }
    valid = true;
    while(Valid()){
      if(rawKey() >= key){
        break;
      }
      Next();
    }
  }

  // Position at the last key in the source.  The iterator is
  // Valid() after this call iff the source is not empty.
  void SeekToLast() override {
    printf("SeekToLast is unsupported.\n");
    assert(false);
  }

  // Position at the first key in the source that is at or past target.
  // The iterator is Valid() after this call iff the source contains
  // an entry that comes at or past target.
  void Seek(const Slice& target) override {
    if(mutex_ != nullptr){
      mutex_->lock();
    }
    key_type key = DecodeDBBenchFixed64(target.data());
    curPage_ = tree_->getKpage(key, true);
    if(curPage_ == nullptr){
      valid = false;
      return ;
    }
    if (!readOnly_) {
      needFreeKPages_.push_back((char*)curPage_);
    }
    while(Valid()){
      if(rawKey() >= key){
        break;
      }
      Next();
    }
    valid = true;
  }

  // Moves to the next entry in the source.  After this call, Valid() is
  // true iff the iterator was not positioned at the last entry in the source.
  // REQUIRES: Valid()
  void Next() override {
    curIndex++;
    if(curIndex >= curPage_->nums){
      curPage_ = curPage_->nextPage();
      curIndex = 0;
      if(curPage_ == nullptr){
        valid = false;
        return ;
      }
      if(!readOnly_){
        needFreeKPages_.push_back((char*)curPage_);
      }
    }
  }

  // Moves to the previous entry in the source.  After this call, Valid() is
  // true iff the iterator was not positioned at the first entry in source.
  // REQUIRES: Valid()
  void Prev() override {

  }
  // Return the key for the current entry.  The underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // REQUIRES: Valid()
  Slice key()  const override {
    return curPage_->k(curIndex);
  }

  key_type rawKey(){
    return curPage_->rawK(curIndex);
  }

  uint16_t finger() const override { return curPage_->finger[curIndex]; }

  uint32_t pointer() const override { return curPage_->pointer[curIndex]; }

  uint16_t index() const override { return curPage_->index[curIndex]; }
  // Return the value for the current entry.  The underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // REQUIRES: Valid()
  Slice value() const override {
    vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    return addr->v(index());
  }

  double getCapacityUsage() override {
    vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    return addr->getCapacityUsage();
  }

  void clrValue() override {
    vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    addr->clrBitMap(index());
    // addr->bitmap = addr->bitmap & (~(1ULL << index()));
    if (addr->nums() == 0) {
            // std::cout<< "free : " << addr <<std::endl;
      // needFreeVPgaes_.push_back((char*)addr);
      alloc_->freePage((char*)addr, value_t);
    }
  }

  void movePage() override {
    tree_->needFreeKPages = needFreeKPages_;
    tree_->needFreeVPages = needFreeVPgaes_;
  }

  // If an error has occurred, return it.  Else return an ok status.
  Status status() const override {
    return Status::OK();
  }
private:
  PMMemAllocator* alloc_;
  bool readOnly_ = true;
  kPage* curPage_ = nullptr;
  std::vector<char*> needFreeKPages_;
  std::vector<char*> needFreeVPgaes_;
  int curIndex = 0;
  bool valid = false;
  std::shared_ptr<lbtree> tree_;
  std::mutex *mutex_ = nullptr;
};
class BP_Iterator_Trim : public IteratorBTree {
 public:
  BP_Iterator_Trim();
  BP_Iterator_Trim(PMMemAllocator* alloc, lbtree* tree1, std::vector<void*>& pages, int start_index_page = 0, bool autoClearVpage = false)
      : alloc_(alloc),
        tree1_(tree1),
        pages_(pages),
        pos_index_(1),
        // kpage_count_(kpage_count),
        autoClearVpage(autoClearVpage),
        cur_index_page_(start_index_page) {
  }

  ~BP_Iterator_Trim() override {
    // releaseKpage();
  }

    void setKeyStartAndEnd(Slice startk, Slice endk,
                         const Comparator* comparator) {
    startKey = startk;
    endKey = endk;
    rStartKey = startk.empty() ? 0 : DecodeDBBenchFixed64(startk.data());
    rEndKey = endk.empty() ? UINT64_MAX : DecodeDBBenchFixed64(endk.data());
    comparator_ = comparator;
    needTrim = true;
  }
  void setKeyStartAndEnd(key_type startk, key_type endk,
                         const Comparator* comparator) {
    rStartKey = startk;
    rEndKey = endk;
    EncodeFixed64Reverse(cstartKey, startk);
    EncodeFixed64Reverse(cendKey, endk);
    startKey = Slice(cstartKey, 8);
    endKey = Slice(cendKey, 8);
    comparator_ = comparator;
    needTrim = true;
  }

  void setStart(key_type startk){
    rStartKey = startk;
    EncodeFixed64Reverse(cstartKey, startk);
    startKey = Slice(cstartKey, 8);
  }

  void setEndEmpty(){
    endKey = Slice();
  }
  void setEnd(key_type endk){
    rEndKey = endk;
    EncodeFixed64Reverse(cendKey, endk);
    endKey = Slice(cendKey, 8);
  }

  void releaseKVpage() {
    // std::cout<< "free kPage from : " <<
    // ((kPage*)kPages_.front())->minRawKey() <<" to :" <<
    // ((kPage*)kPages_.back())->maxRawKey() <<std::endl;
    if (kPages_.size() == 1 && !skipKpage) {
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

    for(int i = 0; i < vPages_.size(); i++){
      alloc_->freePage(vPages_[i], value_t);
    }
  }

  bool Valid() const override {
    return valid_ && (!needTrim || endKey.empty() ||
                      comparator_->Compare(key(), endKey) < 0);
  }

  void SeekToFirst() override {
    index_page_ = (bnode*)pages_[cur_index_page_];
    kpage_ = (kPage*)index_page_->ch(pos_index_);
    while(pos_index_ + 1 <= index_page_->num()){
      if(((kPage*)index_page_->ch(pos_index_ + 1))->minRawKey() > rStartKey){
        kpage_ = (kPage*)index_page_->ch(pos_index_);
        break;
      }
      pos_index_++;
    }
    if(pos_index_ == index_page_->num()){
      kpage_ = (kPage*)index_page_->ch(pos_index_);
    }
    pos_data_ = 0;
    valid_ = true;
    // valid_ = kpage_count_ == 0 ? false : true;
    // kPages_.reserve(kpage_count);
    kPages_.push_back((char*)kpage_);
    if (needTrim) {
      while (Valid()) {
        // TODO ==?
        if (startKey.empty() || comparator_->Compare(key(), startKey) >= 0) {
          break;
        }
        NextInternal();
      }
    }
    start_pos_index_ = pos_data_;
    skipKpage = kPages_.size() - 1;
    assert(skipKpage == 0 || skipKpage == 1);
  };

  void SeekToLast() override{};

  void Seek(const Slice& target) override {}

  void NextInternal() {
    pos_data_++;
        if (pos_data_ == kpage_->nums) {
            // kpage_count_--;
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
      // if (kpage_count_ == 0) {
      //   valid_ = false;
      //   return;
      // }
      kpage_ = (kPage*)index_page_->ch(pos_index_);
      kPages_.push_back((char*)kpage_);
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

  uint16_t finger() const override { return kpage_->finger[pos_data_]; }

  uint32_t pointer() const override { return kpage_->pointer[pos_data_]; }

  uint16_t index() const override { return kpage_->index[pos_data_]; }

  Slice value() const override {
        vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    return addr->v(index());
  }

  double getCapacityUsage() override {
    vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    return addr->getCapacityUsage();
  }

  void clrValue() override {
    vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    addr->clrBitMap(index());
    // addr->bitmap = addr->bitmap & (~(1ULL << index()));
    if (addr->nums() == 0) {
            //  std::cout<< "free : " << addr <<std::endl;
      vPages_.push_back((char*)addr);
      // alloc_->freePage((char*)addr, value_t);
    }
  }

  Status status() const override { return Status::OK(); }
  void movePage() override {
    tree1_->needFreeKPages = kPages_; 
    tree1_->needFreeVPages = vPages_;
  }

 private:
  PMMemAllocator* alloc_;
  lbtree* tree1_;
  std::vector<void*>& pages_;
  std::vector<char*> kPages_;
  std::vector<char*> vPages_;
  // int kpage_count_;  
  int cur_index_page_;    bnode* index_page_;
  kPage* kpage_;
  int pos_index_;    int pos_data_;     bool valid_ = false;
  bool autoClearVpage = false;
    bool needTrim = false;
  const Comparator* comparator_;
  Slice startKey;
  Slice endKey;
  char cstartKey[12];
  char cendKey[12];
  key_type rStartKey;
  key_type rEndKey;
  int start_pos_index_ = -1;
  int skipKpage = 0;
  int end_pos_index_ = -1;
};

class BP_Iterator : public IteratorBTree {
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
    kPages_.push_back((char*)kpage_);
  }

  void setKeyStartAndEnd(Slice startk, Slice endk,
                         const Comparator* comparator) {
    startKey = startk;
    endKey = endk;
    rStartKey = startk.empty() ? 0 : DecodeDBBenchFixed64(startk.data());
    rEndKey = endk.empty() ? UINT64_MAX : DecodeDBBenchFixed64(endk.data());
    comparator_ = comparator;
    needTrim = true;
  }
  void setKeyStartAndEnd(key_type startk, key_type endk,
                         const Comparator* comparator) {
    rStartKey = startk;
    rEndKey = endk;
    EncodeFixed64Reverse(cstartKey, startk);
    EncodeFixed64Reverse(cendKey, endk);
    startKey = Slice(cstartKey, 8);
    endKey = Slice(cendKey, 8);
    comparator_ = comparator;
    needTrim = true;
  }

  void setStart(key_type startk){
    rStartKey = startk;
    EncodeFixed64Reverse(cstartKey, startk);
    startKey = Slice(cstartKey, 8);
  }

  void setEnd(key_type endk){
    rEndKey = endk;
    EncodeFixed64Reverse(cendKey, endk);
    endKey = Slice(cendKey, 8);
  }

  ~BP_Iterator() override {
    // releaseKpage();
  }

  void releaseKVpage() {
    // std::cout<< "free kPage from : " <<
    // ((kPage*)kPages_.front())->minRawKey() <<" to :" <<
    // ((kPage*)kPages_.back())->maxRawKey() <<std::endl;
    if (kPages_.size() == 1 && !skipKpage) {
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
    
    for(int i = 0; i < vPages_.size(); i++){
      alloc_->freePage(vPages_[i], value_t);
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
        if (pos_data_ == kpage_->nums) {
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
      kPages_.push_back((char*)kpage_);
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

  uint16_t finger() const override { return kpage_->finger[pos_data_]; }

  uint32_t pointer() const override { return kpage_->pointer[pos_data_]; }

  uint16_t index() const override { return kpage_->index[pos_data_]; }

  Slice value() const override {
        vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    return addr->v(index());
  }

  double getCapacityUsage() override {
    vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    return addr->getCapacityUsage();
  }

  void clrValue() override {
    vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
    addr->clrBitMap(index());
    // addr->bitmap = addr->bitmap & (~(1ULL << index()));
    if (addr->nums() == 0) {
      
      //  std::cout<< "free : " << addr <<std::endl;
      alloc_->freePage((char*)addr, value_t);
      // vPages_.push_back((char*)addr);
    }
  }

  Status status() const override { return Status::OK(); }

  void movePage() override {
    tree1_->needFreeKPages = kPages_;
    tree1_->needFreeVPages = vPages_;
  }

 private:
  PMMemAllocator* alloc_;
  lbtree* tree1_;
  std::vector<void*>& pages_;
  std::vector<char*> kPages_;
  std::vector<char*> vPages_;
  int kpage_count_;  
  int cur_index_page_;    bnode* index_page_;
  kPage* kpage_;
  int pos_index_;    int pos_data_;     bool valid_ = false;
  bool autoClearVpage = false;
    bool needTrim = false;
  const Comparator* comparator_;
  Slice startKey;
  Slice endKey;
  char cstartKey[12];
  char cendKey[12];
  key_type rStartKey;
  key_type rEndKey;
  int start_pos_index_ = -1;
  int skipKpage = 0;
  int end_pos_index_ = -1;
};

}  // namespace leveldb

#endif
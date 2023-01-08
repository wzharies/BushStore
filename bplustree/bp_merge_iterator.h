#ifndef STORAGE_LEVELDB_BPLUSTREE_BP_MERGE_ITERATOR_H_
#define STORAGE_LEVELDB_BPLUSTREE_BP_MERGE_ITERATOR_H_
#include <vector>
#include "bptree.h"
#include "bp_iterator.h"
namespace leveldb{
class BP_Merge_Iterator{
public:
    BP_Merge_Iterator();
    BP_Merge_Iterator(std::vector<BP_Iterator*>& its, const Comparator* comparator) : its_(its), comparator_(comparator){
        FindSmallest();
    }
    ~BP_Merge_Iterator(){
        
    }

    bool Valid(){ return (current_ != nullptr);}

    void SeekToFirst(){};

    void SeekToLast(){};

    void Seek(key_type key){}

    void Next(){
        current_->Next();
        FindSmallest();
    }

    void Prev(){

    }

    Slice key(){
        return current_->key();
    }

    unsigned char finger(){
        return current_->finger();
    }

    uint32_t pointer(){
        return current_->pointer();
    }

    unsigned char index(){
        return current_->index();
    }

    Slice value(){
        return current_->value();
    }

    void clrValue(){
        current_->clrValue();
    }

    Status status(){
        return Status::OK();
    }
private:
    void FindSmallest(){
        BP_Iterator* smallest = nullptr;
        for (int i = 0; i < its_.size(); i++) {
            BP_Iterator* child = its_[i];
            if (child->Valid()) {
                if (smallest == nullptr) {
                    smallest = child;
                } else if (comparator_->Compare(child->key(), smallest->key()) < 0) {
                    smallest = child;
                }
            }
        }
        current_ = smallest;
    }
    std::vector<BP_Iterator*>& its_;
    const Comparator* comparator_;
    BP_Iterator* current_;
};

} // namespace leveldb
#endif
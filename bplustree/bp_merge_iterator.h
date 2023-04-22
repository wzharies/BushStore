#ifndef STORAGE_LEVELDB_BPLUSTREE_BP_MERGE_ITERATOR_H_
#define STORAGE_LEVELDB_BPLUSTREE_BP_MERGE_ITERATOR_H_
#include <vector>
#include "bptree.h"
#include "bp_iterator.h"
namespace leveldb{
class BP_Merge_Iterator{
public:
    BP_Merge_Iterator();
    BP_Merge_Iterator(std::vector<IteratorBTree*>& its, const Comparator* comparator) : its_(its), comparator_(comparator){
    }
    ~BP_Merge_Iterator(){
        
    }

    bool Valid(){ return (current_ != nullptr);}

    void SeekToFirst(){
        for(auto& it : its_){
            it->SeekToFirst();
        }
        FindSmallest();
    };

    void SeekToLast(){};

    void Seek(){}

    void Next(){
        current_->Next();
        FindSmallest();
    }

    void Prev(){

    }

    Slice key(){
        return current_->key();
    }

    uint16_t finger(){
        return current_->finger();
    }

    uint32_t pointer(){
        return current_->pointer();
    }

    uint16_t index(){
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
        IteratorBTree* smallest = nullptr;
        for (int i = 0; i < its_.size(); i++) {
            IteratorBTree* child = its_[i];
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
    std::vector<IteratorBTree*>& its_;
    const Comparator* comparator_;
    IteratorBTree* current_;
};

} // namespace leveldb
#endif
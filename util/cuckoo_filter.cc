

#include "util/cuckoo_filter.h"
#include "db/dbformat.h"


namespace leveldb{
#include "cuckoo_filter.h"
uint64_t MurmurHash64A (const void * key, int len, unsigned int seed = 0x20220601)
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t * data = (const uint64_t *)key;
    const uint64_t * end = data + (len/8);

    while (data != end)
    {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char * data2 = (const unsigned char*)data;

    switch (len & 7)
    {
        case 7: h ^= uint64_t(data2[6]) << 48;
        case 6: h ^= uint64_t(data2[5]) << 40;
        case 5: h ^= uint64_t(data2[4]) << 32;
        case 4: h ^= uint64_t(data2[3]) << 24;
        case 3: h ^= uint64_t(data2[2]) << 16;
        case 2: h ^= uint64_t(data2[1]) << 8;
        case 1: h ^= uint64_t(data2[0]);
            h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}
inline size_t IndexHash(uint32_t bucket_num, uint32_t hv) {
    // table_->num_buckets is always a power of two, so modulo can be replaced
    // with
    // bitwise-and:
    return hv & (bucket_num - 1);
}

inline uint32_t TagHash(uint32_t hv)  {
    uint32_t tag;
    tag = hv & ((1ULL << (TAG_SIZE * 8)) - 1);
    tag += (tag == 0);
    return tag;
}

void CuckooFilter::GenerateIndexTagHash(Slice key, size_t *index1, size_t *index2, uint32_t *tag) {
    const uint64_t hash = MurmurHash64A(key.data(), key.size());
    //hash值的一半给tag，另一个给index
    *index1 = IndexHash(bucket_num_, hash >> 32);
    *tag = TagHash(hash);
    *index2 = IndexHash(bucket_num_, (uint32_t)(*index1 ^ ((*tag) * 0x5bd1e995)));
    if(rd_() % 2 == 0){
        std::swap(*index1, *index2);
    }
    if(*index1 != IndexHash(bucket_num_, (uint32_t)(*index2 ^ ((*tag) * 0x5bd1e995)))){
        printf("index1 != index2\n");
    }
}

CuckooFilter::CuckooFilter(uint32_t bucket_num){
    bucket_num_ = bucket_num;
    slots_ = static_cast<cuckoo_slot *>(calloc(ASSOC_WAY * bucket_num, sizeof(struct cuckoo_slot)));
    buckets_ = static_cast<cuckoo_slot **>(malloc(bucket_num * sizeof(struct cuckoo_slot *)));
    for(int i = 0; i < bucket_num; i++){
        buckets_[i] = &slots_[i * ASSOC_WAY];
    }
}

void CuckooFilter::Get(Slice key, uint32_t* value){
    uint32_t tag;
    size_t index1, index2;
    int i;
    GenerateIndexTagHash(key, &index1, &index2, &tag);
    uint32_t max_value = 0;
    struct cuckoo_slot* bucket = buckets_[index1];
    for(i = 0; i < ASSOC_WAY; i++){
        uint32_t probeTag = bucket[i].tag.load(std::memory_order_relaxed);
        if(probeTag == tag){
            max_value = std::max(max_value, bucket[i].lid.load(std::memory_order_relaxed));
        }
    }
    if(i == ASSOC_WAY){
        bucket = buckets_[index2];
        for(i = 0; i < ASSOC_WAY; i++){
            uint32_t probeTag = bucket[i].tag.load(std::memory_order_relaxed);
            if(probeTag == tag){
                max_value = std::max(max_value, bucket[i].lid.load(std::memory_order_relaxed));
            }
        }
    }
    *value = max_value;
}
void CuckooFilter::Put(Slice key, uint32_t value){
    uint32_t tag;
    size_t index1, index2;
    int i;
    uint32_t empty_key = 0;
    GenerateIndexTagHash(key, &index1, &index2, &tag);

    struct cuckoo_slot* bucket = buckets_[index1];
    for(i = 0; i < ASSOC_WAY; i++){
        uint32_t probeTag = bucket[i].tag.load(std::memory_order_relaxed);
        uint32_t probeLid = bucket[i].lid.load(std::memory_order_relaxed);
        if(probeTag == 0 && probeLid == 0){
            if(bucket[i].tag.compare_exchange_strong(empty_key,tag,std::memory_order_relaxed)){
                bucket[i].lid.store(value,std::memory_order_relaxed);
            }
            printf("put %u %u in index: %zu pick: %d\n", tag, value, index1,i);
            return ;
        }
    }
    if(i == ASSOC_WAY){
        bucket = buckets_[index2];
        for(i = 0; i < ASSOC_WAY; i++){
            uint32_t probeTag = bucket[i].tag.load(std::memory_order_relaxed);
            uint32_t probeLid = bucket[i].lid.load(std::memory_order_relaxed);
            if(probeTag == 0 && probeLid == 0){
                if(bucket[i].tag.compare_exchange_strong(empty_key,tag,std::memory_order_relaxed)){
                    bucket[i].lid.store(value,std::memory_order_relaxed);
                }
                printf("put %u %u in index: %zu pick: %d\n", tag, value, index1,i);
                return ;
            }
        }
        if(i == ASSOC_WAY){
            printf("need kick\n");
            uint32_t kick_tag(tag);
            uint32_t kick_value(value);
            size_t kick_index = index2;
            for(int j = 0; j < MAX_KICK; j++){
                size_t pick = rd_() % ASSOC_WAY;
                kick_tag = bucket[pick].tag.exchange(kick_tag,std::memory_order_relaxed);
                kick_value = bucket[pick].lid.exchange(kick_value,std::memory_order_relaxed);
                printf("kick out %d : %d from index :%zu pick :%zu\n",kick_tag, kick_value, kick_index, pick);
                kick_index = IndexHash(bucket_num_, (uint32_t)(kick_index ^ (kick_tag * 0x5bd1e995)));
                bucket = buckets_[kick_index];
                for(int k = 0; k < ASSOC_WAY; k++){
                    uint32_t probeTag = bucket[k].tag.load(std::memory_order_relaxed);
                    uint32_t probeLid = bucket[k].lid.load(std::memory_order_relaxed);
                    if(probeTag == 0 && probeLid == 0){
                        if(bucket[k].tag.compare_exchange_strong(empty_key,kick_tag,std::memory_order_relaxed)){
                            bucket[k].lid.store(kick_value,std::memory_order_relaxed);
                        }
                        printf("kick %d times, put index :%zu pick: %d\n", j+1,kick_index, k);
                        printf("put %u %u\n", tag, value);
                        return ;
                    }
                }
            }
            printf("MAX KICK!!!\n");
        }
    }
}
void CuckooFilter::Delete(Slice key){
    uint32_t tag;
    size_t index1, index2;
    int i;
    GenerateIndexTagHash(key, &index1, &index2, &tag);

    struct cuckoo_slot* bucket = buckets_[index1];
    for(i = 0; i < ASSOC_WAY; i++){
        uint32_t probeTag = bucket[i].tag.load(std::memory_order_relaxed);
        if(probeTag == tag){
            if(bucket[i].tag.compare_exchange_strong(tag,0,std::memory_order_relaxed)){
                bucket[i].lid.store(0,std::memory_order_relaxed);
            }
            return ;
        }
    }
    if(i == ASSOC_WAY){
        bucket = buckets_[index2];
        for(i = 0; i < ASSOC_WAY; i++){
            uint32_t probeTag = bucket[i].tag.load(std::memory_order_relaxed);
            if(probeTag == tag){
                if(bucket[i].tag.compare_exchange_strong(tag,0,std::memory_order_relaxed)){
                    bucket[i].lid.store(0,std::memory_order_relaxed);
                }
                return ;
            }
        }
        if(i == ASSOC_WAY){
            printf("don't find it\n");
        }
    }
}
void CuckooFilter::Delete(Slice key, uint32_t value){
    uint32_t tag;
    size_t index1, index2;
    int i;
    GenerateIndexTagHash(key, &index1, &index2, &tag);

    struct cuckoo_slot* bucket = buckets_[index1];
    for(i = 0; i < ASSOC_WAY; i++){
        uint32_t probeTag = bucket[i].tag.load(std::memory_order_relaxed);
        uint32_t probeLid = bucket[i].lid.load(std::memory_order_relaxed);
        if(probeTag == tag && probeLid == value){
            if(bucket[i].tag.compare_exchange_strong(tag,0,std::memory_order_relaxed)){
                bucket[i].lid.store(0,std::memory_order_relaxed);
            }
            return ;
        }
    }
    if(i == ASSOC_WAY){
        bucket = buckets_[index2];
        for(i = 0; i < ASSOC_WAY; i++){
            uint32_t probeTag = bucket[i].tag.load(std::memory_order_relaxed);
            uint32_t probeLid = bucket[i].lid.load(std::memory_order_relaxed);
            if(probeTag == tag && probeLid == value){
                if(bucket[i].tag.compare_exchange_strong(tag,0,std::memory_order_relaxed)){
                    bucket[i].lid.store(0,std::memory_order_relaxed);
                }
                return ;
            }
        }
        if(i == ASSOC_WAY){
            printf("Delete don't find it\n");
        }
    }
}
}
#!/bin/bash
#set -x
KB=$((1024))
MB=$(($KB*1024))
GB=$(($MB*1024))
# APP_PREFIX=sudo
APP_PREFIX="numactl --cpunodebind=1 --membind=1"

db_path=$(pwd)
db_bench=$db_path/build
db_include=$db_path/include
ycsb_path=$db_path/ycsbc

sata_path=/tmp/pm_test
# ssd_path=/tmp/pm_test
ssd_path=/media/nvme/pm_test
pm_path=/mnt/pmem1/pm_test
# leveldb_path=/tmp/leveldb
leveldb_path=$pm_path
output_path=$db_path/output-new13
output_file=$output_path/result.out

export CPLUS_INCLUDE_PATH=$db_path/include:$CPLUS_INCLUDE_PATH
export LIBRARY_PATH=$db_path/build:$LIBRARY_PATH

benchmarks="overwrite,readrandom,readseq,stats"
# benchmarks2="fillseqNofresh,readrandom,readseq,stats"
ycsb_input=1KB_ALL

num_thread=1
value_size=1024
num_kvs=$((10*$MB))
write_buffer_size=$((64*$MB))
max_file_size=$((128*$MB))
pm_size=$((380*$GB))
bucket_nums=$((4*$MB)) # bucket_nums * 4 > nums_kvs
max_open_files=$((50000))
reads=$((-1))
use_pm=1
flush_ssd=0
throughput=0
dynamic_tree=1
write_batch=1
gc_ratio=0.5
group_size=0

WRITE10M-8B() {
    value_size=$((8))
    num_kvs=$((10*$MB))
    write_batch=1;
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}

WRITE400G-1K() {
    value_size=$((1*$KB))
    num_kvs=$((200*$GB / $value_size))
    write_batch=5;
    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
}

WRITE400G-4K() {
    value_size=$((4*$KB))
    num_kvs=$((200*$GB / $value_size))
    write_batch=5;
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}

WRITE400G-16K() {
    value_size=$((16*$KB))
    num_kvs=$((200*$GB / $value_size))
    write_batch=5;
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}

WRITE100M-8B() {
    value_size=$((8))
    num_kvs=$((100*$MB))
    write_batch=1;
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}

WRITE100M-64B() {
    value_size=$((64))
    num_kvs=$((100*$MB))
    write_batch=1;
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}
WRITE100M-256B() {
    value_size=$((256))
    num_kvs=$((100*$MB))
    write_batch=1;
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}


WRITE100M-1KB() {
    value_size=$((1*$KB))
    num_kvs=$((100*$MB))
    write_batch=5;
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}
WRITE100M-4KB() {
    value_size=$((4*$KB))
    num_kvs=$((100*$MB))
    write_batch=5;
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}

WRITE20G() {
    value_size=$((1*$KB))
    num_kvs=$((20*$GB / $value_size))
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}
WRITE80G_FLUSHSSD() {
    leveldb_path=$ssd_path;
    value_size=$((4*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((8*$GB))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}
WRITE80G-1KB-THROUGHPUT() {
    value_size=$((1*$KB))
    num_kvs=$((80*$GB / $value_size))
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
    throughput=1
}
WRITE80G-4KB-THROUGHPUT() {
    value_size=$((4*$KB))
    num_kvs=$((80*$GB / $value_size))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    throughput=1
}
WRITE80G-16KB-THROUGHPUT() {
    value_size=$((16*$KB))
    num_kvs=$((80*$GB / $value_size))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    throughput=1
}
WRITE80G-64KB-THROUGHPUT() {
    value_size=$((64*$KB))
    num_kvs=$((80*$GB / $value_size))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    throughput=1
}
WRITE80G-256B() {
    value_size=$((256))
    num_kvs=$((80*$GB / $value_size))
    bucket_nums=$((128*$MB)) # bucket_nums * 4 > nums_kvs
}
WRITE80G() {
    value_size=$((1*$KB))
    num_kvs=$((80*$GB / $value_size))
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}
WRITE80G-4K() {
    value_size=$((4*$KB))
    num_kvs=$((80*$GB / $value_size))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
}
WRITE80G-16K() {
    value_size=$((16*$KB))
    num_kvs=$((80*$GB / $value_size))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
}
WRITE80G-64K() {
    value_size=$((64*$KB))
    num_kvs=$((80*$GB / $value_size))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
}

WRITE40G-4K() {
    value_size=$((4*$KB))
    num_kvs=$((40*$GB / $value_size))
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}

WRITE40G-1K() {
    value_size=$((1*$KB))
    num_kvs=$((40*$GB / $value_size))
    bucket_nums=$((32*$MB)) # bucket_nums * 4 > nums_kvs
}

WRITE80G_8GNVM() {
    leveldb_path=$ssd_path;
    value_size=$((1*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((8*$GB))
    bucket_nums=$((16*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}
WRITE80G_16GNVM() {
    leveldb_path=$ssd_path;
    value_size=$((1*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((16*$GB))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}
WRITE80G_32GNVM() {
    leveldb_path=$ssd_path;
    value_size=$((1*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((32*$GB))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}
WRITE80G_64GNVM() {
    leveldb_path=$ssd_path;
    value_size=$((1*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((64*$GB))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}

WRITE80G_8GNVM_4K() {
    leveldb_path=$ssd_path;
    value_size=$((4*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((8*$GB))
    bucket_nums=$((16*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}
WRITE80G_16GNVM_4K() {
    leveldb_path=$ssd_path;
    value_size=$((4*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((16*$GB))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}
WRITE80G_32GNVM_4K() {
    leveldb_path=$ssd_path;
    value_size=$((4*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((32*$GB))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}
WRITE80G_64GNVM_4K() {
    leveldb_path=$ssd_path;
    value_size=$((4*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((64*$GB))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}

RUN_DB_BENCH() {
    CLEAN_DB
    output_file=$output_file.txt
    if [ -f "$output_file" ]; then
        rm "$output_file"
        echo "delete output_file: $output_file"
    fi
    parameters="--benchmarks=$benchmarks \
                --num=$num_kvs \
                --value_size=$value_size \
                --write_buffer_size=$write_buffer_size \
                --max_file_size=$max_file_size \
                --open_files=$max_open_files \
                --reads=$reads \
                --pm_size=$pm_size \
                --pm_path=$pm_path \
                --db=$leveldb_path \
                --bucket_nums=$bucket_nums \
                --use_pm=$use_pm \
                --threads=$num_thread \
                --flush_ssd=$flush_ssd \
                --throughput=$throughput \
                --dynamic_tree=$dynamic_tree \
                --write_batch=$write_batch \
                --gc_ratio=$gc_ratio \
                --group_size=$group_size \
                "
    cmd="$APP_PREFIX $db_bench/db_bench $parameters >> $output_file"
    echo "$cmd" >> "$output_file"
    echo "$cmd"
    eval "$cmd"
}

RUN_YCSB(){
    CLEAN_DB
    if [ -f "$output_file" ]; then
        rm "$output_file"
        echo "delete output_file: $output_file"
    fi
    cmd="$APP_PREFIX $ycsb_path/ycsbc $ycsb_path/input/$ycsb_input >> $output_file"
    echo "$cmd" >> "$output_file"
    echo "$cmd"
    eval "$cmd"
}

CLEAN_DB() {
  if [ -z "$pm_path" ]
  then
        echo "PM path empty."
        exit
  fi
  if [ -z "$leveldb_path" ]
  then
        echo "DB path empty."
        exit
  fi
  find ${pm_path:?}  -type f ! -name 'allocator.edb' -delete
  find ${leveldb_path:?}  -type f ! -name 'allocator.edb' -delete
#   rm -rf ${leveldb_path:?}/*
#   rm -rf ${pm_path:?}/*
#   mkdir -p $pm_path
}

CLEAN_ALL_DB() {
  if [ -z "$pm_path" ]
  then
        echo "PM path empty."
        exit
  fi
  if [ -z "$leveldb_path" ]
  then
        echo "DB path empty."
        exit
  fi
  find ${pm_path:?}  -type f ! -name 'allocator.edb' -delete
  find ${leveldb_path:?}  -type f ! -name 'allocator.edb' -delete
#   rm -rf ${leveldb_path:?}/*
#   rm -rf ${pm_path:?}/*
#   mkdir -p $pm_path
}

SET_OUTPUT_PATH() {
    if [ ! -d "$output_path" ]; then
        mkdir "$output_path"
        echo "Created output_path: $output_path"
    fi
    # else
    #     rm -rf "${output_path:?}/"*
    #     echo "Cleared output_path: $output_path"
    # fi
    # touch $output_file
    # echo "Created file: $output_file"
}

MAKE() {
  if [ ! -d "$db_bench" ]; then
    mkdir "$db_bench"
  fi
  cd $db_bench

#   cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
  cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && cmake --build .

  cd ..

  cd "$ycsb_path" || exit
  make clean
  make
  cd ..
}

DB_BENCH_TEST() {
    echo "------------db_bench------------"
    benchmarks="fillrandom,readrandom,readseq,stats"
    echo "------256B random write/read-----"
    output_file=$output_path/Rnd_NVM_256B
    WRITE80G-256B
    RUN_DB_BENCH

    echo "------1KB random write/read-----"
    output_file=$output_path/Rnd_NVM_1K
    WRITE80G
    RUN_DB_BENCH

    echo "------4KB random write/read-----"
    output_file=$output_path/Rnd_NVM_4K
    WRITE80G-4K
    RUN_DB_BENCH

    echo "------16KB random write/read-----"
    output_file=$output_path/Rnd_NVM_16K
    WRITE80G-16K
    RUN_DB_BENCH

    echo "------64KB random write/read-----"
    output_file=$output_path/Rnd_NVM_64K
    WRITE80G-64K
    RUN_DB_BENCH


    benchmarks="fillseq,stats"
    # benchmarks="fillseq,readseq,stats"
    echo "------256B random write/read-----"
    output_file=$output_path/Seq_NVM_256B
    WRITE80G-256B
    RUN_DB_BENCH

    echo "------1KB sequential write/read-----"
    output_file=$output_path/Seq_NVM_1K
    WRITE80G
    RUN_DB_BENCH

    echo "------4KB sequential write/read-----"
    output_file=$output_path/Seq_NVM_4K
    WRITE80G-4K
    RUN_DB_BENCH

    echo "------16KB sequential write/read-----"
    output_file=$output_path/Seq_NVM_16K
    WRITE80G-16K
    RUN_DB_BENCH

    echo "------64KB sequential write/read-----"
    output_file=$output_path/Seq_NVM_64K
    WRITE80G-64K
    RUN_DB_BENCH

    CLEAN_DB
}


DB_BENCH_TEST2() {
    echo "------------db_bench------------"
    benchmarks="fillrandom,readseq,stats"
    echo "------256B random write/read-----"
    output_file=$output_path/Rnd_NVM_256B
    WRITE80G-256B
    RUN_DB_BENCH

    echo "------1KB random write/read-----"
    output_file=$output_path/Rnd_NVM_1K
    WRITE80G
    RUN_DB_BENCH

    echo "------4KB random write/read-----"
    output_file=$output_path/Rnd_NVM_4K
    WRITE80G-4K
    RUN_DB_BENCH

    echo "------16KB random write/read-----"
    output_file=$output_path/Rnd_NVM_16K
    WRITE80G-16K
    RUN_DB_BENCH

    echo "------64KB random write/read-----"
    output_file=$output_path/Rnd_NVM_64K
    WRITE80G-64K
    RUN_DB_BENCH

    CLEAN_DB
}

DB_BENCH_THROUGHPUT() {
    echo "--------db_bench-throughput-------"
    benchmarks="fillrandom,stats"

    echo "------1K random write/read-----"
    output_file=$output_path/Throughput_Rnd_NVM_1KB
    WRITE80G-1KB-THROUGHPUT
    RUN_DB_BENCH

    # echo "------4K random write/read-----"
    # output_file=$output_path/Throughput_Rnd_NVM_4KB
    # WRITE80G-4KB-THROUGHPUT
    # RUN_DB_BENCH
    # throughput=0

    # echo "------16K random write/read-----"
    # output_file=$output_path/Throughput_Rnd_NVM_16KB
    # WRITE80G-16KB-THROUGHPUT
    # RUN_DB_BENCH
    # throughput=0

    # echo "------64K random write/read-----"
    # output_file=$output_path/Throughput_Rnd_NVM_64KB
    # WRITE80G-64KB-THROUGHPUT
    # RUN_DB_BENCH
    # throughput=0

    CLEAN_DB
}

DB_BENCH_TEST_FLUSHSSD() {
    leveldb_path=$ssd_path;
    echo "----------db_bench_flushssd----------"
    APP_PREFIX=""
    benchmarks="fillrandom,readrandom,readseq,stats"
    # benchmarks="fillrandom,readrandom,stats"
    echo "---8GNVM--1KB random write/read---"
    output_file=$output_path/NVM8G_Rnd_1K
    WRITE80G_8GNVM
    RUN_DB_BENCH

    echo "---16GNVM--1KB random write/read---"
    output_file=$output_path/NVM16G_Rnd_1K
    WRITE80G_16GNVM
    RUN_DB_BENCH

    echo "---32GNVM--1KB random write/read---"
    output_file=$output_path/NVM32G_Rnd_1K
    WRITE80G_32GNVM
    RUN_DB_BENCH

    echo "---64GNVM--1KB random write/read---"
    output_file=$output_path/NVM64G_Rnd_1K
    WRITE80G_64GNVM
    RUN_DB_BENCH

    
    benchmarks="fillseq,stats"
    # benchmarks="fillseq,readseq,stats"
    echo "---8GNVM--1KB seq write/read---"
    output_file=$output_path/NVM8G_Seq_1K
    WRITE80G_8GNVM
    RUN_DB_BENCH

    echo "---16GNVM--1KB seq write/read---"
    output_file=$output_path/NVM16G_Seq_1K
    WRITE80G_16GNVM
    RUN_DB_BENCH

    echo "---32GNVM--1KB seq write/read---"
    output_file=$output_path/NVM32G_Seq_1K
    WRITE80G_32GNVM
    RUN_DB_BENCH

    echo "---64GNVM--1KB seq write/read---"
    output_file=$output_path/NVM64G_Seq_1K
    WRITE80G_64GNVM
    RUN_DB_BENCH

    CLEAN_DB
    APP_PREFIX="numactl --cpunodebind=0 --membind=0"
    pm_size=$((180*$GB))
    flush_ssd=0
    leveldb_path=$pm_path;
}

DB_BENCH_TEST_FLUSHSSD2() {
    leveldb_path=$ssd_path;
    echo "----------db_bench_flushssd----------"
    APP_PREFIX=""
    benchmarks="fillrandom,readseq,stats"
    echo "---8GNVM--1KB random write/read---"
    output_file=$output_path/NVM8G_Rnd_1K
    WRITE80G_8GNVM
    RUN_DB_BENCH

    echo "---16GNVM--1KB random write/read---"
    output_file=$output_path/NVM16G_Rnd_1K
    WRITE80G_16GNVM
    RUN_DB_BENCH

    echo "---32GNVM--1KB random write/read---"
    output_file=$output_path/NVM32G_Rnd_1K
    WRITE80G_32GNVM
    RUN_DB_BENCH

    echo "---64GNVM--1KB random write/read---"
    output_file=$output_path/NVM64G_Rnd_1K
    WRITE80G_64GNVM
    RUN_DB_BENCH

    CLEAN_DB
    APP_PREFIX="numactl --cpunodebind=0 --membind=0"
    pm_size=$((180*$GB))
    flush_ssd=0
    leveldb_path=$pm_path;
}

DB_BENCH_TEST_FLUSHSSD_4K() {
    leveldb_path=$ssd_path;
    echo "----------db_bench_flushssd----------"
    APP_PREFIX=""
    benchmarks="fillrandom,readrandom,stats"
    echo "---8GNVM--4KB random write/read---"
    output_file=$output_path/NVM8G_Rnd_4K
    WRITE80G_8GNVM_4K
    RUN_DB_BENCH

    echo "---16GNVM--4KB random write/read---"
    output_file=$output_path/NVM16G_Rnd_4K
    WRITE80G_16GNVM_4K
    RUN_DB_BENCH

    echo "---32GNVM--4KB random write/read---"
    output_file=$output_path/NVM32G_Rnd_4K
    WRITE80G_32GNVM_4K
    RUN_DB_BENCH

    echo "---64GNVM--4KB random write/read---"
    output_file=$output_path/NVM64G_Rnd_4K
    WRITE80G_64GNVM_4K
    RUN_DB_BENCH

    
    benchmarks="fillseq,readseq,stats"
    echo "---8GNVM--4KB seq write/read---"
    output_file=$output_path/NVM8G_Seq_4K
    WRITE80G_8GNVM_4K
    RUN_DB_BENCH

    echo "---16GNVM--4KB seq write/read---"
    output_file=$output_path/NVM16G_Seq_4K
    WRITE80G_16GNVM_4K
    RUN_DB_BENCH

    echo "---32GNVM--4KB seq write/read---"
    output_file=$output_path/NVM32G_Seq_4K
    WRITE80G_32GNVM_4K
    RUN_DB_BENCH

    echo "---64GNVM--4KB seq write/read---"
    output_file=$output_path/NVM64G_Seq_4K
    WRITE80G_64GNVM_4K
    RUN_DB_BENCH

    CLEAN_DB
    APP_PREFIX="numactl --cpunodebind=0 --membind=0"
    pm_size=$((180*$GB))
    flush_ssd=0
    leveldb_path=$pm_path;
}

YCSB_TEST(){
    cd "$ycsb_path" || exit
    echo "------------YCSB------------"
    echo "-----1KB YCSB performance-----"
    output_file=$output_path/YCSB_1KB
    ycsb_input=1KB_ALL
    RUN_YCSB

    echo "------------YCSB------------"
    echo "-----1KB YCSB performance-----"
    output_file=$output_path/YCSB_1KB
    ycsb_input=1KB_ALL
    RUN_YCSB

    echo "-----4KB YCSB performance-----"
    output_file=$output_path/YCSB_4KB
    ycsb_input=4KB_ALL
    RUN_YCSB
    cd ..
}

YCSB_TEST_LATENCY(){
    cd "$ycsb_path" || exit
    echo "------------YCSB------------"
    echo "-----1KB YCSB latency-----"
    output_file=$output_path/YCSB_1KB_Latency1
    ycsb_input=1KB_ALL_Latency
    RUN_YCSB

    echo "------------YCSB------------"
    echo "-----1KB YCSB latency-----"
    output_file=$output_path/YCSB_1KB_Latency2
    ycsb_input=1KB_ALL_Latency
    RUN_YCSB
    # echo "-----4KB YCSB latency-----"
    # output_file=$output_path/YCSB_4KB_Latency
    # ycsb_input=4KB_ALL_Latency
    # RUN_YCSB
    cd ..
}

YCSB_TEST_SSD(){
    APP_PREFIX=""
    cd "$ycsb_path" || exit
    echo "------------YCSB------------"
    echo "-----1KB YCSB SSD-----"
    output_file=$output_path/YCSB_1KB_SSD
    ycsb_input=1KB_ALL_SSD
    RUN_YCSB

    # echo "-----4KB YCSB SSD-----"
    # output_file=$output_path/YCSB_4KB_SSD
    # ycsb_input=4KB_ALL_SSD
    # RUN_YCSB
    cd ..
    APP_PREFIX="numactl --cpunodebind=0 --membind=0"
}

CUCKOO_FILTER_ANALYSIS(){
    echo "------cuckoo filter analysis-------"
    sed -i 's/READ_TIME_ANALYSIS = false/READ_TIME_ANALYSIS = true/g' "$db_path"/util/global.h

    benchmarks="fillseq,fillrandom,readwhilewriting,stats"

    echo "---- with cuckoo filter ----"
    sed -i 's/CUCKOO_FILTER = false/CUCKOO_FILTER = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Cuckoo_Filter_YES_RW_NVM_4K
    WRITE40G-4K
    RUN_DB_BENCH

    echo "---- without cuckoo filter ----"
    sed -i 's/CUCKOO_FILTER = true/CUCKOO_FILTER = false/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Cuckoo_Filter_NO_RW_NVM_4K
    WRITE40G-4K
    RUN_DB_BENCH

    sed -i 's/READ_TIME_ANALYSIS = true/READ_TIME_ANALYSIS = false/g' "$db_path"/util/global.h
    sed -i 's/CUCKOO_FILTER = false/CUCKOO_FILTER = true/g' "$db_path"/util/global.h
    MAKE
}

CUCKOO_FILTER_ANALYSIS2(){
    echo "------cuckoo filter analysis-------"
    sed -i 's/READ_TIME_ANALYSIS = false/READ_TIME_ANALYSIS = true/g' "$db_path"/util/global.h
    sed -i 's/WRITE_TIME_ANALYSIS = false/WRITE_TIME_ANALYSIS = true/g' "$db_path"/util/global.h
    sed -i 's/TEST_CUCKOOFILTER = false/TEST_CUCKOOFILTER = true/g' "$db_path"/util/global.h

    # benchmarks="fillrandom,stats"
    benchmarks="fillrandom,stats,readrandom,stats,readmissing,stats,"

    # echo "---- with immediate delete cuckoo filter without bloom filter----"
    # sed -i 's/TEST_CUCKOO_DELETE = false/TEST_CUCKOO_DELETE = true/g' "$db_path"/util/global.h
    # sed -i 's/TEST_CUCKOOFILTER = true/TEST_CUCKOOFILTER = false/g' "$db_path"/util/global.h
    # MAKE
    # output_file=$output_path/Cuckoo_Filter_Delete_R_NVM_1K
    # WRITE80G
    # RUN_DB_BENCH
    # sed -i 's/TEST_CUCKOO_DELETE = true/TEST_CUCKOO_DELETE = false/g' "$db_path"/util/global.h
    # sed -i 's/TEST_CUCKOOFILTER = false/TEST_CUCKOOFILTER = true/g' "$db_path"/util/global.h
    # MAKE



    # benchmarks="fillrandom,stats,readrandom,stats"
    echo "---- without cuckoo filter without bloom filter----"
    sed -i 's/CUCKOO_FILTER = true/CUCKOO_FILTER = false/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Cuckoo_Filter_NO_BL_NO_RW_NVM_1K
    WRITE80G
    RUN_DB_BENCH

    echo "---- without cuckoo filter with bloom filter----"
    sed -i 's/CUCKOO_FILTER = true/CUCKOO_FILTER = false/g' "$db_path"/util/global.h
    sed -i 's/BLOOM_FILTER = false/BLOOM_FILTER = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Cuckoo_Filter_NO_BL_YES_RW_NVM_1K
    WRITE80G
    RUN_DB_BENCH

    echo "---- with cuckoo filter 32M ----"
    sed -i 's/CUCKOO_FILTER = false/CUCKOO_FILTER = true/g' "$db_path"/util/global.h
    sed -i 's/BLOOM_FILTER = true/BLOOM_FILTER = false/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Cuckoo_Filter_YES_RW_NVM_1K
    WRITE80G
    RUN_DB_BENCH

    # echo "---- with cuckoo filter 16M ----"
    # output_file=$output_path/Cuckoo_Filter_YES_16M_RW_NVM_1K
    # WRITE80G
    # bucket_nums=$((16*$MB))
    # RUN_DB_BENCH

    # echo "---- with cuckoo filter 8MB ----"
    # output_file=$output_path/Cuckoo_Filter_YES_8M_RW_NVM_1K
    # WRITE80G
    # bucket_nums=$((8*$MB))
    # RUN_DB_BENCH

    # echo "---- with cuckoo filter 4MB----"
    # output_file=$output_path/Cuckoo_Filter_YES_4M_RW_NVM_1K
    # WRITE80G
    # bucket_nums=$((4*$MB))
    # RUN_DB_BENCH

    # echo "---- with cuckoo filter with bloom filter 32M ----"
    # sed -i 's/CUCKOO_FILTER = false/CUCKOO_FILTER = true/g' "$db_path"/util/global.h
    # sed -i 's/BLOOM_FILTER = false/BLOOM_FILTER = true/g' "$db_path"/util/global.h
    # MAKE
    # output_file=$output_path/Cuckoo_Filter_YES_BL_YES_32M_RW_NVM_1K
    # WRITE80G
    # RUN_DB_BENCH

    # echo "---- with cuckoo filter with bloom filter 8M ----"
    # sed -i 's/CUCKOO_FILTER = false/CUCKOO_FILTER = true/g' "$db_path"/util/global.h
    # sed -i 's/BLOOM_FILTER = false/BLOOM_FILTER = true/g' "$db_path"/util/global.h
    # MAKE
    # output_file=$output_path/Cuckoo_Filter_YES_BL_YES_8M_RW_NVM_1K
    # WRITE80G
    # RUN_DB_BENCH

    sed -i 's/READ_TIME_ANALYSIS = true/READ_TIME_ANALYSIS = false/g' "$db_path"/util/global.h
    sed -i 's/WRITE_TIME_ANALYSIS = true/WRITE_TIME_ANALYSIS = false/g' "$db_path"/util/global.h
    sed -i 's/TEST_CUCKOOFILTER = true/TEST_CUCKOOFILTER = false/g' "$db_path"/util/global.h
    sed -i 's/CUCKOO_FILTER = false/CUCKOO_FILTER = true/g' "$db_path"/util/global.h
    sed -i 's/BLOOM_FILTER = true/BLOOM_FILTER = false/g' "$db_path"/util/global.h
    MAKE
}

WRITE_TIME_ANALYSIS(){
    echo "------write time analysis-------"
    sed -i 's/WRITE_TIME_ANALYSIS = false/WRITE_TIME_ANALYSIS = true/g' "$db_path"/util/global.h

    benchmarks="fillrandom,stats"
    MAKE
    # output_file=$output_path/WRITE_TIME_ANALYSIS_80G_4K_RND
    # WRITE80G-4K
    # RUN_DB_BENCH

    output_file=$output_path/WRITE_TIME_ANALYSIS_80G_1K_RND
    WRITE80G
    RUN_DB_BENCH

    sed -i 's/WRITE_TIME_ANALYSIS = true/WRITE_TIME_ANALYSIS = false/g' "$db_path"/util/global.h
    MAKE
}

THREAD_COUNT_ANALYSIS(){
    leveldb_path=$ssd_path;
    echo "------thread count analysis-------"
    benchmarks="fillrandom,stats"
    echo "---- 1 thread ----"
    sed -i 's/WRITE_TIME_ANALYSIS = false/WRITE_TIME_ANALYSIS = true/g' "$db_path"/util/global.h
    sed -i 's/TASK_COUNT = [0-9]*/TASK_COUNT = 1/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Thead_1_Rnd_W_8GNVM_1K
    WRITE80G_16GNVM
    RUN_DB_BENCH

    echo "---- 4 thread ----"
    sed -i 's/TASK_COUNT = [0-9]*/TASK_COUNT = 4/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Thead_4_Rnd_W_8GNVM_1K
    WRITE80G_16GNVM
    RUN_DB_BENCH

    echo "---- 8 thread ----"
    sed -i 's/TASK_COUNT = [0-9]*/TASK_COUNT = 8/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Thead_8_Rnd_W_8GNVM_1K
    WRITE80G_16GNVM
    RUN_DB_BENCH

    echo "---- 16 thread ----"
    sed -i 's/TASK_COUNT = [0-9]*/TASK_COUNT = 16/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Thead_16_Rnd_W_8GNVM_1K
    WRITE80G_16GNVM
    RUN_DB_BENCH

    echo "---- 32 thread ----"
    sed -i 's/TASK_COUNT = [0-9]*/TASK_COUNT = 32/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Thead_32_Rnd_W_8GNVM_1K
    WRITE80G_16GNVM
    RUN_DB_BENCH

    echo "---- 64 thread ----"
    sed -i 's/TASK_COUNT = [0-9]*/TASK_COUNT = 64/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Thead_64_Rnd_W_8GNVM_1K
    WRITE80G_16GNVM
    RUN_DB_BENCH

    pm_size=$((180*$GB))
    flush_ssd=0
    leveldb_path=$pm_path;

    sed -i 's/WRITE_TIME_ANALYSIS = true/WRITE_TIME_ANALYSIS = false/g' "$db_path"/util/global.h
    MAKE
}

DYNAMIC_TREE_ANALYSIS(){
    echo "------dynamic tree analysis-------"
    benchmarks="fillrandom,stats"
    throughput=1

    echo "---- dynamic ----"
    output_file=$output_path/tree_dynamic
    dynamic_tree=1
    write_buffer_size=$((64*$MB))
    WRITE80G
    RUN_DB_BENCH

    # echo "---- static 16MB memtable----"
    # output_file=$output_path/tree_static_16MB
    # dynamic_tree=0
    # write_buffer_size=$((16*$MB))
    # WRITE80G
    # RUN_DB_BENCH

    # echo "---- static 32MB memtable----"
    # output_file=$output_path/tree_static_32MB
    # dynamic_tree=0
    # write_buffer_size=$((32*$MB))
    # WRITE80G
    # RUN_DB_BENCH

    # echo "---- static 64MB memtable----"
    # output_file=$output_path/tree_static_64MB
    # dynamic_tree=0
    # write_buffer_size=$((64*$MB))
    # WRITE80G
    # RUN_DB_BENCH

    # echo "---- static 128MB memtable----"
    # output_file=$output_path/tree_static_128MB
    # dynamic_tree=0
    # write_buffer_size=$((128*$MB))
    # WRITE80G
    # RUN_DB_BENCH

    # echo "---- static 256MB memtable----"
    # output_file=$output_path/tree_static_256MB
    # dynamic_tree=0
    # write_buffer_size=$((256*$MB))
    # WRITE80G
    # RUN_DB_BENCH

    throughput=0
    dynamic_tree=1
    write_buffer_size=$((64*$MB))
}

DATA_SIZE_ANALYSIS(){
    flush_ssd=1
    leveldb_path=$ssd_path
    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
    value_size=$((1*$KB))
    echo "------data size analysis-------"
    benchmarks="fillrandom,readrandom,stats"

    CLEAN_ALL_DB

    echo "---- 40GB 8GNVM----"
    output_file=$output_path/data_40G
    pm_size=$((8*$GB))
    write_buffer_size=$((64*$MB))
    num_kvs=$((40*$GB / $value_size))
    RUN_DB_BENCH

    echo "---- 80GB 16GNVM----"
    output_file=$output_path/data_80G
    pm_size=$((16*$GB))
    write_buffer_size=$((64*$MB))
    num_kvs=$((80*$GB / $value_size))
    RUN_DB_BENCH

    echo "---- 120GB 24GNVM----"
    output_file=$output_path/data_120G
    pm_size=$((24*$GB))
    write_buffer_size=$((64*$MB))
    num_kvs=$((120*$GB / $value_size))
    RUN_DB_BENCH

    echo "---- 160GB 32GNVM----"
    output_file=$output_path/data_160G
    pm_size=$((32*$GB))
    write_buffer_size=$((64*$MB))
    num_kvs=$((160*$GB / $value_size))
    RUN_DB_BENCH

    echo "---- 200GB 40GNVM----"
    output_file=$output_path/data_200G
    pm_size=$((40*$GB))
    write_buffer_size=$((64*$MB))
    num_kvs=$((200*$GB / $value_size))
    RUN_DB_BENCH

    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
    pm_size=$((180*$GB))
    flush_ssd=0
    leveldb_path=$pm_path
}

DATA_SIZE_ANALYSIS2(){
    flush_ssd=1
    leveldb_path=$ssd_path
    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
    value_size=$((1*$KB))
    reads=$((1024*1024))
    # max_file_size=$((256*$MB))
    # APP_PREFIX=""
    echo "------data size analysis-------"
    benchmarks="fillrandom,stats,approximate-memory-usage,readrandom,readrandom"

    CLEAN_ALL_DB

    # echo "---- 80GB 16GNVM----"
    # output_file=$output_path/data_80G_NUMA
    # pm_size=$((16*$GB))
    # write_buffer_size=$((64*$MB))
    # num_kvs=$((80*$GB / $value_size))
    # RUN_DB_BENCH

    # echo "---- 160GB 32GNVM----"
    # output_file=$output_path/data_160G_NUMA
    # pm_size=$((32*$GB))
    # write_buffer_size=$((64*$MB))
    # num_kvs=$((160*$GB / $value_size))
    # RUN_DB_BENCH

    # echo "---- 240GB 48GNVM----"
    # output_file=$output_path/data_240G_NUMA
    # pm_size=$((48*$GB))
    # write_buffer_size=$((64*$MB))
    # num_kvs=$((240*$GB / $value_size))
    # RUN_DB_BENCH

    # echo "---- 320GB 64GNVM----"
    # output_file=$output_path/data_320G_128MB
    # pm_size=$((64*$GB))
    # write_buffer_size=$((64*$MB))
    # num_kvs=$((320*$GB / $value_size))
    # RUN_DB_BENCH

    # sed -i 's/Cache_All_Filter = false/Cache_All_Filter = true/g' "$db_path"/util/global.h
    # MAKE
    # echo "---- 320GB 32GNVM----"
    # output_file=$output_path/data_320G_128MB_CacheAll
    # pm_size=$((64*$GB))
    # write_buffer_size=$((64*$MB))
    # num_kvs=$((320*$GB / $value_size))
    # RUN_DB_BENCH

    # sed -i 's/Cache_All_Filter = true/Cache_All_Filter = false/g' "$db_path"/util/global.h
    # MAKE

    # max_file_size=$((512*$MB))
    # echo "---- 320GB 32GNVM----"
    # output_file=$output_path/data_320G_512MB
    # pm_size=$((64*$GB))
    # write_buffer_size=$((64*$MB))
    # num_kvs=$((320*$GB / $value_size))
    # RUN_DB_BENCH

    # echo "---- 400GB 40GNVM----"
    # output_file=$output_path/data_400G_NUMA
    # pm_size=$((80*$GB))
    # write_buffer_size=$((64*$MB))
    # num_kvs=$((400*$GB / $value_size))
    # RUN_DB_BENCH

    echo "---- 600GB 120GNVM----"
    output_file=$output_path/data_600G_NUMA
    pm_size=$((120*$GB))
    write_buffer_size=$((64*$MB))
    num_kvs=$((600*$GB / $value_size))
    RUN_DB_BENCH

    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
    pm_size=$((180*$GB))
    max_file_size=$((128*$MB))
    # APP_PREFIX="numactl --cpunodebind=1 --membind=1"
    reads=$((-1))
    flush_ssd=0
    leveldb_path=$pm_path
}

NVM_DATA_SIZE_ANALYSIS(){
    # flush_ssd=1
    # leveldb_path=$ssd_path
    pm_size=$((400*$GB))
    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
    value_size=$((1*$KB))
    echo "------data size analysis-------"
    benchmarks="fillrandom,readrandom,stats"

    CLEAN_ALL_DB

    echo "---- 40GB 8GNVM----"
    output_file=$output_path/data_NVM_40G
    write_buffer_size=$((256*$MB))
    num_kvs=$((40*$GB / $value_size))
    RUN_DB_BENCH

    echo "---- 80GB 16GNVM----"
    output_file=$output_path/data_NVM_80G
    write_buffer_size=$((256*$MB))
    num_kvs=$((80*$GB / $value_size))
    RUN_DB_BENCH

    echo "---- 120GB 24GNVM----"
    output_file=$output_path/data_NVM_120G
    write_buffer_size=$((256*$MB))
    num_kvs=$((120*$GB / $value_size))
    RUN_DB_BENCH

    echo "---- 160GB 32GNVM----"
    output_file=$output_path/data_NVM_160G
    write_buffer_size=$((256*$MB))
    num_kvs=$((160*$GB / $value_size))
    RUN_DB_BENCH

    echo "---- 200GB 40GNVM----"
    output_file=$output_path/data_NVM_200G
    write_buffer_size=$((256*$MB))
    num_kvs=$((200*$GB / $value_size))
    RUN_DB_BENCH

    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
    pm_size=$((180*$GB))
    # flush_ssd=0
    # leveldb_path=$pm_path
}

NVM_DATA_SIZE_ANALYSIS2(){
    # flush_ssd=1
    # leveldb_path=$ssd_path
    pm_size=$((670*$GB))
    bucket_nums=$((128*$MB)) # bucket_nums * 4 > nums_kvs
    value_size=$((1*$KB))
    # reads=$((10*1024))
    echo "------data size analysis-------"
    benchmarks="fillrandom,readrandom,stats"

    CLEAN_ALL_DB

    # echo "---- 80GB----"
    # output_file=$output_path/data_NVM_80G
    # write_buffer_size=$((256*$MB))
    # num_kvs=$((80*$GB / $value_size))
    # RUN_DB_BENCH

    # echo "---- 160GB----"
    # output_file=$output_path/data_NVM_160G
    # write_buffer_size=$((256*$MB))
    # num_kvs=$((160*$GB / $value_size))
    # RUN_DB_BENCH

    # echo "---- 240GB----"
    # output_file=$output_path/data_NVM_240G
    # write_buffer_size=$((256*$MB))
    # num_kvs=$((240*$GB / $value_size))
    # RUN_DB_BENCH

    # echo "---- 320GB----"
    # output_file=$output_path/data_NVM_320G
    # write_buffer_size=$((256*$MB))
    # num_kvs=$((320*$GB / $value_size))
    # RUN_DB_BENCH

    # echo "---- 400GB----"
    # output_file=$output_path/data_NVM_400G
    # write_buffer_size=$((256*$MB))
    # num_kvs=$((400*$GB / $value_size))
    # RUN_DB_BENCH

    echo "---- 600GB----"
    output_file=$output_path/data_NVM_600G2
    write_buffer_size=$((256*$MB))
    num_kvs=$((600*$GB / $value_size))
    RUN_DB_BENCH

    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
    pm_size=$((180*$GB))
    reads=$((-1))
    # flush_ssd=0
    # leveldb_path=$pm_path
}

NVM_DATA_SIZE_ANALYSIS3(){
    # flush_ssd=1
    # leveldb_path=$ssd_path
    pm_size=$((700*$GB))
    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
    value_size=$((1*$KB))
    reads=$((10*1024))
    group_size=$((80*$GB))
    echo "------data size analysis-------"
    benchmarks="fillrandom,readrandom,stats"

    CLEAN_ALL_DB


    echo "---- 400GB----"
    output_file=$output_path/data_NVM_400G
    write_buffer_size=$((256*$MB))
    num_kvs=$((640*$GB / $value_size))
    RUN_DB_BENCH

    bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
    pm_size=$((180*$GB))
    reads=$((-1))
    group_size=$((0))
    # flush_ssd=0
    # leveldb_path=$pm_path
}

# INDEX_TEST(){
#     echo "------data size analysis-------"
#     benchmarks="fillseq,readrandom,stats"
#     write_buffer_size=$((150*$GB)) # bucket_nums * 4 > nums_kvs
#     pm_size=$((180*$GB))

#     echo "---- SkipList 100M----"
#     output_file=$output_path/SkipList-DRAM
#     value_size=$((1*$KB))
#     num_kvs=$((100*$MB / $value_size))
#     benchmarks="fillseq,readrandom,stats"
#     RUN_DB_BENCH

#     echo "---- SkipList 1G----"
#     output_file=$output_path/SkipList-DRAM
#     value_size=$((1*$KB))
#     num_kvs=$((1*$GB / $value_size))
#     benchmarks="fillseq,readrandom,stats"
#     RUN_DB_BENCH

#     echo "---- SkipList 10G----"
#     output_file=$output_path/SkipList-DRAM
#     value_size=$((1*$KB))
#     num_kvs=$((10*$GB / $value_size))
#     benchmarks="fillseq,readrandom,stats"
#     RUN_DB_BENCH

#     echo "---- SkipList 100G----"
#     output_file=$output_path/SkipList-DRAM
#     value_size=$((1*$KB))
#     num_kvs=$((100*$GB / $value_size))
#     benchmarks="fillseq,readrandom,stats"
#     RUN_DB_BENCH

#     bucket_nums=$((64*$MB)) # bucket_nums * 4 > nums_kvs
#     pm_size=$((190*$GB))
#     flush_ssd=0
# }

function SMALL_VALUE_TEST(){
    echo "------data size analysis-------"
    benchmarks="fillrandom,readrandom,readseq,stats"
    num_kvs=$((100*$MB))
    # gc_ratio=0

    echo "---- key100M_8B----"
    output_file=$output_path/key100M_8B
    value_size=$((8))
    RUN_DB_BENCH

    echo "---- key100M_32B----"
    output_file=$output_path/key100M_32B
    value_size=$((32))
    RUN_DB_BENCH

    echo "---- key100M_128B----"
    output_file=$output_path/key100M_128B
    value_size=$((128))
    RUN_DB_BENCH

    # sed -i 's/KV_SEPERATE = true/KV_SEPERATE = false/g' "$db_path"/util/global.h
    # MAKE

    # echo "---- key100M_8B NOKV----"
    # output_file=$output_path/key100M_8B_NOKV
    # value_size=$((8))
    # RUN_DB_BENCH

    # echo "---- key100M_32B NOKV----"
    # output_file=$output_path/key100M_32B_NOKV
    # value_size=$((32))
    # RUN_DB_BENCH

    # echo "---- key100M_128B NOKV----"
    # output_file=$output_path/key100M_128B_NOKV
    # value_size=$((128))
    # RUN_DB_BENCH

    # sed -i 's/KV_SEPERATE = false/KV_SEPERATE = true/g' "$db_path"/util/global.h
    # MAKE

    # gc_ratio=0.5
}

function MALLOC_FLUSH_TEST(){
    echo "------malloc flush analysis-------"
    benchmarks="fillrandom,stats"
    sed -i 's/MALLOC_TIME = false/MALLOC_TIME = true/g' "$db_path"/util/global.h
    sed -i 's/WRITE_TIME_ANALYSIS = false/WRITE_TIME_ANALYSIS = true/g' "$db_path"/util/global.h

    echo "---- with malloc flush ----"
    sed -i 's/MALLOC_FLUSH = false/MALLOC_FLUSH = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/MALLOC_FLUSH_YES_RW_NVM_1K
    WRITE80G
    RUN_DB_BENCH

    echo "---- without malloc flush ----"
    sed -i 's/MALLOC_FLUSH = true/MALLOC_FLUSH = false/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/MALLOC_FLUSH_NO_RW_NVM_1K
    WRITE80G
    RUN_DB_BENCH

    sed -i 's/MALLOC_FLUSH = true/MALLOC_FLUSH = false/g' "$db_path"/util/global.h
    sed -i 's/MALLOC_TIME = true/MALLOC_TIME = false/g' "$db_path"/util/global.h
    sed -i 's/WRITE_TIME_ANALYSIS = true/WRITE_TIME_ANALYSIS = false/g' "$db_path"/util/global.h
    MAKE
}

# DB_BENCH_TEST_GC() {
#     echo "------------db_bench gc------------"
#     benchmarks="fillrandom,stats"
#     # benchmarks="fillrandom,overwrite,overwrite,overwrite,overwrite,overwrite,overwrite,overwrite,overwrite,overwrite,readrandom,stats"
#     pm_size=$((380*$GB))

#     sed -i 's/KV_SEPERATE = false/KV_SEPERATE = true/g' "$db_path"/util/global.h
#     MAKE
#     echo "------1KB random write/read KV_Sep-----"
#     output_file=$output_path/Rnd_NVM_1K_KV_Sep
#     WRITE200G-1K
#     RUN_DB_BENCH

#     echo "------4KB random write/read KV_Sep-----"
#     output_file=$output_path/Rnd_NVM_4K_KV_Sep
#     WRITE200G-4K
#     RUN_DB_BENCH

#     echo "------16KB random write/read KV_Sep-----"
#     output_file=$output_path/Rnd_NVM_16K_KV_Sep
#     WRITE200G-16K
#     RUN_DB_BENCH

#     sed -i 's/KV_SEPERATE = true/KV_SEPERATE = false/g' "$db_path"/util/global.h
#     MAKE
#     echo "------1KB random write/read NoKV-----"
#     output_file=$output_path/Rnd_NVM_1K_NoKV
#     WRITE200G-1K
#     RUN_DB_BENCH

#     echo "------4KB random write/read NoKV-----"
#     output_file=$output_path/Rnd_NVM_4K_NoKV
#     WRITE200G-4K
#     RUN_DB_BENCH

#     echo "------16KB random write/read NoKV-----"
#     output_file=$output_path/Rnd_NVM_16K_NoKV
#     WRITE200G-16K
#     RUN_DB_BENCH

#     pm_size=$((180*$GB))
#     sed -i 's/KV_SEPERATE = false/KV_SEPERATE = true/g' "$db_path"/util/global.h
#     MAKE
#     CLEAN_DB
# }

DB_BENCH_TEST_GC() {
    echo "------------db_bench gc------------"
    benchmarks="fillrandom,readrandom,readseq,stats"
    # benchmarks="fillrandom,overwrite,overwrite,overwrite,overwrite,overwrite,overwrite,overwrite,overwrite,overwrite,readrandom,stats"
    pm_size=$((400*$GB))

    echo "------------db_bench KV SEP GC=0.5------------"
    sed -i 's/KV_SEPERATE = false/KV_SEPERATE = true/g' "$db_path"/util/global.h
    MAKE

    echo "------8B random write/read KV_Sep-----"
    output_file=$output_path/Rnd_NVM_8B_KV_Sep
    WRITE100M-8B
    RUN_DB_BENCH

    echo "------64B random write/read KV_Sep-----"
    output_file=$output_path/Rnd_NVM_64B_KV_Sep
    WRITE100M-64B
    RUN_DB_BENCH

    echo "------256B random write/read KV_Sep-----"
    output_file=$output_path/Rnd_NVM_256B_KV_Sep
    WRITE100M-256B
    RUN_DB_BENCH


    echo "------1KB random write/read KV_Sep-----"
    output_file=$output_path/Rnd_NVM_1KB_80G__KV_Sep
    WRITE80G
    RUN_DB_BENCH

    echo "------4KB random write/read KV_Sep-----"
    output_file=$output_path/Rnd_NVM_4KB_80G_KV_Sep
    WRITE80G-4K
    RUN_DB_BENCH

    # echo "------16KB random write/read KV_Sep-----"
    # output_file=$output_path/Rnd_NVM_16KB_80G_KV_Sep
    # WRITE80G-16K
    # RUN_DB_BENCH


    # echo "------1KB random write/read KV_Sep-----"
    # output_file=$output_path/Rnd_NVM_1KB_400G__KV_Sep
    # WRITE400G-1K
    # RUN_DB_BENCH

    # echo "------4KB random write/read KV_Sep-----"
    # output_file=$output_path/Rnd_NVM_4KB_400G_KV_Sep
    # WRITE400G-4K
    # RUN_DB_BENCH

    # echo "------16KB random write/read KV_Sep-----"
    # output_file=$output_path/Rnd_NVM_16KB_400G_KV_Sep
    # WRITE400G-16K
    # RUN_DB_BENCH



    echo "------------db_bench KV SEP GC=0------------"
    gc_ratio=0

    echo "------8B random write/read KV_Sep_NOGC-----"
    output_file=$output_path/Rnd_NVM_8B_KV_Sep_NOGC
    WRITE100M-8B
    RUN_DB_BENCH

    echo "------64B random write/read KV_Sep_NOGC-----"
    output_file=$output_path/Rnd_NVM_64B_KV_Sep_NOGC
    WRITE100M-64B
    RUN_DB_BENCH

    echo "------256B random write/read KV_Sep_NOGC-----"
    output_file=$output_path/Rnd_NVM_256B_KV_Sep_NOGC
    WRITE100M-256B
    RUN_DB_BENCH


    echo "------1KB random write/read KV_Sep_NOGC-----"
    output_file=$output_path/Rnd_NVM_1KB_80G__KV_Sep_NOGC
    WRITE80G
    RUN_DB_BENCH

    echo "------4KB random write/read KV_Sep_NOGC-----"
    output_file=$output_path/Rnd_NVM_4KB_80G_KV_Sep_NOGC
    WRITE80G-4K
    RUN_DB_BENCH

    # echo "------16KB random write/read KV_Sep_NOGC-----"
    # output_file=$output_path/Rnd_NVM_16KB_80G_KV_Sep_NOGC
    # WRITE80G-16K
    # RUN_DB_BENCH


    # echo "------1KB random write/read KV_Sep_NOGC-----"
    # output_file=$output_path/Rnd_NVM_1KB_400G__KV_Sep_NOGC
    # WRITE400G-1K
    # RUN_DB_BENCH

    # echo "------4KB random write/read KV_Sep_NOGC-----"
    # output_file=$output_path/Rnd_NVM_4KB_400G_KV_Sep_NOGC
    # WRITE400G-4K
    # RUN_DB_BENCH

    # echo "------16KB random write/read KV_Sep_NOGC-----"
    # output_file=$output_path/Rnd_NVM_16KB_400G_KV_Sep_NOGC
    # WRITE400G-16K
    # RUN_DB_BENCH
    gc_ratio=0.5



    echo "------------db_bench KV NO SEP------------"
    sed -i 's/KV_SEPERATE = true/KV_SEPERATE = false/g' "$db_path"/util/global.h
    MAKE

    echo "------8B random write/read NO_KV_SEP-----"
    output_file=$output_path/Rnd_NVM_8B_NO_KV_SEP
    WRITE100M-8B
    RUN_DB_BENCH

    echo "------64B random write/read NO_KV_SEP-----"
    output_file=$output_path/Rnd_NVM_64B_NO_KV_SEP
    WRITE100M-64B
    RUN_DB_BENCH

    echo "------256B random write/read NO_KV_SEP-----"
    output_file=$output_path/Rnd_NVM_256B_NO_KV_SEP
    WRITE100M-256B
    RUN_DB_BENCH


    echo "------1KB random write/read NO_KV_SEP-----"
    output_file=$output_path/Rnd_NVM_1KB_80G__NO_KV_SEP
    WRITE80G
    RUN_DB_BENCH

    echo "------4KB random write/read NO_KV_SEP-----"
    output_file=$output_path/Rnd_NVM_4KB_80G_NO_KV_SEP
    WRITE80G-4K
    RUN_DB_BENCH

    # echo "------16KB random write/read NO_KV_SEP-----"
    # output_file=$output_path/Rnd_NVM_16KB_80G_NO_KV_SEP
    # WRITE80G-16K
    # RUN_DB_BENCH


    # echo "------1KB random write/read NO_KV_SEP-----"
    # output_file=$output_path/Rnd_NVM_1KB_400G__NO_KV_SEP
    # WRITE400G-1K
    # RUN_DB_BENCH

    # echo "------4KB random write/read NO_KV_SEP-----"
    # output_file=$output_path/Rnd_NVM_4KB_400G_NO_KV_SEP
    # WRITE400G-4K
    # RUN_DB_BENCH

    # echo "------16KB random write/read NO_KV_SEP-----"
    # output_file=$output_path/Rnd_NVM_16KB_400G_NO_KV_SEP
    # WRITE400G-16K
    # RUN_DB_BENCH

    sed -i 's/KV_SEPERATE = false/KV_SEPERATE = true/g' "$db_path"/util/global.h
    MAKE
    CLEAN_DB
}

INDEX_SCAN_TEST(){
    echo "------------db_bench gc------------"
    pm_size=$((380*$GB))
    dynamic_tree=0
    write_buffer_size=$((1*$GB))

    echo "------1kb 10M SkipList DRAM-----"
    benchmarks="fillrandom,skiplistindex,readseq,stats"
    output_file=$output_path/Rnd_NVM_10M_SkipList_DRAM
    WRITE10M-8B
    RUN_DB_BENCH

    echo "------1kb 10M SkipList NVM-----"
    sed -i 's/TEST_SKIPLIST_NVM = false/TEST_SKIPLIST_NVM = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Rnd_NVM_10M_SkipList_NVM
    WRITE10M-8B
    RUN_DB_BENCH
    sed -i 's/TEST_SKIPLIST_NVM = true/TEST_SKIPLIST_NVM = false/g' "$db_path"/util/global.h
    MAKE

    benchmarks="fillrandom,flush,bptreeindex,readseq,stats"
    echo "------1kb 10M B+Tree HyBrid-----"
    output_file=$output_path/Rnd_NVM_10M_BPTree_Hybrid
    WRITE10M-8B
    RUN_DB_BENCH

    benchmarks="fillrandom,flush,bptreeindex,readseq,stats"
    echo "------1kb 10M B+Tree NVM-----"
    sed -i 's/TEST_BPTREE_NVM = false/TEST_BPTREE_NVM = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Rnd_NVM_10M_BPTree_NVM
    WRITE10M-8B
    RUN_DB_BENCH
    sed -i 's/TEST_BPTREE_NVM = true/TEST_BPTREE_NVM = false/g' "$db_path"/util/global.h
    MAKE

    
    benchmarks="fillrandom,flush,bptreeindex,readseq,stats"
    echo "------1kb 10M B+Tree DRAM-----"
    output_file=$output_path/Rnd_NVM_10M_BPTree_DRAM
    use_pm=0
    WRITE10M-8B
    RUN_DB_BENCH
    use_pm=1

    benchmarks="fillrandom,flush,sstableindex,stats"
    echo "------1kb 10M SSTable NVM Key-----"
    sed -i 's/TEST_FLUSH_SSD = false/TEST_FLUSH_SSD = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Rnd_NVM_10M_SSTable_NVM_Key
    WRITE10M-8B
    RUN_DB_BENCH
    sed -i 's/TEST_FLUSH_SSD = true/TEST_FLUSH_SSD = false/g' "$db_path"/util/global.h
    MAKE

    benchmarks="fillrandom,flush,readseq,stats"
    echo "------1kb 10M SSTable NVM KV-----"
    sed -i 's/TEST_FLUSH_SSD = false/TEST_FLUSH_SSD = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Rnd_NVM_10M_SSTable_NVM_KV
    WRITE10M-8B
    RUN_DB_BENCH
    sed -i 's/TEST_FLUSH_SSD = true/TEST_FLUSH_SSD = false/g' "$db_path"/util/global.h
    MAKE

    pm_size=$((380*$GB))
    dynamic_tree=1
    CLEAN_DB
}


INDEX_TEST(){
    echo "------------db_bench gc------------"
    pm_size=$((380*$GB))
    dynamic_tree=0
    write_buffer_size=$((1*$GB))
    sed -i 's/CUCKOO_FILTER = true/CUCKOO_FILTER = false/g' "$db_path"/util/global.h
    MAKE

    echo "------1kb 10M SkipList DRAM-----"
    benchmarks="fillrandom,readrandom,stats"
    sed -i 's/TEST_SKIPLIST_DRAM = false/TEST_SKIPLIST_DRAM = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Rnd_NVM_Random_10M_SkipList_DRAM
    WRITE10M-8B
    RUN_DB_BENCH
    sed -i 's/TEST_SKIPLIST_DRAM = true/TEST_SKIPLIST_DRAM = false/g' "$db_path"/util/global.h
    MAKE

    echo "------1kb 10M SkipList NVM-----"
    sed -i 's/TEST_SKIPLIST_NVM = false/TEST_SKIPLIST_NVM = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Rnd_NVM_Random_10M_SkipList_NVM
    WRITE10M-8B
    RUN_DB_BENCH
    sed -i 's/TEST_SKIPLIST_NVM = true/TEST_SKIPLIST_NVM = false/g' "$db_path"/util/global.h
    MAKE


    echo "------1kb 10M B+Tree HyBrid-----"
    benchmarks="fillrandom,flush,readrandom,stats"
    output_file=$output_path/Rnd_NVM_Random_10M_BPTree_Hybrid
    WRITE10M-8B
    RUN_DB_BENCH

    echo "------1kb 10M B+Tree NVM-----"
    sed -i 's/TEST_BPTREE_NVM = false/TEST_BPTREE_NVM = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Rnd_NVM_Random_10M_BPTree_NVM
    WRITE10M-8B
    RUN_DB_BENCH
    sed -i 's/TEST_BPTREE_NVM = true/TEST_BPTREE_NVM = false/g' "$db_path"/util/global.h
    MAKE

    echo "------1kb 10M B+Tree DRAM-----"
    output_file=$output_path/Rnd_NVM_Random_10M_BPTree_DRAM
    use_pm=0
    WRITE10M-8B
    RUN_DB_BENCH
    use_pm=1


    echo "------1kb 10M SSTable NVM-----"
    benchmarks="fillrandom,flush,readrandom,stats"
    sed -i 's/TEST_FLUSH_SSD = false/TEST_FLUSH_SSD = true/g' "$db_path"/util/global.h
    MAKE
    output_file=$output_path/Rnd_NVM_Random_10M_SSTable_NVM
    WRITE10M-8B
    RUN_DB_BENCH
    sed -i 's/TEST_FLUSH_SSD = true/TEST_FLUSH_SSD = false/g' "$db_path"/util/global.h
    MAKE


    sed -i 's/CUCKOO_FILTER = false/CUCKOO_FILTER = true/g' "$db_path"/util/global.h
    MAKE
    pm_size=$((380*$GB))
    dynamic_tree=1
    CLEAN_DB
}

# DB_BENCH_ReWhileWr_TEST() {
#     echo "------------db_bench------------"
#     benchmarks="readwhilewriting,stats"

#     echo "------1KB random write/read-----"
#     num_thread=10
#     output_file=$output_path/Rnd_NVM_1K_RWW
#     WRITE80G
#     RUN_DB_BENCH

#     num_thread=1

#     CLEAN_DB
# }



MAKE
SET_OUTPUT_PATH



YCSB_TEST_MULTI_THREAD(){
    APP_PREFIX=""
    cd "$ycsb_path" || exit
    echo "------------YCSB------------"
    
    # echo "-----YCSB_MT1-----"
    # output_file=$output_path/YCSB_MT1
    # ycsb_input=1KB_MULTI_THREAD1
    # RUN_YCSB

    # echo "-----YCSB_MT2-----"
    # output_file=$output_path/YCSB_MT2
    # ycsb_input=1KB_MULTI_THREAD2
    # RUN_YCSB

    echo "-----YCSB_MT3-----"
    output_file=$output_path/YCSB_MT3
    ycsb_input=1KB_MULTI_THREAD3
    RUN_YCSB

    echo "-----YCSB_MT4-----"
    output_file=$output_path/YCSB_MT4
    ycsb_input=1KB_MULTI_THREAD4
    RUN_YCSB

    cd ..
    APP_PREFIX="numactl --cpunodebind=0 --membind=0"
}

# # echo "chapter 4.1"
# DB_BENCH_TEST
# # DB_BENCH_TEST2
# DB_BENCH_THROUGHPUT
# SMALL_VALUE_TEST
# DYNAMIC_TREE_ANALYSIS

# # echo "chapter 4.2"
# YCSB_TEST
# YCSB_TEST_LATENCY

# # echo "chapter 4.3"
# YCSB_TEST_SSD
THREAD_COUNT_ANALYSIS
# DB_BENCH_TEST_FLUSHSSD
# # DB_BENCH_TEST_FLUSHSSD2
# # # DB_BENCH_TEST_FLUSHSSD_4K
# DATA_SIZE_ANALYSIS

# # echo "chapter 4.4"
# MALLOC_FLUSH_TEST
# WRITE_TIME_ANALYSIS
# # CUCKOO_FILTER_ANALYSIS
# CUCKOO_FILTER_ANALYSIS2

# INDEX_SCAN_TEST
# # SMALL_VALUE_TEST

# INDEX_TEST
# # DB_BENCH_ReWhileWr_TEST
# # YCSB_TEST_MULTI_THREAD
# # DB_BENCH_TEST_GC
# NVM_DATA_SIZE_ANALYSIS

# CUCKOO_FILTER_ANALYSIS2
# NVM_DATA_SIZE_ANALYSIS2
# DATA_SIZE_ANALYSIS2

# MALLOC_FLUSH_TEST
# DYNAMIC_TREE_ANALYSIS

CLEAN_DB
# sudo cp build/libleveldb.a /usr/local/lib/
# sudo cp -r include/leveldb /usr/local/include/
# -exec break __sanitizer::Die

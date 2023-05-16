#!/bin/bash
#set -x
KB=$((1024))
MB=$(($KB*1024))
GB=$(($MB*1024))
# APP_PREFIX=sudo
APP_PREFIX=numactl --cpunodebind=0 --membind=0

db_path=$(pwd)
db_bench=$db_path/build
db_include=$db_path/include
ycsb_path=$db_path/ycsbc

sata_path=/tmp/pm_test
ssd_path=/media/nvme1/pm_test
pm_path=/mnt/pmem0.1/pm_test
# leveldb_path=/tmp/leveldb-wzh
leveldb_path=$pm_path
output_path=$db_path/output
output_file=$output_path/result.out

export CPLUS_INCLUDE_PATH=$db_path/include:$CPLUS_INCLUDE_PATH
export LIBRARY_PATH=$db_path/build:$LIBRARY_PATH

benchmarks="overwrite,readrandom,readseq,stats"
# benchmarks2="fillseqNofresh,readrandom,readseq,stats"
ycsb_input=1KB_ALL

num_thread=1
value_size=1024
num_kvs=$((10*$MB))
write_buffer_size=$((50*$MB))
max_file_size=$((128*$MB))
pm_size=$((200*$GB))
bucket_nums=$((4*$MB)) # bucket_nums * 4 > nums_kvs
use_pm=1
flush_ssd=0

WRITE10G() {
    pm_path=$sata_path
    leveldb_path=$sata_path
    # leveldb_path=$pm_path;
    value_size=1000
    num_thread=1
    num_kvs=$((10*$MB))
    # write_buffer_size=$((3*$MB))
    max_file_size=$((100*$MB))
    pm_size=$((20*$GB))
    bucket_nums=$((4*$MB)) # bucket_nums * 4 > nums_kvs
}
WRITE80G_FLUSHSSD() {
    leveldb_path=$ssd_path;
    value_size=$((4*$KB))
    num_kvs=$((80*$GB / $value_size))
    pm_size=$((8*$GB))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
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
WRITE100G() {
    leveldb_path=$pm_path;
    value_size=1000
    num_thread=1
    num_kvs=$((100*$MB))
    # write_buffer_size=$((40*$MB))
    max_file_size=$((1024*$MB))
    pm_size=$((200*$GB))
    bucket_nums=$((40*$MB)) # bucket_nums * 4 > nums_kvs
    use_pm=1
}

RUN_DB_BENCH() {
    CLEAN_DB
    parameters="--benchmarks=$benchmarks \
                --num=$num_kvs \
                --value_size=$value_size \
                --write_buffer_size=$write_buffer_size \
                --max_file_size=$max_file_size \
                --pm_size=$pm_size \
                --pm_path=$pm_path \
                --db=$leveldb_path \
                --bucket_nums=$bucket_nums \
                --use_pm=$use_pm \
                --threads=$num_thread \
                --flush_ssd=$flush_ssd
                "
    cmd="$APP_PREFIX $db_bench/db_bench $parameters >> $output_file"
    echo $cmd >> $output_file
    echo $cmd
    eval $cmd
}

RUN_YCSB(){
    CLEAN_DB
    cmd="$APP_PREFIX $ycsb_path/ycsbc $ycsb_path/input/$ycsb_input >> $output_file"
    echo $cmd >> $output_file
    echo $cmd
    eval $cmd
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
  rm -rf ${leveldb_path:?}/*
  rm -rf ${pm_path:?}/*
#   mkdir -p $pm_path
}

SET_OUTPUT_PATH() {
    if [ ! -d "$output_path" ]; then
        # 如果目录不存在，则创建目录
        mkdir "$output_path"
        echo "Created output_path: $output_path"
    else
        # 如果目录已存在，则清空目录下的所有文件
        rm -rf "${output_path:?}/"*
        echo "Cleared output_path: $output_path"
    fi
    touch $output_file
    echo "Created file: $output_file"
}

MAKE() {
  cd $db_bench
  #make clean
  make -j32
  cd ..
  cd $ycsb_path
  #make clean
  make
  cd ..
}

DB_BENCH_TEST() {
    echo "------------db_bench------------"
    benchmarks="fillrandom,readrandom,stats"
    echo "------1KB random write/read-----"
    WRITE80G
    RUN_DB_BENCH

    echo "------4KB random write/read-----"
    WRITE80G-4K
    RUN_DB_BENCH

    echo "------16KB random write/read-----"
    WRITE80G-16K
    RUN_DB_BENCH

    echo "------64KB random write/read-----"
    WRITE80G-64K
    RUN_DB_BENCH


    benchmarks="fillseq,readseq,stats"
    echo "------1KB sequential write/read-----"
    WRITE80G
    RUN_DB_BENCH

    echo "------4KB sequential write/read-----"
    WRITE80G-4K
    RUN_DB_BENCH

    echo "------16KB sequential write/read-----"
    WRITE80G-16K
    RUN_DB_BENCH

    echo "------64KB sequential write/read-----"
    WRITE80G-64K
    RUN_DB_BENCH

    CLEAN_DB
}
DB_BENCH_TEST_FLUSHSSD() {
    echo "----------db_bench_flushssd----------"
    benchmarks="fillrandom,readrandom,stats"
    echo "------4KB random write/read-----"
    WRITE80G_FLUSHSSD
    RUN_DB_BENCH

    CLEAN_DB
}

YCSB_TEST(){
    cd $ycsb_path
    echo "------------YCSB------------"
    # echo "-----1KB YCSB performance-----"
    # ycsb_input=1KB_ALL
    # RUN_YCSB

    echo "-----4KB YCSB performance-----"
    ycsb_input=4KB_ALL
    RUN_YCSB
    cd ..
}



MAKE
SET_OUTPUT_PATH

DB_BENCH_TEST_FLUSHSSD
# DB_BENCH_TEST
# YCSB_TEST

# sudo cp build/libleveldb.a /usr/local/lib/
# sudo cp -r include/leveldb /usr/local/include/
# -exec break __sanitizer::Die
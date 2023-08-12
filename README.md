# BushStore: Efficient B+Tree Group Indexing for LSM-Tree in Hybrid Persistent Storage.

&#160; &#160; &#160; &#160; BushStore is an adaptive and horizontally scalable Log-Structured Merge-Tree for NVM-SSD hybrid storage that mitigates write and read amplifications. This web page hosts the code and data used in the paper titled "BushStore: Efficient B+Tree Group Indexing for LSM-Tree in Hybrid Persistent Storage".

## Important Notes on Tail Latency  (BushStore vs. MioDB) 

 &#160; &#160; &#160; After submitting our paper, we became aware of a code issue in one of the crucial baselines. This issue accounts for certain key comparative results which could influence the reviewers' assessment of our work. As such, we wish to clarify this matter and provide additional evidence to aid reviewers in making an informed judgment.

&#160; &#160; &#160; &#160; We refer to Table 2 of our paper which presents the tail latency under different percentiles. The low tail latencies of MioDB are mainly attributed to, according to their paper, the one-piece flushing, skiplist compaction with lower WA, etc. However, after careful study of their source code, it seems that the dominating factor is MioDB uses NVM as **memory extension**. Therefore it is hard, if not impossible, to guarantee **persistence** of flushing in MioDB. This issue has been noticed by some developers of the community ([here](https://github.com/CGCL-codes/mioDB/issues/16)). In contrast, our work uses the PMDK interface, which has performance implications for persistence. This explains the reason for the results of p99.9 and p99.99 tail latency for BushStore versus MioDB.

&#160; &#160; &#160; &#160; As additional notes, we show how BushStore can be tuned towards tail latencies lower than MioDB, meanwhile preserving flushing persistence. This can be done by adjusting the persistence strategy of WAL. Previously in BushStore, the persistence granularity of WAL was one entire data page. While such large persistence granularity improves throughput, it also adds to tail latency, as a trade-off. By reducing the persistence granularity, the tail latency can be significantly reduced. However, this comes at the expense of slight reduction in throughput.


&#160; &#160; &#160; &#160; So we add an additional persistence strategy for WAL where we write each data page in multiple persistence operations (32 times). At the expense of losing 5.7% in average write latency, we obtain 67% reduction at p99.9 and 77% reduction at p99.99 in tail latency, outperforming MioDB while preserving flushing persistence. The results are shown in the last row of the following table (BushStore-Small-WAL).


Table: Tail Latency(us)


| KV Stores           | avg.   | p95  | p99  | p99.9 | p99.99 |
| ------------------- | ------ | ---- | ---- | ----- | ------ |
| RocksDB             | 18.87  | 26   | 41   | 1097  | 1113   |
| NoveLSM             | 20.582 | 24   | 27   | 60    | 527    |
| SLM-DB              | 33.98  | 18   | 1085 | 1195  | 1225   |
| MatrixKV            | 20.54  | 18   | 199  | 1100  | 1107   |
| MioDB               | 10.21  | 19   | 22   | 32    | 62     |
| BushStore           | 2.915  | 3    | 4    | 46    | 116    |
| BushStore-Small-WAL | 3.083  | 10   | 11   | 15    | 26     |

&#160; &#160; &#160; &#160; We will update the full results of the new strategy in a revision of our paper.

## Environment, Workloads and Evaluation

### 1. Environment

**Our** evaluation is based on the following configurations and versions, you can try others.

* Hardware configuration

```
[CPU] 18-core 2.60GHz Intel(R) Xeon(R) Gold 6240C CPUs with 24.75 MB cache * 2
[MEM] 2666MHz DDR4 DRAM (32GB * 12)
[NVM] 1.5TB Intel Optane DC PMMs (128GB * 12)
[SSD] INTEL SSDPEDME016T4F.
```

* Operating environment

```
[os] Ubuntu 20.04.4 LTS (GNU/Linux 5.4.0-155-generic x86_64)
[gcc] 9.4.0
[cmake] 3.22.0(>=3.9)
[snappy] 1.2.0
[ndctl]
```

### 2. Workloads

```
db_bench (BushStore/benchmarks/db_bench)
ycsb (BushStore/ycsbc)
```

### 3. Compilation

You can compile as follows, or just use the `run_db_bench.sh` script, which packages the complete compilation and testing process

1. Compiling the BushStore and `db_bench` tools

```
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && make
```

2. Compiling YCSB

```
cd ycsbc
make
```

### 4. Running

It is recommended to run the test directly using `run_db_bench.sh`, which presets the parameters used in our experiments. Or you can use the following way to test manually

* Microbenchmark

For example, to test the random read/write of 80G 1KB value size data, we use the following parameters

```
./db_bench --benchmarks=fillrandom,readrandom,stats --num=83886080 --value_size=1024 --write_buffer_size=67108864 --max_file_size=134217728 --open_files=10000 --reads=-1 --pm_size=193273528320 --pm_path=/mnt/pmem0.1/pm_test --db=/mnt/pmem0.1/pm_test --bucket_nums=33554432 --use_pm=1 --threads=1 --flush_ssd=0 --throughput=0 --dynamic_tree=1
```

* YCSB

For example, to test YCSBC with 1KB value size, you can use the following approach.

```
cd ycsbc
./ycsbc ./input/1KB_ALL
```

For specific YCSB runtime parameters, you can read and modify ycsbc/workloads, ycsbc/inputs, ycsbc/db/leveldb_db.cc

* Full Test

You can run `run_db_bench.sh` directly and it will generate the output directory in the BushStore directory, which contains the results of running all the tests for the paper

```
./run_db_bench.sh
```

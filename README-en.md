# Skiplist-CPP

A lightweight KV engine based on a skip list. The current goal of this repository is a small, explainable storage component rather than a full database.

Current baseline:

* C++17 header-only skip list implementation
* Main APIs: `put / get / contains / erase / scan / checkpoint / recover`
* Compatibility APIs: `insert_element / search_element / delete_element / dump_file / load_file`
* Table-level read/write lock concurrency model
* Basic `snapshot + WAL` recovery path
* Parameterized benchmark harness

## Persistence Model

The repository currently implements a P1-level basic recovery model:

* Snapshot format
  * Header: `SKIPLIST_SNAPSHOT_V1`
  * Records: `<key>\t<escaped_value>`
* WAL format
  * `P\t<key>\t<escaped_value>`
  * `D\t<key>`
* `recover()` order
  * load snapshot first
  * replay WAL next
* `checkpoint()` semantics
  * write a new snapshot
  * truncate the old WAL

Current boundaries:

* Persistence is only supported for `K=int, V=std::string`
* Recovery is basic and not production-grade crash consistency
* There is still no SSTable, compaction, Bloom filter, checksum, or corruption repair

## Project Layout

* `skiplist.h`
  * core skip list implementation
* `main.cpp`
  * demo for `put / get / scan / checkpoint / recover`
* `tests/test_main.cpp`
  * lightweight regression test runner
* `stress-test/stress_test.cpp`
  * benchmark harness
* `stress_test_start.sh`
  * benchmark build/run entry
* `store/`
  * default snapshot/WAL directory
* `bin/`
  * generated executables

## Public APIs

* `WriteResult put(const K&, const V&)`
* `std::optional<V> get(const K&) const`
* `bool contains(const K&) const`
* `bool erase(const K&)`
* `std::vector<std::pair<K, V>> scan(const K&, const K&) const`
* `size_t size() const`
* `bool checkpoint()`
* `bool recover()`

## Build and Test

```bash
make
./bin/main
```

```bash
make test
```

```bash
sh stress_test_start.sh
```

Example benchmark command:

```bash
sh stress_test_start.sh --threads=4 --ops=100000 --workload=mixed --sync-wal=false
```

## Demo Flow

`main.cpp` now demonstrates:

1. `put`
2. `get`
3. `scan`
4. `checkpoint`
5. more writes after checkpoint
6. recovery in a fresh instance

## Test Coverage

The current regression suite covers:

* base CRUD behavior and compatibility APIs
* ordered `scan`
* snapshot round-trip
* WAL append ordering and failure paths
* WAL-only recover
* snapshot + WAL recover
* WAL truncation after checkpoint
* invalid snapshot / invalid WAL failure cases
* concurrency smoke tests

## Next Steps

Likely next milestones if the project keeps moving toward a database design:

* `MemTable + Immutable MemTable + SSTable`
* background flush
* compaction
* checksum and corruption detection
* fuller automated tests and CI

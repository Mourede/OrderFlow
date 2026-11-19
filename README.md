# OrderFlow

OrderFlow is a C++17 program that simulates a warehouse filling orders, built for the CS4348 Operating Systems project.

One thread makes up customer orders and puts them in a queue. A few worker threads take orders from the queue and try to fill them from a shared stock list. An order either ships in full or gets backordered. It never ships half-filled, and the stock never goes below zero, even when many threads are working at once.

```
Producer  ->  Queue  ->  Worker 1  \
                     ->  Worker 2   -->  Inventory
                     ->  Worker N  /
```

## Build

You need Linux (or WSL on Windows), a C++17 compiler, CMake 3.16 or newer, and Git.

```bash
cmake -S . -B build
cmake --build build -j
./build/orderflow
```

You'll see something like this:

```
=== OrderFlow simulation ===
Workers:          4
Orders processed: 1000
Fulfilled:        788 (78.8% fill rate)
Backordered:      212
Units shipped:    4337 of 22735
Elapsed:          64.0 ms
Throughput:       15632.0 orders/sec
```

## Options

| Option | What it does |
| --- | --- |
| `--workers N` | Number of worker threads |
| `--orders N` | Number of orders to generate |
| `--seed N` | Random seed |
| `--inventory FILE` | Starting stock (default: `data/inventory.csv`) |
| `--pick-us N` | Pretend time to pick each order, in microseconds |
| `--help` | Show help |

Example:

```bash
./build/orderflow --workers 8 --orders 5000 --seed 7
```

More workers means faster runs. With more than one worker, the numbers can change a little from run to run, because it depends on which thread grabs the last items first. With `--workers 1`, the same seed always gives the same result.

## Tests

```bash
cd build && ctest --output-on-failure
```

To check for data races, build with ThreadSanitizer:

```bash
cmake -S . -B build-tsan -DORDERFLOW_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan -j
cd build-tsan && ctest
```

GoogleTest is downloaded the first time you build, unless you already have it installed (`sudo apt install libgtest-dev`).

## Files

```
include/orderflow/   headers (queue, inventory, simulator)
src/                 source files and main.cpp
tests/               unit tests
data/                sample inventory.csv
```


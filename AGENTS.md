# Repository Guidelines

## Project Structure & Module Organization
`skiplist.h` contains the header-only skip list implementation and is the main place for core logic changes. `main.cpp` is a small demo program that exercises insert, search, delete, and file dump behavior. `stress-test/stress_test.cpp` is the performance harness, and `stress_test_start.sh` builds and runs it. `bin/` holds compiled demo and stress binaries. `store/dumpFile` is the persisted `key:value` output written by the demo. `README.md` and `README-en.md` are the primary user-facing docs.

## Build, Test, and Development Commands
Run `make` to compile the demo into `bin/main` with `g++` and C++11 flags. Run `./bin/main` to execute the sample workflow and refresh `store/dumpFile`. Run `sh stress_test_start.sh` to build `bin/stress` and execute the threaded stress test. Run `make clean` to remove root-level object files. When validating a logic change, run both the demo and the stress script.

## Coding Style & Naming Conventions
Target C++11 and stay close to the existing header-only design. Prefer 4-space indentation and keep braces on the same line as declarations and control statements. Use `PascalCase` for types such as `SkipList` and `Node`, `snake_case` for methods such as `insert_element`, and `ALL_CAPS` for macros such as `STORE_FILE`. Keep serialization-compatible changes explicit: the current on-disk format is `key:value` per line.

## Testing Guidelines
This repository currently relies on executable validation rather than a checked-in unit test suite. Before opening a PR, run `make`, `./bin/main`, and `sh stress_test_start.sh`, then note the commands in the PR description. If you add automated tests later, place them in a dedicated test directory and document the new command in the README.

## Commit & Pull Request Guidelines
Recent history mixes vague subjects like `Update` with clearer fixes like `fix memory leak` and `fix load_file`. Prefer short, imperative commit messages that describe the actual change, for example `fix: load dump file parsing` or `docs: clarify stress test usage`. Pull requests should include a concise summary, linked issue if available, and the local validation commands you ran. No screenshots are needed for this CLI project.

## Generated Files
Treat `bin/main`, `bin/stress`, and `store/dumpFile` as generated outputs. Only include changes to those files when the artifact itself is intentionally part of the review.

# Cloud Storage System — Simplified Banking System (C++)

A minimal in-memory implementation of a banking system designed to pass a multi-level test suite.

Implemented levels
- Level 1: Accounts, deposits, transfers
- Level 2: Top spenders ranking (by total outgoing)
- Level 3: Scheduled payments (with cancel), processed before other ops at same timestamp; scheduled withdrawals count toward outgoing totals
- Level 4: Merge accounts and historical balance queries (GetBalance)

Files
- `banking_system_impl.hpp`: Class declaration and in-memory state
- `banking_system_impl.cpp`: Method implementations for Levels 1–4

Notes
- The public API comes from `banking_system.hpp` (provided by the testing harness). This repository only contains the implementation files that plug into that interface.
- All timestamps are strictly increasing across operations. Scheduled payments are processed before any other operation at a given timestamp.
- Merges move balance, combine outgoing totals, reassign pending scheduled payments, and preserve balance history for historical queries.

Building and testing
This repository does not include the test harness or `banking_system.hpp`. In the grading environment, tests are executed via scripts similar to:

- Run a single test case:
  ```sh
  bash run_single_test.sh "<test_case_name>"
  ```
- Run all tests (example):
  ```sh
  mkdir -p build && cp /catch2-utils/tests_runner.o build && make -s && ./build/tests_runner --use-colour=yes
  ```

High-level design
- Balances: `std::unordered_map<std::string, int>`
- Outgoing totals: `std::unordered_map<std::string, int>`
- Scheduled payments: global ordinal ids (`paymentN`), `paymentById_`, and `dueTimeToPaymentIds_` mapping due timestamp to creation-ordered lists
- Historical balances: per-account `balanceEvents_` storing `(timestamp, delta)`; `GetBalance` sums deltas up to `time_at`
- Merges: record merge edges `mergedInto_` with timestamp and adjust balances/outgoing/pending payments accordingly

License
- MIT (or align with your project’s default). 
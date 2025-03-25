# 🏛️ C++ Multi-Order Type Order Book

This project is a modular, high-performance **limit order book** implemented in modern C++20. It supports multiple order types and is built with clarity, testability, and extensibility in mind — making it ideal for HFT simulations, interview prep, or foundational exchange engines.

---

## ✨ Features

- ✅ **Price-Time Priority Matching**
- ✅ Supports 5 Order Types:
  - Good Till Cancel (GTC)
  - Fill and Kill (FAK)
  - Fill or Kill (FOK)
  - Good For Day (GFD)
  - Market Orders
- ✅ Thread-safe data access using `std::mutex`, `std::scoped_lock`, and atomics
- ✅ Clean modular design:
   - Clearly separated concerns: storage, matching, stats tracking, pruning
   - Organized private sections for:
        - `OrderStorage`-like structures (`bids_`, `asks_`, `orderLookup_`)
        - `MatchOrders()` and validation helpers
        - `LevelData` for tracking per-price statistics
        - `PruneGoodForDayOrders()` thread for timed cancellation
- ✅ Extensible and ready for testing with real-world scenarios

---



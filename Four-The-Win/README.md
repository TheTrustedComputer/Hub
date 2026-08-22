# Four the Win!

**Four the Win! (FTW)** is a high-performance Connect 4 solver written in modern C, designed for research and experimentation in computational game theory.

## Overview

FTW's goal is to analyze, prove, and visualize the outcomes of Connect 4 and its variants using efficient bitboard-based logic and game-tree search algorithms.

### Features

- **Supports** original Connect 4, Misere, Cylinder, PopOut, Pop 10, and Make 7.
- **Negamax** with alpha-beta pruning, iterative deepening, and prinicipal varation search.
- **Transposition table** keyed by position with a shallowest depth replacement strategy.
- **Dynamic move ordering** tuned for Connect 4 heuristics, enhancing cutoff frequency.
- **History heuristic** for prioritizing moves that have been successful in earlier searches.
- **Hybrid MCTS-PNS** engine combining probabilistic sampling with proof propagation.
- **SQLite caching** for the persistent lookup of already predetermined outcomes.
- **libdivide/fastmod** for runtime constant divisor and fast remainder arithmetic.
- **Command-line interface** for testing, benchmarking, and exploring positions.
- **Clean modular architecture**, no external dependencies beyond SQLite, libdivide, or fastmod.

### Works in Progress (WIPs)

- Root-level parallelization for multi-core scaling.
- Fixed-point game-theory analysis in cyclic games.
- Extended documentation and command-line reference.

## Linux Build Instructions

### Requirements

- A C23 compiler (GCC 14+, Clang 18+).
    - Clang is the recommended compiler for any computationally intensive tasks.
    - GCC is acceptable; performance differs by processor, workload, and scheduler noise.
    - PGO can improve throughput by an additional 5%; Make 7 greatly benefits from this increase.
    - Building with other compilers is at your own risk; we only support GCC and Clang.
- A glibc installation.
    - Linking against different C libraries (musl) may or may not succeed due to missing implementations of certain language features.
- Minimal example:
    - ```clang -std=c23 -lm FourTheWin.c```

### Optional Dependencies

Before downloading dependencies from their respective websites, consider checking your package manager for matching ones. Always rely on a trusted source! We are not responsible for any security breaches or system compromises resulting from visiting suspicious domains.

- [libdivide](https://github.com/ridiculousfish/libdivide)
- [fastmod](https://github.com/lemire/fastmod)
- [SQLite](https://sqlite.org)
- [GNU Make](https://www.gnu.org/software/make)

## Contributing

We welcome pull requests. If you have a suggestion for enhancing FTW, please submit it for review. Be sure to document your changes and briefly explain your reasoning. We do not mind if you use AI assistance, but please verify your work for inconsistencies. Discrepancies between your code and your description will cause unnecessary delays.
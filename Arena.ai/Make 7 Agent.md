# Make 7 Solver

**Build a smart, high-performance, interactive solver for the game Make 7 in modern ISO C**. If the toolchain supports C23, choose that language revision.

## Guidelines

* Avoid introducing C++ syntax, extraneous arrays, excessive dynamic allocation, and unnecessary dependencies.
* Create efficient, self-contained code, and document its structure and any non-obvious bit trickery.
* Use Allman-style braces, descriptive variable names, and appropriate indentation to enhance readability. Prefer ``static inline`` helpers to macros.
* Aggressively apply and nest type qualifiers wherever possible. Precompute frequently used read-only data with ``constexpr``, and ``const`` for constants that depend on a variable; consider declaring them static. Moreover, ``const`` clarifies that a scalar variable or a pointer's address and the value it references are immutable throughout its lifetime. ``restrict`` can offer a free boost for non-overlapping pointers.
* Prefer fixed-width integer types from ``<stdint.h>``. Integrate portable standard libraries like ``<threads.h>`` and ``<stdatomic.h>`` rather than POSIX threads for concurrency.

### Game-specific terminology

* Opt for distance to seven (DT7) for the number of plies preceding a forced win or loss.
* Refer to win/loss distance when discussing the generalized minimax concept.
* Distinguish between internal ply distances, encoded search scores, and displayed win/loss distances.
* Do **NOT** adopt chess-derived vocabulary, such as *mate*, *checkmate*, *stalemate*, *mating sequence*, or *centipawn*. Generalize to *win*, *draw*, *forcing* sequence, centi*unit*, so on.
* When adapting algorithms from chess-engine literature, translate them without altering the underlying mathematical behavior.

For additional guidelines, refer to ``AGENTS.md`` in the attachment. When ready, **share the complete, compilable code**, along with a brief explanation of the program in a ``README.md`` file. Write the win detector and other technical notes or findings in ``TECHNICAL.md``.

## Game Rules

* The board is a blank 7x7 grid for two players.
* Each player begins with eleven #1 tiles, eleven #2 tiles, and four #3 tiles.
* A move involves selecting a player's tile and a non-full column. Gravity drops the tile into the lowest empty cell.
* #1 and #2 tiles may land anywhere, but #3 tiles may only land in designated cells. Let A-G denote columns from left to right, and 1-7 denote rows from bottom to top. The permitted cells are A3, B5, C6, D4, E6, F5, and G3.
* Dropped tiles are permanent and removed from the player's reserve. The turn passes to their opponent.
* A player wins by forming a contiguous horizontal, vertical, or diagonal alignment of their tiles that sums to seven. There are two interpretations:
    * Exact - the complete maximal run must sum to seven. ``[1][2][2][3]`` = 8, which does not win.
    * Window - a contiguous sub-run within it may sum to seven. ``[1][2][2][3]`` wins with ``[2][2][3]`` = 7.
* If a player has no legal moves on their turn, the game is a draw. The board does not need to be full.
* Express the move notation as ``<tile><column>``: ``2D`` indicates a #2 tile in column D.


## Structure

Below is a Make 7 position representation from an existing C game engine. Stick rigidly to this structure and its bitboard layout. Do not refactor it.

```c
typedef struct
{
    uint64_t mask, side;
    uint64_t tile1, tile2;
    uint64_t lastT;
    uint32_t avails;
    bool turn;
}
Make7;

// Column-major order + full sentinel bit
// Make7.mask => mask of all occupied cells
// Make7.side => mask of mover's tiles
// Make7.tile1 => mask of tiles of value 1
// Make7.tile2 => mask of tiles of value 2
// Make7.lastT => single bit of last placed tile
// Make7.avails => packed field of player's reserves
// Make7.turn => side-to-move flag: 0 = P1; 1 = P2

// Layout of Make7.avails:
//  P2_3 P2_2  P2_1   P1_3 P1_2  P1_1
// [100][1011][1011] [100][1011][1011]
// Bits 0-10 => P1's off-board tiles
// Bits 11-21 => P2's off-board tiles
// Initial value => 0x25dcbb
```

## Engines

The solver equips two search engines: **Principal Variation Search** and **Monte Carlo Tree Search**.

### Principal Variation Search

Principal Variation Search, or NegaScout, improves upon alpha-beta pruning by performing a null-window search around the "principal variation," or the optimal sequence of moves. Combined with effective move ordering, it discards more game tree branches than alpha-beta in isolation. However, if the initial assumption is incorrect, it necessitates a full-window re-search. Use the negamax variant and capitalize on game-agnostic optimizations, such as:

* Transposition tables cache positions that have been reached from multiple paths.
* Fail-soft alpha-beta returns terminal scores that extend beyond the current bounds.
* Iterative deepening populates the search space with pertinent values and quickly pinpoints forcing lines.
* History heuristics reattempt moves that previously generated branch cutoffs, or "global killers."
* Lazy SMP runs the search across diversified threads that share a transposition table.
* Thread voting aggregates the strongest moves from each thread to achieve consensus.

Regarding selective search heuristics:

* Tactical extensions may be used to artificially augment search depth when positions allow.
* Late move reductions narrow the scope of examination as the move array approaches its end.
* Aspiration windows commence narrowly and widen based on fail-highs and fail-lows.

Exercise caution when incorporating search heuristics into NegaScout. This transforms the engine into a formidable competitor rather than an authoritative assistant. It trades accuracy for speed, so introduce a toggle with a corresponding warning (Oracle/Expert). Deploy any applicable search optimization techniques not listed above, provided they maintain correctness. Upgrade the move generator with an inexpensive tactical filter that discards moves that lead to an instant loss unless defeat is inevitable, thereby reducing the branching factor without affecting minimax values. Return directly if loss is imminent.

In Oracle mode, both outcomes and distances are exact; NegaScout can finish executing. In Expert mode, however, once a win or loss is discovered, the predicted distance is an upper bound. A final verification pass is required to match Oracle. This occurs when both the lower and upper distance bounds coincide (i.e., the result cannot be improved upon in that number of moves). Do not reinitialize NegaScout merely to do this. Resetting the depth technically constitutes a reinitialization. Stay within the existing search framework without losing accumulated progress.

Each entry in the transposition table should occupy no more than sixteen bytes of memory. If possible, reserve the remaining bits for collision handling, replacement, aging, and so on. See the example below. Strict adherence to this precise structure is not obligatory.

* 64-bit hash or lock
* 16-bit score
* 6-bit depth (49 cells maximum)
* 5-bit move (at most 21 moves)
* 2-bit bound (lower/exact/upper)

Make 7 has not undergone thorough analysis; do not assume an established evaluation function exists. Instead, design a compact, fast heuristic that can be invoked at many large leaf nodes. The heuristic should be sensitive enough to distinguish such subtleties across board states (i.e., fewer equal scores among candidates mean more opportunities for ordering). Ensure its weight vectors are tunable via the self-play lab by pitting challengers against the champion and retaining the stronger set. The evaluation should cover strategically influential features like:

* Neighboring friendly runs and their distances to seven
* Whether those runs can be legally extended into wins
* Immediate threats and winning continuations
* Opponent threats and requisite defensive maneuvers
* Mobility and remaining legal moves
* Available inventory of #1, #2, and #3 tiles
* Accessibility and tactical significance of #3 target cells
* Board geometry and line participation

Rewarding higher-valued tiles solely because ``3 > 2 > 1`` is a mistake. A #3 tile should be beneficial due to the complications or configurations it creates. To construct a useful move-ordering scheme, arrange tile values from highest to lowest, prioritizing critical actions such as immediate wins, forced defenses, and transposition-table moves. Wins, draws, and losses dominate every heuristic. A neutral score does not necessarily prove a draw. Rather, it signals that the engine has deemed the position balanced thus far. The scheme should reflect this pattern:

1. Immediate wins
2. Forced defenses
3. Transposition table moves
4. History moves
5. Descending tile values
6. Column precedence

### Monte Carlo Tree Search

Monte Carlo Tree Search (MCTS) is a search algorithm for decision-making in vast, potentially unbounded state spaces. Instead of considering all possibilities, it intelligently selects a subset, expands some states, and runs a stochastic simulation on that subset. The reward is then derived from simulation results and propagated backward toward the root. Unlike minimax, MCTS eliminates the need for an evaluator; it learns which branches are most promising through simulations and a selection policy, typically UCT.

Brainstorm an alternative to purely uniform random rollouts. For example, MAST (Move-Average Sampling Technique) is a heuristic that biases toward successful moves from past simulations. NST (N-gram Selection Technique) builds on MAST by incorporating local sequences. Nevertheless, randomness is integral to preventing overfitting, so employ a method like epsilon-greedy.

Program a parallel, tactical MCTS solver that carries win/draw/loss information within each node. This will prompt it to stop once the root node is decided. When an expanded node is terminal, disregard exploration and prune its branches. Pruning decreases memory usage and concentrates computing resources on unresolved subtrees. Avoid relying on playouts to determine termination. Rely on a principled minimax approach during backpropagation. Let ``s`` be a state, ``a`` be an action, and ``S(s, a)`` be the successor of ``s`` after taking ``a``:

1. ``WIN(s) <=> ∃a LOSS(S(s, a))``

    "``s`` wins if and only if there exists an ``a`` such that ``S(s, a)`` loses."

2. ``DRAW(s) <=> ∃a DRAW(S(s, a)) ∧ ∀a ¬LOSS(S(s, a))``

    "``s`` draws if and only if there exists an ``a`` such that ``S(s, a)`` draws and ``S(s, a)`` does not lose for all ``a``."

3. ``LOSS(s) <=> ∀a WIN(S(s, a))``

    "``s`` loses if and only if ``S(s, a)`` wins for all ``a``."

4. Otherwise, ``s`` is inconclusive.

An MCTS node should be between thirty-two and sixty-four bytes, and the tree grows indefinitely. Consequently, let the operating system kill the process when virtual memory is depleted. Be mindful of custom allocators. The cost of traversing a large free list may exceed that of a naive ``malloc()``/``free()``.

* 8-byte reward
* 8-byte visit count
* Pointers to parent/child nodes (8 bytes on 64-bit targets)
* 5-bit child count
* 5-bit move
* 2-bit proof (none/loss/draw/win)

During the rollout phase, take advantage of any winning or defensive situations that arise. Do not expand obviously losing nodes (i.e., the opponent wins on their turn). If the rollouts reach a solved position at any point, discontinue them and assign the solution reward. When selecting nodes, bypass any solved ones. These measures should increase playing strength and search efficiency.

Provide draw reward shaping while conserving the existing scale. Raise the reward when a node's empirical value is below neutral and lower it when it is above neutral, converging to the original reward at equilibrium. Apply stochastic rounding if fractional adjustments are warranted. Make shaping user-configurable with a default of disabled.

## User Interface

The interface is a text-based read-eval-print loop (REPL). The solver application begins by initializing the game state, showing the initial board state and other metrics, such as tile reserves. Then, it waits for user commands and accepts move notation as input, with or without whitespace or tabs. Do not vaguely inform users when they enter invalid commands. Instead, instruct them to type the help command. All messages are suggestive.

```
Command not understood. Type ? for help.
```

The shell prompt is ``[PLAYER P]`` with a trailing space, where ``P`` is either ``1`` or ``2`` depending on whose turn it is. The shorthand commands are:

* ``?`` - command menu
* ``l`` - self-play tuning **l**ab
* ``m`` - list legal **m**oves
* ``n`` - **n**umber of plies (half-moves)
* ``o`` - toggle **o**racle/expert
* ``p`` - **p**rint board
* ``q`` - **q**uit program
* ``r`` - **r**estart game
* ``s`` - **s**olve position (PVS)
* ``t`` - run MC**T**S
* ``u`` - **u**ndo last move
* ``v`` - mo**v**e history
* ``w`` - switch **w**in rule (window/exact)

``l``'s optional parameters are ``[gr] [pd] [mu] [rd]``, where:
* ``[gr]`` - game rounds (default: 100)
* ``[pd]`` - playing depth (default: 8)
* ``[mu]`` - mutation intensity (default: 0.25)
* ``[rd]`` - maximum random opening ply (default: 7)

Word commands:

* ``epsilon [E]`` - get/set MAST/NST epsilon to ``E`` (default: 0.25)
* ``evaluate`` - heuristic evaluation at this position
* ``perft <D>`` - performance test to depth ``D``
* ``shape [K]`` - get/set draw shaping to ``K``
* ``table [N]`` - get/set transposition table size to ``2^N`` bytes (default: 16 MiB)
* ``thread [T]`` - get/set thread count to ``T`` (default: OS-provided, else 1)
* ``uct [C]`` - get/set UCT exploration constant to ``C`` (default: square root of 2)
* ``weights`` - dump champion's weight vector

### Command Specifications

#### **``?``**

Organize the menu as follows, inserting a leading space to each command:

```
Short commands:
 ? - command menu
 l [round] [depth] [mutation] [randply] - self-play tuning lab
...
Long commands:
 epsilon [E] - get/set MAST/NST epsilon
 evaluate - heuristic evaluation
...
```

#### **``l``**

The laboratory runs continuously until the user presses Ctrl+C. See the telemetrics section for advice. In any case, the mutated challenger and defending champion play a paired game, exchanging sides per match. Print a status message for each round, recording wins, draws, and losses from the challenger's perspective.

```
Tuning weight vector (63/100)
Challenger score: W25 D4 L34
```

After a tuning generation, if the champion beats the challenger, conclude with:

```
Champion defended.
```

Or vice versa:

```
Challenger promoted.
```

A challenger promotes when they outperform the champion by at least 55%.

#### **``m``**

Sort the columns from left to right, then from smallest to largest face value. Space out each move.

```
1A 1B 1C 1D 1E 1F 1G 2A 2B 2C 2D 2E 2F 2G 3A 3B 3C 3D 3E 3F 3G
```

#### **``n``**

Print the raw plies from the initial position. If no moves have been made, this is zero.

#### **``o``**

Acknowledge when either oracle or expert mode is active.

```
Oracle mode: outcomes and distances are exact.
```

```
Expert mode: outcomes may resolve sooner, but distances are tentative until a verification pass tightens them.
```

#### **``p``**

Utilize ANSI escape codes for colorful output:

* A ``[]`` represents an empty cell with a bright blue foreground and a dark blue background. A red foreground signifies a #3 destination cell.
* Player 1's tiles have a vivid green background, and Player 2's have a vivid yellow background. Emphasize each digit with black, full-width characters. Color the prompt to correspond to the mover, but keep the background identical to the console's.
* At the bottom of the board, print the tile quantity for the current player.
* Preserve spacing between columns and rows for legibility when read in a monospace typeface.
* Make sure the board is oriented accordingly by starting with the top rows and tracing downward.

```
[][][][][][][]
[][][][][][][]
[][][][][][][]
[][][][][][][]
３[][][][][][]
２[][][][][][]
１[][][][][][]
1x11 2x11 3x4
```

#### **``q``**

Before quitting, clean up any allocated resources.

#### **``r``**

Reprint the board to communicate to the user that the game has restarted.

#### **``s``**

Format the NegaScout telemetry and print each block on the same line every quarter second. In other words, reset the cursor and overwrite the old information with new data. Clear any stale characters from the console by padding with spaces.

```
score [sc]
depth [nd]/[sd]
node  [n]
speed [s]
time  [t]
ttuse [tu]
tthit [th]
move  [m] [im]/[tm]
line  [l]
```

Key:

* ``[sc]`` = heuristic score / win distance
* ``[nd]`` = nominal ply depth
* ``[sd]`` = selective ply depth
* ``[n]`` = node count
* ``[s]`` = speed in nodes per second
* ``[t]`` = elapsed time in seconds
* ``[tu]`` = table usage in percent
* ``[th]`` = table hit count
* ``[m]`` = current move
* ``[im]`` = move index
* ``[tm]`` = total moves
* ``[l]`` = principal variation

Below is an illustration of the output.

```
score 42
depth 10/20
node  1234567890
speed 10288065
time  120
ttuse 37%
tthit 12345
move  1F 17/21
line  2D 1A 2B 3C 1D 2E 3F 1G
```

Once NegaScout finds a solution, display a bold, vibrant green ``Wn`` (win), red ``Ln`` (loss), or yellow ``DRAW`` when the outcome is guaranteed in ``n`` moves, and stop. The distance starts at zero, so, parity-wise, wins are even, and losses are odd, unlike the internal distance. If the player can win immediately, show ``WIN``, not ``W0``. Draws have no defined distance. In all other cases, present an evaluation score.

```
score W10
```

#### **``t``**

Ditto for MCTS. Report only ``WIN``/``DRAW``/``LOSS`` if a solution has arrived for the root node; unproven nodes are ``NONE``. Override the win rate to 100%, 50%, and 0% for proven wins, draws, and losses, respectively. Apply the same colors as NegaScout.

```
rate   [r]
depth  [rd]/[md]
iters  [i]
speed  [s]
time   [t]
visits [v]
solved [n]
proof  [p]
line   [l]
```

Key:

* ``[r]`` = win rate in four significant figures
* ``[rd]`` = selected depth from the root
* ``[md]`` = maximum depth
* ``[i]`` = MCTS iterations
* ``[s]`` = speed in iterations per second
* ``[t]`` = elapsed time in seconds
* ``[v]`` = percentage of visits versus siblings
* ``[n]`` = total solved nodes
* ``[p]`` = root minimax proof
* ``[l]`` = principal variation

Illustration:

```
rate   49.99%
depth  10/20
iters  1234567890
speed  10288065
time   120
visits 37%
solved 12345
proof  NONE
line   2D 1A 2B 3C 1D 2E 3F 1G
```

#### ``u``

Reprint the board to mirror the undone move. Do not do so if there is nothing to undo.

#### ``v``

A series of moves strung together into a single line.

```
1A1B1C1D1E1F1G2A2B2C2D2E2F2G3A3B3C3D3E3F3G
```

#### ``w``

Announce the switched rule.

```
Window rule: any consecutive sub-run of tiles may sum to seven.
```

```
Exact rule: the whole maximal run of tiles must sum to seven.
```

#### ``evaluate``

Print the heuristic score unchanged.

#### ``perft``

Print the performance test results for the current position. Below are the actual computations for the empty board to depth eight under the window rule. Further results are in the tips area.

```
1622584196 nodes | 844882017 #1s | 844882017 #2s | 53517784 #3s | 4.848834128 secs | 334633883 node/sec
```

#### ``weights``

Print descriptive labels for the weight vectors per line while not being verbose.

```
Some reward: 1
Some penalty: -1
...
```

#### Getters and Setters

If an argument is not specified, reveal the current value. Otherwise, set it silently. Only print in the event of an error or failure.

```
Could not set <key> to <value>: <reason>.
```

## Telemetrics

Do not provide the user with feedback when the search finishes. The search telemetry is responsible.

Wire up a SIGINT handler to abort the search when the user hits Ctrl+C. Furthermore, run the telemetry on a separate thread. This frees the search threads from costly time-interval checks, especially for MCTS. Conversely, they would likely incur the small overhead of polling the stop flag. Activate the handler before search, deactivate it at the end, and make it reusable.

## Challenge

Win detection is expected to be the hottest part of the game logic. One straightforward solution runs in ``O(R)``, where ``R`` is the length of the friendly run through the last placed tile and ``R ≤ 7``. Using bitboards, arithmetic, population counts, lookup tables, or other optimization techniques, try to derive a genuine fixed-work ``O(1)`` detector for both rule variants. Carefully profile it against the ``O(R)`` approach, especially when integrating it into search engines. If feasible, construct loops whose iteration count does not depend on ``R``. Explain the resulting algorithm and why it is correct and, most importantly, demonstrate that it runs *faster* on average across several positions. Counterexamples are welcome if the fixed-work algorithm ultimately yields slower in game tree search workloads.

## Test Positions

A few solvable positions are open to debugging the search engine, static evaluator, and game logic. A properly multithreaded and optimized engine with fine-tuned weights, advanced search, and domain knowledge could solve them in _seconds_, not minutes or hours. The fewer nodes and wall time, the better. Consider including additional weights or pruning strategies based on observations. Experiment with them to see whether they increase solving efficiency across different game states. Never expose positions to the interface, as they are intended for testing and profiling.

| Move Sequence | Game-Theoretic Result | Notes |
| - | - | - |
| 2C2C2D2D2E | L1 (Window and Exact) | Double-threat detection in 1 node
| 2D2D2B2D | W20 (Window and Exact) |
| 2D2D2C2D | W14 (Window and Exact) |
| 2D2D2C2D3D1E2A | L11 (Window), L15 (Exact) |
| 2D2D2C2D3D1B1F | L17 (Window), L15 (Exact) |
| 2D2D2D2D2D2D2D2C2C2C2C2C2C2C2E2E2E2E2E2E1B1E1B | DRAW (Window), W24 (Exact) |

## Tips

* To reiterate, minimize chess terminology throughout the codebase. For instance, replace *mate* distances with game-specific phrasing, like distance to *7*. Generalizing to *win* distances is also a legitimate compromise. The design document does not reference "mate" anywhere because Make 7 is not a chess variant.
* Conduct a performance test that counts terminals as leaves and record the number of nodes per second. An increase in this figure without errors suggests successful throughput gains. Alternatively, simulate multiple matches with random movers until identifying a winner. Repeat until exceeding the desired number of rounds. See the data from an independently developed program below to verify these metrics.

    Perft depth 1 to 7 (both rules):
    * 14 nodes
    * 196 nodes
    * 2752 nodes
    * 38824 nodes
    * 550864 nodes
    * 7860408 nodes
    * 112845266 nodes

    Perft depth 8 (winning positions enter the picture):
    * 1622584196 nodes (window)
    * 1622675022 nodes (exact)

    Perft depth 9:
    * 23491865352 nodes (window)
    * 23494640532 nodes (exact)

    Perft depth 10 (may take a long time and necessitate an opt-in smoke test):
    * 339588991914 nodes (window)
    * 339961457388 nodes (exact)

    The program achieved an ``O(R)`` throughput of around 330 million nodes per second when compiled with ``-O3 -flto -march=native`` on a Zen 3 processor. These speed benchmarks may vary depending on the hardware, compiler, operating system, workload, and scheduler noise.

* Only the last placed tile may produce a win. Conversely, no tile above it can win due to gravity. Resist the temptation to scan the entire board; however, review it for debugging purposes, if desired. Reaching seven requires at least three tiles, so ignoring shorter runs would save significant computational time. In fact, the sliding-window technique is not required for runs of exactly three tiles.
* As for move generation, examine only the relevant columns and skip the rest. While iterating over all seven columns is reasonable, iterating just over the set columns is better because it eliminates wasted effort. Bitboards are sufficient for deriving the legal move mask.
* Although not mandatory by the specification, toggling between the two win rules via function pointers could be more effective than conditionals or build-time options, particularly for improving the user experience as the code evolves.
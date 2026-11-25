# Make 7 Solver

**Build a smart, high-performance, interactive solver web app for the game Make 7.**

## Rules

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

## Engines

The solver equips two search engines: **Principal Variation Search** and **Monte Carlo Tree Search**.

### Principal Variation Search

Principal Variation Search, or NegaScout, improves upon alpha-beta pruning by performing a null-window search around the "principal variation," or the optimal sequence of moves. Combined with effective move ordering, it discards more game tree branches than alpha-beta in isolation. However, if the initial assumption is incorrect, it necessitates a full-window re-search. Use the negamax variant and capitalize on game-agnostic optimizations, such as:

* Transposition tables cache positions that have been reached from multiple paths.
* Fail-soft alpha-beta returns terminal scores that extend beyond the current bounds.
* Iterative deepening populates the search space with pertinent values and quickly pinpoints forcing lines.
* History heuristics reattempt moves that previously generated branch cutoffs, or "global killers."

Regarding selective search heuristics:

* Tactical extensions may be used to artificially augment search depth when positions allow.
* Late move reductions narrow the scope of examination as the move array approaches its end.
* Aspiration windows commence narrowly and widen based on fail-highs and fail-lows.

Exercise caution when incorporating search heuristics into NegaScout. This transforms the engine into a formidable competitor rather than an authoritative assistant. It trades accuracy for speed, so introduce a toggle with a corresponding warning (Oracle/Expert). Deploy any applicable search optimization techniques not listed above, provided they maintain correctness. Upgrade the move generator with an inexpensive tactical filter that discards moves that lead to an instant loss unless defeat is inevitable, thereby reducing the branching factor without affecting minimax values. Return directly if loss is imminent.

In Oracle mode, both outcomes and distances are exact; NegaScout can finish executing. In Expert mode, however, once a win or loss is discovered, the predicted distance is an upper bound. A final verification pass is required to match Oracle. This occurs when both the lower and upper distance bounds coincide (i.e., the result cannot be improved upon in that number of moves). Do not reinitialize NegaScout merely to do this. Resetting the depth technically constitutes a reinitialization. Stay within the existing search framework without losing accumulated progress.

Each entry in the transposition table should occupy no more than eight bytes of memory. If possible, reserve the remaining bits for collision handling, replacement, aging, and so on. See the example below. The size is nonnegotiable, but strict adherence to this precise structure is not obligatory.

* Partial hash or lock
* 16-bit score
* 6-bit depth (49 cells maximum)
* 5-bit move (at most 21 moves)
* 2-bit bound (lower/exact/upper)

Pay particular attention to collisions, especially when searching in Oracle mode or verifying Expert win distances. Distinct position keys may yield the same partial lock. Neglecting this scenario would contaminate the transposition table with entries that appear valid but are incorrect for the given search context. Nonetheless, this tradeoff is tolerable in Expert mode.

Make 7 has not undergone thorough analysis; do not assume an established evaluation function exists. Instead, design a compact, fast heuristic that can be invoked at many large leaf nodes. The heuristic should be sensitive enough to distinguish such subtleties across board states (i.e., fewer equal scores among candidates mean more opportunities for ordering). Ensure its weight vectors are tunable via the self-play lab by pitting challengers against the champion and retaining the stronger set. The evaluation should cover strategically influential features like:

* Neighboring friendly runs and their distances to seven
* Whether those runs can be legally extended into wins
* Immediate threats and winning continuations
* Opponent threats and requisite defensive maneuvers
* Mobility and remaining legal moves
* Available inventory of #1, #2, and #3 tiles
* Accessibility and tactical significance of #3 target cells
* Board geometry and line participation

Rewarding higher-valued tiles by their face value is a mistake. A #3 tile should be beneficial due to the complications or configurations it creates. To construct a useful move-ordering scheme, arrange tile values from highest to lowest, prioritizing critical actions such as immediate wins, forced defenses, and transposition-table moves. Wins, draws, and losses dominate every heuristic. A neutral score does not necessarily prove a draw. Rather, it signals that the engine has deemed the position balanced thus far. The scheme should reflect this pattern:

1. Immediate wins
2. Forced defenses
3. Transposition table moves
4. History moves
5. Descending tile values
6. Column precedence

### Monte Carlo Tree Search

Monte Carlo Tree Search (MCTS) is a search algorithm for decision-making in vast, potentially unbounded state spaces. Instead of considering all possibilities, it intelligently selects a subset, expands some states, and runs a stochastic simulation on that subset. The reward is then derived from simulation results and propagated backward toward the root. Unlike minimax, MCTS eliminates the need for an evaluator; it learns which branches are most promising through simulations and a selection policy, typically UCT.

Brainstorm an alternative to purely uniform random rollouts. For example, MAST (Move-Average Sampling Technique) is a heuristic that biases toward successful moves from past simulations. NST (N-gram Selection Technique) builds on MAST by incorporating local sequences. Nevertheless, randomness is integral to preventing overfitting, so employ a method like epsilon-greedy.

Program an MCTS solver that carries win/loss information within each node, excluding draws (see the next section for an explanation). This will prompt it to stop once the root node is decided. When an expanded node is terminal, disregard exploration and prune its branches. Pruning decreases memory usage and concentrates computing resources on unresolved subtrees. Avoid relying on playouts to determine termination. Rely on a principled minimax approach during backpropagation. Let ``s`` be a state, ``a`` be an action, and ``S(s, a)`` be the successor of ``s`` after taking ``a``:

1. ``WIN(s) <=> ∃a LOSS(S(s, a))``

    "``s`` wins if and only if there exists an ``a`` such that ``S(s, a)`` loses."

2. ``DRAW(s) <=> ∃a DRAW(S(s, a)) ∧ ∀a ¬LOSS(S(s, a))``

    "``s`` draws if and only if there exists an ``a`` such that ``S(s, a)`` draws and ``S(s, a)`` does not lose for all ``a``."

3. ``LOSS(s) <=> ∀a WIN(S(s, a))``

    "``s`` loses if and only if ``S(s, a)`` wins for all ``a``."

4. Otherwise, ``s`` is inconclusive.

An MCTS node should fit within sixteen bytes, accounting for overflow or rollover during prolonged analyses:

* 32-bit reward
* 32-bit visit count
* 32-bit parent or first child offset
* 5-bit child count
* 5-bit move
* 2-bit proof (none/loss/draw/win)

Keep in mind that storing the raw win rate as a 32-bit floating-point number will cause updates to stagnate before visits saturate. Most importantly, do not stop when the tree becomes full. Instead, perform a generational reset and garbage collection to restart with informed statistics from previous searches. Refrain from clearing everything.

During the rollout phase, take advantage of any winning or defensive situations that arise. Do not expand obviously losing nodes (i.e., the opponent wins on their turn). If the rollouts reach a solved position at any point, discontinue them and assign the solution reward. When selecting nodes, bypass any solved ones. These measures should increase playing strength and search efficiency.

Provide draw reward shaping while conserving the existing scale. Raise the reward when a node's empirical value is below neutral and lower it when it is above neutral, converging to the original reward at equilibrium. Apply stochastic rounding if fractional adjustments are warranted. Make shaping user-configurable with a default of disabled.

### Coding Language

Please develop the engine in an appropriate language and compile it to WebAssembly. **Install any dependencies** if needed. Leverage Memory64 to access a large, dedicated linear memory containing the transposition table and MCTS node pool. NegaScout and MCTS are mutually exclusive and thus share the same region. Store everything else (e.g., board, moves, indices, scores, search, etc.) in separate WASM32 memory (i.e., multi-memory). The WebAssembly engine manages the game state and drives the search, while the application serves as a thin presenter around it. Allow the user to change the arena size, the exploration constant, and the epsilon parameter.

The JavaScript API limits WASM64 modules to a maximum of 16 GiB of addressable memory, browser permitting. Allocate the entire amount when the user resizes the arena. For instance, if the user wants the whole 16 GiB (2^31 TT entries; 2^30 MCTS nodes), the browser should convey that figure verbatim. Ideally, there should be no clamping. If clamping happens, reassess the engine's memory layout to accommodate the requested space.

Do **NOT** ignore this request without a compelling reason. "Too complex," "too complicated," and "too challenging" are not valid excuses. Check if any Vite configuration files are editable where permissible. If not, wire a build script and import it into a file called ``postcss.config.cjs`` or ``postcss.config.js``. Create the file if it does not exist. While its primary purpose is to load PostCSS plugins, it can also act as a build hook for the engine. Do not insert any plugins; they are unnecessary. Skip this step if you have a shell to run commands. Whenever practical, update the toolchain to one that has first-class Memory64 and multi-memory support.

If compilation is truly unattainable in the build environment, explain why WebAssembly will not build with the given toolchain, and make sure the engine code builds with WASM64 when the user clones the project locally. This conversion should be seamless and automatic. Most modern, mainstream browsers already support multi-memory and Memory64. Swap them if this one does not recognize these features. Investigate other avenues before falling back to TypeScript as an absolute last resort.

In any case, you have two options: **AssemblyScript** and **WAT**.

#### AssemblyScript

AssemblyScript is a TypeScript dialect tailored for low-level WebAssembly programming. It lets TypeScript programmers write AssemblyScript code without learning a new language. The compiler then translates it into WebAssembly bytecode. If any stage of the compilation pipeline fails, the code reverts to standard TypeScript.

The AssemblyScript compiler partially supports Memory64. Although AssemblyScript's official status for Memory64 is listed as uncertain, the compiler accepts the flag ``--enable memory64``. To be certain, probe its ability to read and write beyond 4 GiB of memory before committing. Unfortunately, multi-memory support is lacking, but since NegaScout and MCTS are mutually exclusive, this should not be a major concern. Lastly, compile the engine with speed optimizations using ``-Ospeed``.

#### WAT

WAT refers to the WebAssembly text format. It is a human-readable, debug-friendly rendition of WebAssembly bytecode. Unlike AssemblyScript, it fully supports Memory64 and multi-memory. This grants NegaScout and MCTS access to all memory independent of allocations from other parts of the engine.

Nonetheless, debugging will be more involved if something goes wrong. Therefore, thoroughly review the code for logical inconsistencies and test incrementally before deeming it ready. Furthermore, optimizations are not free without compilers. Even if the hand-assembled code is clean and meets the specification, speed discrepancies may occur. As such, inspect the code for any performance enhancements.

Ultimately, pick your poison. A mixture of AssemblyScript and WAT is acceptable. The former can harness the engine's WASM32 portions, and the latter can do the same with the WASM64 arena. The AssemblyScript compiler can also print in WAT; thus, it may be worthwhile to compare your WAT with the compiler's output. Moreover, achieving full Memory64 support in AssemblyScript likely entails patching the compiler's emitted WAT to use 64-bit offsets.

## User Interface

Choose a minimalist dark theme to reduce eye fatigue from bright screens while preserving vivid, distinct tile colors. Player 1's tiles should be green, and Player 2's should be yellow. All digits should be black. The shades do not have to be perfect, but they should be consistent for each player. Do not label the #3 landing cells with the numeric glyph. Delineate them with a red border, as in the physical game. The tiles should be square with large numbers at the center. Dropping animations and sound effects are optional.

The solver conducts an infinite, live analysis of a position, is always visible, and runs on demand. This analysis operates independently of the main thread on a compatible architecture to keep the interface responsive and can be enabled or disabled at any time. If the user cannot interact with the interface while the solver is active, the interface is unresponsive. Leave it to run its course until it finds a solution or the user interrupts; never abort or loop it prematurely. Place a best-move token on the board according to the solver's recommendation. New positions must safely cancel or supersede obsolete analyses. Honestly announce which backend the engine is running on: WASM64, WASM32, or TypeScript.

In addition, facilitate monitoring of the solver's thought process. For NegaScout:

* Nominal depth
* Selective depth
* Principal variation
* Node count
* Transposition table usage and hits
* Elapsed time
* Speed

As for MCTS:

* Selected node's depth from the root
* Principal variation
* Win rate to four significant figures
* Current move's visits compared to others (exclude root visits)
* Playouts or iterations
* Total solved nodes
* Elapsed time
* Speed

Once NegaScout finds a solution, display a green ``Wn`` (win), red ``Ln`` (loss), or yellow ``DRAW`` when the outcome is guaranteed in ``n`` moves, and stop. The distance starts at zero, so, parity-wise, wins are even, and losses are odd, unlike the internal minimax value. If the player can win immediately, show ``WIN``, not ``W0``. Draws have no defined distance. In all other cases, present an evaluation score. Report ``WIN``/``DRAW``/``LOSS`` if MCTS arrives at a solution for the root.

The user may choose whether the AI opponent goes first or second, adjust its skill level, and freely undo, redo, edit moves, and restart the game as they see fit. Note the human-human and AI-AI modes. Set the players to two humans.

Add keyboard shortcuts to the interface. Pressing 1-3 picks a tile, and pressing A-G drops it into the corresponding column (case-insensitive). The left and right arrow keys advance the game backward and forward one move, respectively. Similarly, the Home and End keys navigate to the earliest and latest positions. Adapt the interface for desktop and mobile clients.

The desktop interface should contain three panels in this order:

* Left - move history, input field
* Center - board, game controls
* Right - analysis, engine settings

As for mobile:

* Top - board, controls
* Middle - moves, inputs
* Bottom - analysis, settings

Orient the board accordingly by starting with the top rows and tracing downward:

```
  A B C D E F G
7 - - - - - - -
6 - - * - * - -
5 - * - - - * -
4 - - - * - - -
3 * - - - - - *
2 - - - - - - -
1 - - - - - - -
```

Legend:

* ``A-G`` - columns (left-to-right)
* ``1-7`` - rows (bottom-to-top)
* ``-`` - empty cells
* ``*`` - #3 cells

## Test Positions

A few solvable positions are open to debugging the search engine, static evaluator, and game logic. A properly optimized engine with fine-tuned weights, advanced search, and domain knowledge could solve them in _seconds_, not minutes or hours. The fewer nodes and wall time, the better. Consider including additional weights or pruning strategies based on observations. Experiment with them to see whether they increase solving efficiency across different game states. Never expose positions to the interface, as they are intended for testing and profiling.

| Move Sequence | Game-Theoretic Result | Notes |
| - | - | - |
| 2C2C2D2D2E | L1 (Window and Exact) | Double-threat detection in 1 node
| 2D2D2B2D | W20 (Window and Exact) |
| 2D2D2C2D | W14 (Window and Exact) |
| 2D2D2C2D3D1E2A | L11 (Window), L15 (Exact) |
| 2D2D2C2D3D1B1F | L17 (Window), L15 (Exact) |
| 2D2D2D2D2D2D2D2C2C2C2C2C2C2C2E2E2E2E2E2E1B1E1B | DRAW (Window), W24 (Exact) |

## Tips

* Minimize chess terminology throughout the codebase. For instance, replace *mate* distances with game-specific phrasing, like distance *to 7*. Generalizing to *win* distances is also a legitimate compromise. The design document does not reference "mate" anywhere because Make 7 is not a chess variant.
* Win detection is expected to be the hottest part of the game logic; optimize carefully, especially when integrating it into search engines.
* Since only the last played tile, excluding those above it, may produce a win, restrict runs containing that tile and test the four axes through it. Resist the temptation to scan the entire board.
* Reaching seven requires at least three tiles. Ignoring shorter runs will save significant computational time. In fact, the sliding-window technique is not essential for runs of exactly three tiles.
* Represent the position as integers and perform bitwise and bit-parallel operations on them. This will significantly boost throughput over mailboxes. One suitable representation is described below:
    * Bitboard 1 - occupancy mask
    * Bitboard 2 - owner's tile mask
    * Bitboard 3 - tile #1 mask
    * Bitboard 4 - tile #2 mask
    * The tile #3 mask is implicit.
* Verify that the move generator is working correctly by comparing its perft numbers (terminals are leaves) against the baseline. The numbers differ from the two rules beginning at depth eight when winning positions enter the game. Depths nine and higher would take too long to compute.
    | Depth | Nodes (Exact / Window) | Difference |
    | - | - | - |
    | 1 | 14 / 14 | 0 |
    | 2 | 196 / 196 | 0 |
    | 3 | 2752 / 2752 | 0 |
    | 4 | 38824 / 38824 | 0 |
    | 5 | 550864 / 550864 | 0 |
    | 6 | 7860408 / 7860408 | 0 |
    | 7 | 112845266 / 112845266 | 0 |
    | **8** | **1622675022** /  **1622584196** | **90826**
    | **9** | **23494640532** / **23491865352** | **2775180**
    | **10** | **339961457388** / **339588991914** | **372465474**
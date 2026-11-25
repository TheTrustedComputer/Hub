# Connect 4 PopOut Solver

**Build a high-performance, interactive solver for Connect 4 PopOut**. Presume that the classic rules and representation are familiar. The key addition is PopOut and its impact on game tree search.

## Rules

During a player's turn, they may make one of two types of moves:
* Drop one of their pieces into any non-full column, exactly as in regular Connect 4.
* If the bottom piece of a column belongs to the player, they may remove or "pop" it, causing the pieces above to fall one space.

A player wins by forming a line of four consecutive pieces horizontally, vertically, or diagonally. If a pop yields a simultaneous four-in-a-row, the popper wins. The board must correctly handle wins created or destroyed after either move. Infinite play is a draw.

The recommended move notation is a number for drops and a letter for pops. ``1`` drops a piece into the leftmost column, while ``A`` pops a piece in that column. ``0``/``J`` denotes the tenth column.

## Engines

The solver equips three search engines: **Principal Variation Search**, **Monte Carlo Tree Search**, and a **goal-directed incremental fixed-point graph resolver**.

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
* 8-bit depth
* 5-bit move (max 16 drops/16 pops)
* 2-bit bound (lower/exact/upper)

Pay particular attention to collisions, especially when searching in Oracle mode or verifying Expert win distances. Distinct position keys may yield the same partial lock. Neglecting this scenario would contaminate the transposition table with entries that appear valid but are incorrect for the given search context. Nonetheless, this tradeoff is tolerable in Expert mode.

Since no known positional theory for PopOut exists, design a fast, sensible heuristic evaluator rather than assuming standard Connect 4 heuristics will suffice. Account for immediate and potential four-in-a-row threats, playable winning cells, multiple or intersecting threats, center control, and mobility. Pieces are not permanently anchored: a pop can shift an entire column and affect several alignments at once. Bottom pieces can enable or restrict future pops, and apparently strong structures can be unstable if their supporting pieces are removable. Reward robust threats and useful drop/pop options while penalizing opponent threats and tactically vulnerable structures. Maintain a symmetric evaluator between players that is inexpensive enough to invoke frequently at search leaves. Ensure its weight vectors are tunable via the self-play lab by pitting challengers against the champion and retaining the stronger set. View the score as an estimate, not proof of a win, draw, or loss.

### Monte Carlo Tree Search

Monte Carlo Tree Search (MCTS) is a search algorithm for decision-making in vast, potentially unbounded state spaces. Instead of considering all possibilities, it intelligently selects a subset, expands some states, and runs a stochastic simulation on that subset. The reward is then derived from simulation results and propagated backward toward the root. Unlike minimax, MCTS eliminates the need for an evaluator; it learns which branches are most promising through simulations and a selection policy, typically UCT.

Brainstorm an alternative to purely uniform random rollouts. For example, MAST (Move-Average Sampling Technique) is a heuristic that biases toward successful moves from past simulations. NST (N-gram Selection Technique) builds on MAST by incorporating local sequences. Nevertheless, randomness is integral to preventing overfitting, so employ a method like epsilon-greedy.

Program an MCTS solver that carries win/loss information within each node, excluding draws (see the next section for an explanation). This will prompt it to stop once the root node is decided. When an expanded node is terminal, disregard exploration and prune its branches. Pruning decreases memory usage and concentrates computing resources on unresolved subtrees. Avoid relying on playouts to determine termination. Rely on a principled minimax approach during backpropagation. Let ``s`` be a state, ``a`` be an action, and ``S(s, a)`` be the successor of ``s`` after taking ``a``:

1. ``WIN(s) <=> ∃a LOSS(S(s, a))``

    "``s`` wins if and only if there exists an ``a`` such that ``S(s, a)`` loses."

2. ``LOSS(s) <=> ∀a WIN(S(s, a))``

    "``s`` loses if and only if ``S(s, a)`` wins for all ``a``."

3. Otherwise, ``s`` is inconclusive.

An MCTS node should fit within sixteen bytes, accounting for overflow or rollover during prolonged analyses:

* 32-bit reward
* 32-bit visit count
* 32-bit parent or first child offset
* 5-bit child count
* 5-bit move
* 2-bit proof (none/loss/win)

Keep in mind that storing the raw win rate as a 32-bit floating-point number will cause updates to stagnate before visits saturate. Most importantly, do not stop when the tree becomes full. Instead, perform a generational reset and garbage collection to restart with informed statistics from previous searches. Refrain from clearing everything.

During the rollout phase, take advantage of any winning or defensive situations that arise. Do not expand obviously losing nodes (i.e., the opponent wins on their turn). If the rollouts reach a solved position at any point, discontinue them and assign the solution reward. When selecting nodes, bypass any solved ones. These measures should increase playing strength and search efficiency.

### Goal-Directed Incremental Fixed-Point Graph Resolver

The goal-directed, incremental fixed-point graph solver is an exact, proof-oriented alternative to NegaScout and MCTS. It aims to prove the root position's minimax outcome while exploring the smallest feasible portion of the reachable state graph. Unlike conventional exhaustive retrograde analysis, it constructs a partial directed graph on demand, consolidates transpositions and tracks predecessor relationships, unresolved dependencies, and proven outcomes. These outcomes propagate backward, potentially resolving more and more positions without further exploration.

Cycles require explicit fixed-point reasoning rather than treating repeated positions or strongly connected components as automatic draws. A player can demonstrate a non-losing strategy through a finite, closed strategy subgraph in which one safe continuation exists at that player's decision points, while factoring in every legal opponent response.

Graph expansion is driven by unresolved proof obligations, not by brute-force enumeration. Heuristics, including information from NegaScout or MCTS, may prioritize promising successors but cannot prove outcomes. The solver separates provisional evaluations from mathematically proven results, reuses certified subgraphs across transpositions, and supports incremental propagation as new information emerges. The algorithm must be sound and complete over the finite PopOut state graph, including arbitrarily long cycles, while allowing early termination as soon as the root's outcome becomes proven.

Exact ``WIN``/``DRAW``/``LOSS`` classification takes precedence over finding optimal distances, which can be calculated separately once outcomes are resolved. Diagnostic statistics can include the number of expanded unique states, edges examined, unresolved dependencies, fixed-point iterations, proven outcomes, peak memory usage, and wall-clock time. Ensure correctness by differential testing against exhaustive reference solutions on reduced boards, covering forced wins and losses, cyclic draws, and positions with multiple transpositions.

The graph stores its data in compact, adjacent arrays, using integer indices in place of object references or individually allocated nodes. Memory consumption scales with the number of discovered positions and recorded transitions.

* 32-byte state node
    * board key
    * 4-byte first outgoing edge index
    * 4-byte first incoming edge index
    * 1-byte outgoing edge count
    * 1-byte unresolved successor count
    * 1-byte outcome
    * other fields
* 16-byte directed edge
    * 4-byte parent node index
    * 4-byte child node index
    * 4-byte next incoming edge index
    * 1-byte move identifier
    * other fields
* 8-byte transposition index
    * 4-byte hash
    * 4-byte node index

Grow node and edge arrays geometrically within contiguous storage, organized into large, reusable chunks. Avoid per-object memory allocations, duplicate keys, and 64-bit pointers. Utilize 32-bit indices with reserved invalid sentinels, upgrading to 64-bit only when capacity demands it. Save work queues, proof obligations, and temporary traversal data independently. Reuse scratch buffers and do not retain easily recomputable information.

### Coding Language

Please develop the engine in an appropriate language and compile it to WebAssembly. **Install any dependencies** if needed. Leverage Memory64 to access a large, dedicated linear memory containing the transposition table, MCTS node pool, and graph pool. NegaScout, MCTS, and the fixed-point resolver are mutually exclusive and thus share the same region. Store everything else (e.g., board, moves, indices, scores, search, etc.) in separate WASM32 memory (i.e., multi-memory). The WebAssembly engine manages the game state and drives the search, while the application serves as a thin presenter around it. Allow the user to change the arena size, the exploration constant, and the epsilon parameter.

The JavaScript API limits WASM64 modules to a maximum of 16 GiB of addressable memory, browser permitting. Allocate the entire amount when the user resizes the arena. For instance, if the user wants the whole 16 GiB (2^31 TT entries; 2^30 MCTS nodes), the browser should convey that figure verbatim. Ideally, there should be no clamping. If clamping happens, reassess the engine's memory layout to accommodate the requested space.

Do **NOT** ignore this request without a compelling reason. "Too complex," "too complicated," and "too challenging" are not valid excuses. Check if any Vite configuration files are editable where permissible. If not, wire a build script and import it into a file called ``postcss.config.cjs`` or ``postcss.config.js``. Create the file if it does not exist. While its primary purpose is to load PostCSS plugins, it can also act as a build hook for the engine. Do not insert any plugins; they are unnecessary. Skip this step if you have a shell to run commands. Whenever practical, update the toolchain to one that has first-class Memory64 and multi-memory support.

If compilation is truly unattainable in the build environment, explain why WebAssembly will not build with the given toolchain, and make sure the engine code builds with WASM64 when the user clones the project locally. This conversion should be seamless and automatic. Most modern, mainstream browsers already support multi-memory and Memory64. Swap them if this one does not recognize these features. Investigate other avenues before falling back to TypeScript as an absolute last resort.

In any case, you have two options: **AssemblyScript** and **WAT**.

#### AssemblyScript

AssemblyScript is a TypeScript dialect tailored for low-level WebAssembly programming. It lets TypeScript programmers write AssemblyScript code without learning a new language. The compiler then translates it into WebAssembly bytecode. If any stage of the compilation pipeline fails, the code reverts to standard TypeScript.

The AssemblyScript compiler partially supports Memory64. Although AssemblyScript's official status for Memory64 is listed as uncertain, the compiler accepts the flag ``--enable memory64``. To be certain, probe its ability to read and write beyond 4 GiB of memory before committing. Unfortunately, multi-memory support is lacking, but since the three search engines are mutually exclusive, this should not be a major concern. Lastly, compile the engine with speed optimizations using ``-Ospeed``.

#### WAT

WAT refers to the WebAssembly text format. It is a human-readable, debug-friendly rendition of WebAssembly bytecode. Unlike AssemblyScript, it fully supports Memory64 and multi-memory. This grants NegaScout, MCTS, and the fixed-point resolver access to all memory independent of allocations from other parts of the engine.

Nonetheless, debugging will be more involved if something goes wrong. Therefore, thoroughly review the code for logical inconsistencies and test incrementally before deeming it ready. Furthermore, optimizations are not free without compilers. Even if the hand-assembled code is clean and meets the specification, speed discrepancies may occur. As such, inspect the code for any performance enhancements.

Ultimately, pick your poison. A mixture of AssemblyScript and WAT is acceptable. The former can harness the engine's WASM32 portions, and the latter can do the same with the WASM64 arena. The AssemblyScript compiler can also print in WAT; thus, it may be worthwhile to compare your WAT with the compiler's output. Moreover, achieving full Memory64 support in AssemblyScript likely entails patching the compiler's emitted WAT to use 64-bit offsets.

## Important Considerations for NegaScout and MCTS

Connect 4's state space forms a finite tree because the column stack height increases monotonically, pieces never move, and the board size is fixed, preventing recurrence to a previous state. PopOut does **not** have these properties. A play sequence can last indefinitely. Pops remove pieces, and subsequent drops and pops can revisit earlier states. Therefore, its state space becomes a **directed cyclic graph**.

A repetition only proves the current search path has entered a cycle; it does *not* establish that perfect play stays in the cycle. A cycle can have an exit that one player can force. If that exit eventually leads to a forced win or loss, declaring the first repetition a draw can obscure the true game-theoretic result. Because PopOut is cyclic, a solver must distinguish between proven and provisional values at the current search horizon:

* Do not assume that search depth corresponds to remaining moves.
* Do not assume that every line someday reaches a terminal position.
* Do not automatically classify a repetition or a detected cycle as a draw.
* Do not use an arbitrary move limit as part of the game state or as a draw proof.
* Transpositions can connect states at different depths, so the table must be designed with cycles in mind.

NegaScout can return a neutral score on a back edge for move ordering, but trees cannot tell when they are going in circles; only graphs can. Without a reasonable stopping point, it would continue endlessly. Capping it at depth 255 is an acceptable compromise. Attach a ``CYCLE`` label indicating that the best approach thus far is to repeat moves, and detach it when a better move is found. MCTS should avoid back edges unless the position forces it, and apply the same capping to its playouts and tree expansion. Make sure that it can unwind the cycle instead of stalling.

## User Interface

Opt for a minimal dark theme to reduce eye fatigue from bright screens while preserving vivid, distinct piece colors. Player 1's pieces should be yellow, and Player 2's should be red. The shades do not have to be exact, but they should be consistent. Dropping and popping animations and sound effects are optional.

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

Once NegaScout finds a solution, display a green ``Wn`` (win) or red ``Ln`` (loss) when the outcome is guaranteed in ``n`` moves, and stop. If the player can win or lose immediately, show ``WIN``/``LOSS``, not ``W0``/``L0``. In all other cases, present an evaluation score. Report ``WIN``/``LOSS`` if MCTS arrives at a solution for the root. For the fixed-point resolver, output a yellow ``DRAW`` accordingly, or ``UNKNOWN`` if out of memory.

The user may choose whether the AI opponent goes first or second, adjust its skill level, and freely undo, redo, edit moves, and restart the game as they see fit. Note the human-human and AI-AI modes. Set the players to two humans. The board size is configurable from 4x4 to 10x10, with 7x6 as the default.

Add keyboard shortcuts to the interface. Pressing a number drops a piece, and pressing a letter pops it from the corresponding column (case-insensitive). The left and right arrow keys advance the game backward and forward one move, respectively. Similarly, the Home and End keys navigate to the earliest and latest positions. Adapt the interface for desktop and mobile clients.

The desktop interface should contain three panels in this order:

* Left - move history, input field
* Center - board, game controls
* Right - analysis, engine settings

As for mobile:

* Top - board, controls
* Middle - moves, inputs
* Bottom - analysis, settings

## Test Positions

A few solvable positions are open to debugging the search engine, static evaluator, and game logic. A properly optimized engine with fine-tuned weights, advanced search, and domain knowledge could solve them in _seconds_, not minutes or hours. The fewer nodes and wall time, the better. Consider including additional weights or pruning strategies based on observations. Experiment with them to see whether they increase solving efficiency across several game states. Never expose positions to the interface, as they are intended for testing and profiling. For the standard seven-wide, six-high board:

| Move Sequence | Game-Theoretic Result | Notes |
| - | - | - |
| ∅ | W20 |
| 1 / 7 | W26 |
| 2 / 6 | L29 |
| 3 / 5 | L23 |
| 33445 | L1 | Double-threat detection in 1 node.
| 44444 | L21 |
| 4444433332 | L33 |
| 44442333232 | W26 |
| 44442333236 | DRAW | Fixed-point resolver only. Tree search algorithms such as NegaScout and MCTS cannot prove draws in a game graph.
| 44234233422111 | W16 |
| 442342334221111 | L19 | This line contains a cycle as a delaying tactic and ultimately returns to the position from the previous row as if ``1`` had not been made, serving as a reminder that cycles do not necessarily end in a draw.

In rare circumstances, a player can defeat themselves when the position is full but allows pops, which all benefit the opponent. On a 4x4 board, ``2`` and its horizontal reflection ``3`` are ``W21``. If NegaScout displays it as ``W22``, it is *not* distance-optimal.

## Tips
* Minimize chess terminology throughout the codebase. The design document does not reference "mate" anywhere because PopOut is not a chess variant.
* Represent the position as integers and perform bitwise and bit-parallel operations on them. This will significantly boost throughput over mailboxes.
* If the opponent threatens an immediate win and there is a drop that can neutralize it, filter out other drops that ignore the threat. This does not apply to pops.
* Pops solely decrease the column stack height; it is possible to bypass the vertical alignment checks. Similarly, they cannot block a vertical three-in-a-row threat and can be omitted from the move generator.
* The pop move operation can be achieved in constant time without looping over the rows.
* Verify that the move generator is working correctly by comparing the 7x6 perft numbers (terminals are leaves) against the baseline:
    | Depth | Nodes |
    | - | - |
    | 1 | 7 |
    | 2 | 49 |
    | 3 | 392 |
    | 4 | 3087 |
    | 5 | 26320 |
    | 6 | 220626 |
    | 7 | 1965614 |
    | 8 | 17051588 |
    | 9 | 155032738 |
    | 10 | 1378595958 |
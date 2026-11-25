# Video Poker Application

**Develop a functional video poker web application that replicates the mechanics of a real video poker machine.**

Prioritize correctness, computational efficiency, compact data structures, and responsive gameplay over visual presentation. The programming language and framework are flexible, provided that the application runs in a modern browser.

## Card Representation

A card is a 6-bit integer.

* Bits 0-1: Suit (0-3)
* Bits 2-5: Rank (0-13)

Ordinary ranks are 0-12, representing Two through Ace. Rank 13 is the joker. Jokers are interchangeable wild cards that share one identifier. Suits can be in any internal order.

## Deck Representation

A deck is a fixed array of up to 57 cards.

* 52 distinct cards
* Up to five jokers

Cards are drawn without replacement. Before each new hand, shuffle the deck using an unbiased algorithm.

## Hand Representation

A hand is a 30-bit integer packing five cards. Include operations to extract, replace, pack, unpack, validate, and canonicalize hands. Cards must not be duplicated, but multiple jokers are permitted.

Canonicalization sorts card identifiers in ascending order, ensuring equivalent unordered hands have identical representations.

## Hand Evaluation

Implement the evaluator using a bitmask of qualifying poker hand properties.

* ``ONE_PAIR``
* ``TWO_PAIR``
* ``THREE_KIND``
* ``FOUR_KIND``
* ``FIVE_KIND``
* ``STRAIGHT``
* ``FLUSH``
* ``BROADWAY``
* ``WILD_RANK``

``BROADWAY`` indicates an Ace-high straight, not the presence of high cards. Derive composite classifications from the underlying properties rather than redundant flags:

* Full house => ``ONE_PAIR | THREE_KIND``
* Straight flush => ``STRAIGHT | FLUSH``
* Royal flush => ``BROADWAY | FLUSH``

Wildcard support must be configurable per game variant. A card may be wild because it is a joker or because the selected game rules designate its rank as wild.

Separate evaluation into three stages:

1. Analyze the original cards, identify natural rank and suit properties, count wilds, and preserve information needed for promotion.
2. Determine every hand property achievable through legal wildcard substitutions without modifying the original cards.
3. Select the highest-paying eligible outcome based on the promoted properties, original hand, wager, and game rules.

Do not assume that independently attainable rank-group properties necessarily coexist in a single wildcard assignment. ``ONE_PAIR | THREE_KIND`` alone does not constitute a full house in a wildcard game. Instead, check eligibility from the rank-frequency distribution and available wilds.

The evaluator should only be responsible for classifying hands. Maintain the distinction between natural and wild combinations, including the number of wild cards. Do not rely on payout amounts for classification or promotion. Leverage the mutual exclusivity of hand types: straights, flushes, and high cards cannot simultaneously occur with N-of-a-kinds or full houses, saving computational resources.

## Games and Paytables

Support adjustable game rules, such as:

* Number of jokers
* Hand categories and qualifying conditions
* Payout multipliers
* Minimum and maximum bets
* Special payouts for maximum bets

Create at least one playable video poker variant without jokers. Additional variants may introduce wild cards and their associated payouts. These three games cover all code paths.

* Jacks or Better
* Joker Poker (Kings or Better)
* Deuces Wild

Ensure that payouts follow the active paytable, since not all variants have identical rankings or rewards. The paytable uses hand evaluator information to assign paylines and issue payouts.

## Machine Mechanics

Complete the deal-hold-draw-payout cycle.

1. The player places a wager.
2. The wager is deducted from the credit balance.
3. The machine shuffles a fresh deck and deals five cards.
4. The player selects cards to hold.
5. The player presses "Draw."
6. A new card replaces each unheld card from the remaining deck.
7. The machine evaluates the final hand and awards credits according to the paytable.
9. The result appears, and the machine is ready for the next hand.

Held cards must stay visible and unchanged. Each hand authorizes exactly one draw after the initial deal. The machine may announce a winning hand on the deal.

Wagers cannot exceed the available balance, and actions must be restricted to those valid for the current state. A winning payout is the total credits returned, not the net profit. All credits are virtual and hold no monetary value. Therefore, players cannot purchase, transfer, exchange, or redeem them.

## User Interface

Design a dark, clean, practical interface with:

* Five playing cards
* Individual hold buttons
* Deal and draw buttons
* Bet adjustment buttons
* Current wager and balance
* Active paytable
* Final hand classification
* Credits scored

Cards should clearly show their rank and suit, with a large pip near the bottom center and omitting inverted indices. Kings and queens should display their respective crowns, while jacks should depict courtier-style headwear. Aces should feature a coat of arms incorporating crossed swords and a shield. Jokers should be distinguishable from other cards, typically as a jester. Mark wild cards with a "WILD" label, but do not mark jokers as such, since they are already wild.

Holding cards should feel intuitive, with clear visual feedback. Avoid unnecessary animations, elaborate graphics, sound effects, and decorative casino elements. Allow keyboard, mouse, and touchscreen input.

Arrange the panels to resemble an actual machine. From top to bottom:

* Paytable
* Announcement
* Hold indicator
* Five-card hand
* Bet and credits
* Control buttons

## Session Statistics

Track:

* Total hands played
* Total credits wagered
* Total credits returned
* Net credit gain or loss
* Number of winning hands
* Frequency of final hand category
* Return percentage

Offer a setting to reset them.

## Correctness and Reliability

* Prevent duplicate cards within a round
* Never draw cards that have already been dealt
* Reject invalid state transitions
* Handle insufficient credits gracefully
* Ensure payouts match the active paytable
* Test ordinary, wild, and edge-case hands
* Use integer or other exact arithmetic for credit accounting
* Keep game logic separate from interface rendering

The application must be usable from the start. It cannot simply demonstrate individual algorithms or showcase a nonfunctional mockup.

### Probability Verification

The hand classification system must agree with the mathematical probabilities of poker hands. For a standard 52-card deck, enumerate all 2598960 five-card combinations and verify their quantities against the established frequencies. The sum of all categories must equal 2598960.

| Category | Count |
| - | - |
| Royal Flush | 4 |
| Straight Flush | 36 |
| Four of a Kind | 624 |
| Full House | 3744 |
| Flush | 5108 |
| Straight | 10200 |
| Three of a Kind | 54912 |
| Two Pair | 123552 |
| One Pair | 1098240 |
| High Card | 1302540 |

### Suit Canonicalization

Hand canonicalization should exploit the mathematical equivalence of poker hands under permutations of the four suits. Among the 2598960 possible five-card combinations, only 134459 are unique. Two hands are equivalent if one can be transformed into the other solely through a global permutation of suit identifiers. The canonicalization process must:

1. Consider all 24 suit permutations.
2. Apply each permutation to all five cards.
3. Leave the card ranks and jokers untouched.
4. Sort the resulting card identifiers.
5. Encode the sorted identifiers into the 30-bit hand representation.
6. Choose the smallest numeric packed representation as the key.

An equivalent, more efficient algorithm is preferable if it produces the same keys for suit-equivalent hands and retains the exact number of equivalence classes.

Extend canonicalization to hands with wildcards while retaining their special identities and wildcard semantics. When a deck has multiple jokers, account for the physical multiplicity of joker combinations.

## Optimal Hold Analyzer

Write a mathematically precise hand analyzer that generates the optimal hold strategy for any initial five-card hand. It must calculate all 32 possible hold masks and recommend the one that maximizes the expected return.

If the initial deck contains $ N $ cards and $ h $ cards are held, the number of possible draws is:

$$ \binom{N - 5}{5 - h} $$

Represent the player's hold selection as a 5-bit integer. Each set bit denotes a held card at the corresponding position in the hand. Valid masks range from 0 (discarding all) to 31 (holding all).

For each hold mask:

1. Determine the held and discarded cards.
2. Construct the remaining deck by excluding the five dealt cards.
3. List every unordered combination of replacement cards.
4. Build and evaluate each resulting five-card hand.
5. Compute its payout using the active paytable and current wager.
6. Accumulate the payout across all draws.
7. Divide the cumulative payout by the number of draw combinations.

After calculating them, select the one with the highest payout. Note that different paytables or wager amounts may produce different optimal hold recommendations for the same cards. Conversely, multiple masks with equal payouts are mathematically equivalent.

Utilize optimizations, such as precomputing combinations and caching evaluations, to speed up analysis. Minimize excessive allocations within the loop.

The analysis report should reveal:

* Held cards
* Draw combination
* Expected return
* Probability of winning hands
* Total losing hands
* Hands per payline

An illustrative example:

| Hold | Return | Win % | Draws | Losses | Paylines
| - | - | - | - | - | - |
| [A] [B] [C] [D] [E] | 0 | 0 | 1 | 0 | [Show/Hide]
| __ [B] [C] [D] [E] | 1 | 1 | 47 | 1 |
| __ __ [C] [D] [E] | 2 | 2 | 1081 | 2 |
| __ __ __ [D] [E] | 3 | 3 | 16215 | 3 |
| __ __ __ __ [E] | 4 | 4 | 178365 | 4 |
| __ __ __ __ __| 5 | 5 | 1533939 | 5 |

Expose them through an interface that lets players inspect and compare alternative holding strategies. Consider sorting the list so that the highest expected value is always first.

Do not use Monte Carlo sampling, heuristic rules, or approximate likelihoods to calculate the optimal mask. However, reuse the canonicalization mechanism to reduce redundant evaluations and cache classification results by hand keys. When retrieving a cached strategy, map the canonical hand's hold-mask positions to the original card positions. Cache entries must differentiate between game configurations that can alter returns, including:

* Deck composition
* Wildcard rules
* Paytable
* Wager and maximum-bet bonuses

Do not use unrestricted suit-orbit multiplicities to weigh conditional draw outcomes. Since the five dealt cards are removed from the deck, the relevant combinatorial symmetries will differ.

The analyzer must work with games that use jokers and wild cards. Apply promotion rules consistently before resolving the paytable. When distinct jokers share the same identifier, count their draw multiplicities. Do not conflate equal encoded hands with equally probable physical outcomes.

For a standard 52-card deck, confirm that:

* All hold masks are evaluated
* A mask holding $ h $ cards has $ C(47, 5 - h) $ possible outcomes
* The total number of outcomes is 2598960
* Every enumerated final hand has five valid cards
* No card, held or discarded, is improperly returned to the draw deck
* Conditional outcome probabilities sum to one for each hold mask
* Expected returns align with independently calculated references
* Suit-equivalent hands yield equivalent strategies after positional remapping
* Cached and uncached analyses yield identical results

## Return to Player Calculator

Program a theoretical return-to-player (RTP) calculator for the video poker machine. RTP measures the expected percentage of credits returned relative to credits wagered, assuming optimal hold decisions for every starting hand.

For each starting hand:

1. Evaluate all hold masks.
2. Calculate the payout of each mask.
3. Pick the maximum payout.
4. Collect that payout into the RTP calculation.

The calculation must not use a predetermined strategy chart, random decisions, or Monte Carlo estimates. Rather, reuse the suit canonicalization system to avoid repeatedly solving equivalent starting hands. For each canonical hand, multiply its payout by its multiplicity. The RTP must match whether calculated using exhaustive physical hands or weighted canonical hands. Suit-based reduction is valid only when the active game rules and payouts are invariant under global suit permutations.

Provide an optimized method of calculating payouts **without traversing millions of replacement hands**. One suggestion uses precomputed subset payout sums and the inclusion-exclusion principle. Define:

$$ G(S) = \sum_{\substack {F \supseteq S \\ |F| = 5}} P(F) $$

Here, $ G(S) $ is the sum of payout $ P $ over every complete five-card hand $ F $ containing the card subset $ S $.

For an initial hand $ H = A \cup B $, where $ A $ contains held cards and $ B $ contains discarded cards, the total payout $ T $ across every draw is:

$$ T(H, A) = \sum_{S \subseteq B} (-1)^{|S|} G(A \cup S) $$

The subsequent expected value is:

$$ EV(H, A) = \frac{T(H, A)}{\binom{N - 5}{5 - |A|}} $$

This method excludes every dealt card from the replacement deck while keeping the held cards. It may use a different algorithm if it preserves mathematical exactness.

Once the RTP has been calculated, display:

* Initial deal combinations
* Normalized hand frequencies
* Computation time

For full-pay, max-bet 9/6 Jacks or Better under optimal play, the RTP calculator must output this reduced fraction:

$$ \frac{1653526326983}{1661102543100} \approx 0.995439044 $$

Use integer arithmetic for the numerator and denominator, then reduce the final fraction by the greatest common divisor. Compare the output with the reference fraction, but do not hardcode that fraction as *the* output. Perform all intermediate expected-value calculations with rational arithmetic.

This fraction, rather than its rounded decimal representation, is the ultimate correctness benchmark.
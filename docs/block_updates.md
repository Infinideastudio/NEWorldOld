# A proposal for the overall design for block update mechanisms

@bridgekat

## Block updates as rewriting rules

The world, the blocks it contains and the block update functions together form an [abstract rewriting system](https://en.wikipedia.org/wiki/Abstract_rewriting_system). For such systems there are two desirable properties:

* **Confluence:** if some world state $A$ might eventually turn into states $B$ or $C$ via zero or more block updates, there exists a world state $D$, such that both $B$ and $C$ might eventually turn into $D$ via zero or more block updates.
* **Locality:** the block update rules can be presented in the form of $s \rightarrow t$ where $s$ and $t$ are collections of blocks each associated with a relative coordinate, such that any occurrence of $s$ in the world can be replaced by $t$ in a block update.

As an example, the vanilla Minecraft lighting system satisfies both properties:

* **Confluence:** by induction, it suffices to show that lighting updates are "confluent in a single step". Two non-adjacent lighting updates are clearly confluent, so we only need to consider adjacent lighting updates. However, given the light levels of all blocks surrounding the two, it is always possible to "relax" the light levels of the two blocks to a unique state determined solely by light levels of the surrounding blocks, and whether the two blocks are light sources themselves.
* **Locality:** this is clear by the definition of lighting updates. Note that sunlight can be implemented as *maximum sky light levels propagate down without attenuation*, and the above proof of confluence still holds.

On the other hand, the vanilla Minecraft redstone system does not satisfy **confluence**, a simple example is when two opposite pistons, with only one air block in between, are activated simultaneously by a single redstone signal. There are two stable end-states, depending on the exact order of block updates.

## Why are the properties desired?

A direct consequence of confluence is, if some stable end-state (also known as a **normal form**) exists, then it must be unique. In particular, the stable end-state *will not depend on the ordering of block updates*, so we can expect our machines to produce the same result *as long as every block has enough chance to update (i.e. loaded and updated until there can be no further changes)*. This makes it possible to rigorously reason about their behaviour, and allows for optimisations on the game engine. **We can safely execute block updates in multiple threads without worrying that some machines can get their behaviour changed by out-of-order block updates.**

Locality might be relatively less important, but they make block updates straightforward to implement. If every rule $s \rightarrow t$ has a small affected radius (i.e. size of block groups $s, t$), it can be quick to check for a possible subsequent update (also known as a **redex**) after some block is changed. **In this way, we can partly guarantee that block updates which *should be* carried out *will be* and with minor delays**.

## Is satisfying these properties a severe limitation to what might be created?

The canonical computation model for these two properties is [asynchronous cellular automata](https://en.wikipedia.org/wiki/Asynchronous_cellular_automaton). It is known that asynchronous cellular automata can [simulate](https://arxiv.org/pdf/2502.05989) their synchronous counterparts, which are known to be Turing complete.

## Example set of rules: the LCM2 circuits

A possible design models synchronous digital circuits using per-block *local clock* and *data* states. Conceptually, *local clock* is a natural number that advances monotonically by block updates, and *data* is a boolean value representing the output signal of the block. We then have different update rules for logic gates and registers:

* **Gates** (NAND, wires and forks): for a block at clock `t`, update rules are applicable iff all its inputs are at clock `t + 1` and all its outputs are either a gate at clock `t` or a register at clock `t + 1`. In this case, the block updates itself so that it carries data `f(inputs)` and has clock `t + 1`, where `f` is the boolean function corresponding to the logic gate (or identity if the block is a wire or fork).
* **Registers** (flip-flops): for a block at clock `t`, update rules are applicable iff its input is at clock `t` and its output is either a gate at clock `t` or a register at clock `t + 1`. In this case, the block updates itself so that it carries data `input` and has clock `t + 1`.

If in a circuit:

* For every "gate → gate" or "register → gate" connection, the clock of the producer block is 0 or 1 greater than the clock of the consumer block;
* For every "gate → register" or "register → register" connection, the clock of consumer block is 0 or 1 greater than the clock of the producer block;

Then the rules maintain these invariants. Therefore, assuming these conditions holds initially, we can only store `clock % 2` at every block. This is why I call this design the "LCM2 (local clock mod 2) circuits".

Apart from the orientation information (which faces are input/ouput interfaces), each block only has 2 × 2 = 4 possible states. The rewrite rules are clearly local. Also, note that for every adjacent connected pair, at most one of the two blocks can update next (by cases: \* → gate, \* → register), and each update flips this ordering - so the system is a special case of [flip automata networks](https://arxiv.org/pdf/2502.05989) which is known to be confluent. Finally, note that the system simulates synchronous updates by having each node "progress at its own pace": *data* always represent the block state at *clock*, which is invariant under any particular update schedule.

To drive LCM2 circuits, we need the inputs to clock on its own. This is simple: just connect a register back to itself so it keeps emitting clock signals with a constant data.

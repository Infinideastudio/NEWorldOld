# A proposal for the overall design for block update mechanisms

@bridgekat

## Block updates as rewriting rules

The world, the blocks it contains and the block update functions together form an [abstract rewriting system](https://en.wikipedia.org/wiki/Abstract_rewriting_system). For such systems there are two desirable properties:

* **Confluence (Church-Rosser):** if some world state $A$ might eventually turn into states $B$ or $C$ via zero or more block updates, there exists a world state $D$, such that both $B$ and $C$ might eventually turn into $D$ via zero or more block updates.
* **Locality (semi-Thue):** the block update rules can be presented in the form of $s \rightarrow t$ where $s$ and $t$ are collections of blocks each associated with a relative coordinate, such that any occurrence of $s$ in the world can be replaced by $t$ in a block update.

As an example, the vanilla Minecraft lighting system satisfies both properties:

* **Confluence:** by induction, it suffices to show that lighting updates are "confluent in a single step". Two non-adjacent lighting updates are clearly confluent, so we only need to consider adjacent lighting updates. However, given the light levels of all blocks surrounding the two, it is always possible to "relax" the light levels of the two blocks to a unique state determined solely by light levels of the surrounding blocks, and whether the two blocks are light sources themselves.
* **Locality:** this is clear by the definition of lighting updates. Note that sunlight can be implemented as *maximum sky light levels propagate down without attenuation*, and the above proof of confluence still holds.

On the other hand, the vanilla Minecraft redstone system does not satisfy **confluence**, a simple example is when two opposite pistons, with only one air block in between, are activated simultaneously by a single redstone signal. There are two stable end-states, depending on the exact order of block updates.

## Why are the properties desired?

A direct consequence of confluence is, if some stable end-state (also known as a **normal form**) exists, then it must be unique. In particular, the stable end-state *will not depend on the ordering of block updates*, so we can expect our machines to produce the same result *as long as every block has enough chance to update (i.e. loaded and updated until there can be no further changes)*. This makes it possible to rigorously reason about their behaviour, and allows for optimisations on the game engine. **We can safely execute block updates in multiple threads without worrying that some machines can get their behaviour changed by out-of-order block updates.**

Locality might be relatively less important, but they make block updates straightforward to implement. If every rule $s \rightarrow t$ has a small affected radius (i.e. size of block groups $s, t$), it can be quick to check for a possible subsequent update (also known as a **redex**) after some block is changed. **In this way, we can partly guarantee that block updates which *should be* carried out *will be* and with minor delays**.

## Is satisfying these properties a severe limitation to what might be created?

I am not too sure for now. At least [either](https://en.wikipedia.org/wiki/Lambda_calculus) [one](https://en.wikipedia.org/wiki/Semi-Thue_system) of them does not exclude the possibility of Turing completeness, and it seems possible that a $\lambda$-calculus defined using some notion of [explicit substitutions](https://en.wikipedia.org/wiki/Explicit_substitution) might satisfy both.

As a note, structure generation (trees, villages etc.) can be implemented as expansion of special **structure blocks**, which are confluent and local block updates if we enforce a total order on blocks (e.g. ordered by hardness and then ID) such that in overlapping areas the blocks with higher precedence overwrite the lower. The world generator only needs to take care of the generation of initial structure blocks.

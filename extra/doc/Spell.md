# Spells                                 {#spells} #

Spells consist of a whole lot of information, the most usual way to access them is via their ID. A good
tool to use when examining spells and looking into what they will do is [QSW](https://github.com/sidsukana/QSpellWork) which will show you alot of the information available about a spell.

A spell is made up of a lot of parts, on of these are the effects that a spell has, without them spells
wouldn't do much at all since the effects apply different kinds of `Aura`s that do different things
to the target or something else, in the most common scenario that would be to deal damage. If we take
a look at the Frostbolt spell (id 116) in QSW for 1.12.x and scroll down to the Effects heading we will
see that there are 2 effects. The maximum amount of effects on spell can have is 3, these three indexes
are referenced via the `EFFECT_INDEX_0`, `EFFECT_INDEX_1` and `EFFECT_INDEX_2` macros.

Lets take a closer look at Effect 0, it has an id of 6 which QSW tells us is named `SPELL_EFFECT_APPLY_AURA` which tells us that we will apply an `Aura` with this effect, if we look a few lines down we see that
the Aura Id is 33 which corresponds to `SPELL_AURA_MOD_DECREASE_SPEED` which is the slowing effect of
our Frostbolt spell. If we look to the right on the same line we see that `value=-40` which would mean
that we should decrease the speed by 40%. Knowing what this value means depends on what Aura Id we are
applying, all of them have different meanings for the `value` and `misc` values. The `periodic` variable
tells us whether or not this effect is periodic as in coming back over and over again, this is usual
for some healing spells or DOTs.

The part in the server that handles this is mostly the `Aura` class and the `AuraModifier` struct which
is the part we talked about most recently, the one which keeps track of the Aura Id, value, misc and if it's periodic or not. 
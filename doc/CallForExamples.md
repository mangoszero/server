# Example of how to use the Unit::CallFor/CheckFor            {#CallForExample} #

In general, when using the CallFor* functions you need to supply a functor with a `operator()` which
takes (usually) a `Unit` as it's argument and does something with it. A typical functor for that might
look something like this:
~~~{.cpp}
struct MyFunctor
{
    explicit MyFunctor(int _a) : a(a) {}
    void operator()(Unit* unit) const { unit->RemoveAura(a, EFFECT_INDEX_1); };
    int a;
};
~~~
When the CallFor* is called it will go through all the `Unit`s (or something else) that the function
name says that it will and call the `operator()` with the `Unit` it found as argument. Much like a loop
looking like this:
~~~{.cpp}
void CallForEveryone(Functor f)
{
    for(int i = 0; i < AllUnits.Size(); ++i)
    {
        f(GetUnit(i));
    }
}
~~~
Bear in mind that this code is not something that represents how the API works, just an example for
understanding how it all works.

## Using the Unit::CallForAllControlledUnits                  {#CallForAllControlledUnitsExample} ##

A functor for `Unit::CallForAllControlledUnits` might look this. This specific one is used
to change the speed at which units move, if the "main" `Unit` changes moving speed so should all
his pets, minions etc:
~~~{.cpp}
struct SetSpeedRateHelper
{
    explicit SetSpeedRateHelper(UnitMoveType _mtype, bool _forced) : mtype(_mtype), forced(_forced) {}
    void operator()(Unit* unit) const { unit->UpdateSpeed(mtype, forced); }
    UnitMoveType mtype;
    bool forced;
};
~~~
Then you would use this one as follows:
~~~{.cpp}
CallForAllControlledUnits(SetSpeedRateHelper(mtype, forced),
                          CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM | CONTROLLED_MINIPET);
~~~
And it would call the `operator()` in the `SetSpeedRateHelper` with a given `Unit` for all the pets, guardians, charmed units and minipets.
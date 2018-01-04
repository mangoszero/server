#include "botpch.h"
#include "../../playerbot.h"
#include "WarriorActions.h"

using namespace ai;

NextAction** CastRendAction::getPrerequisites()
{
    return NextAction::merge( NextAction::array(0, new NextAction("reach melee"), nullptr), CastDebuffSpellAction::getPrerequisites());
}

NextAction** CastRendOnAttackerAction::getPrerequisites()
{
    return NextAction::merge( NextAction::array(0, new NextAction("reach melee"), nullptr), CastDebuffSpellOnAttackerAction::getPrerequisites());
}

NextAction** CastDisarmAction::getPrerequisites()
{
    return NextAction::merge( NextAction::array(0, new NextAction("reach melee"), new NextAction("defensive stance"), nullptr), CastDebuffSpellAction::getPrerequisites());
}

NextAction** CastSunderArmorAction::getPrerequisites()
{
    return NextAction::merge( NextAction::array(0, new NextAction("reach melee"), nullptr), CastDebuffSpellAction::getPrerequisites());
}

NextAction** CastRevengeAction::getPrerequisites()
{
    return NextAction::merge( NextAction::array(0, new NextAction("defensive stance"), nullptr), CastMeleeSpellAction::getPrerequisites());
}

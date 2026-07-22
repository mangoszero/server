#include "botpch.h"
#include "../../playerbot.h"
#include "CcActions.h"

using namespace ai;

bool CcReachSpellAction::Execute(Event event)
{
    Unit* target = AI_VALUE(Unit*, "cc reach target");
    if (!target)
    {
        return false;
    }

    float dist = bot->GetDistance(target);
    if (dist <= sPlayerbotAIConfig.spellDistance +
        sPlayerbotAIConfig.tooCloseDistance)
    {
        if (!bot->IsWithinLOSInMap(target))
        {
            float nx, ny, nz;
            if (FindNearbyLosPoint(target, nx, ny, nz))
            {
                return MoveTo(bot->GetMapId(), nx, ny, nz);
            }
        }
        return false;
    }

    return MoveTo(target, sPlayerbotAIConfig.spellDistance);
}

bool CcReachSpellAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "cc reach target");
    if (!target)
    {
        return false;
    }

    if (bot->GetDistance(target) > sPlayerbotAIConfig.spellDistance ||
        !bot->IsWithinLOSInMap(target))
    {
        return true;
    }

    return false;
}

bool CastCcOnMyTargetAction::isPersistent()
{
    if (bot->getClass() == CLASS_ROGUE)
    {
        return false;
    }

    return !m_ccWithdrawn;
}

bool CastCcOnMyTargetAction::isUseful()
{
    if (bot->getClass() == CLASS_ROGUE)
    {
        return false;
    }

    if (m_ccWithdrawn)
    {
        Player* master = GetMaster();
        if (master)
        {
            Unit* target = sObjectAccessor.GetUnit(*bot, master->GetSelectionGuid());
            ObjectGuid currentGuid = target ? target->GetObjectGuid() : ObjectGuid();
            ObjectGuid ccGuid = m_ccTargetGuid;

            if (currentGuid != ccGuid)
            {
                m_castAttempts = 0;
                m_ccWithdrawn = false;
                m_ccTargetGuid = ObjectGuid();
                return true;
            }
        }

        return false;
    }

    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    Unit* target = sObjectAccessor.GetUnit(*bot, master->GetSelectionGuid());
    if (!target || !target->IsAlive())
    {
        return false;
    }

    return true;
}

NextAction** CastCcOnMyTargetAction::getPrerequisites()
{
    if (bot->getClass() == CLASS_ROGUE)
    {
        return NULL;
    }

    Player* master = GetMaster();
    if (!master)
    {
        return NULL;
    }

    Unit* target = sObjectAccessor.GetUnit(*bot, master->GetSelectionGuid());
    if (!target)
    {
        return NULL;
    }

    string ccSpell = GetCcSpell(target);
    if (ccSpell.empty() || ai->HasAura(ccSpell, target))
    {
        return NULL;
    }

    context->GetValue<Unit*>("current target")->Set(target);
    context->GetValue<Unit*>("cc reach target")->Set(target);

    return NextAction::array(0, new NextAction("cc reach spell"), NULL);
}

bool CastCcOnMyTargetAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    Unit* target = sObjectAccessor.GetUnit(*bot, master->GetSelectionGuid());
    if (!target)
    {
        ai->TellMaster("You have no target");
        return false;
    }

    if (bot->getClass() == CLASS_ROGUE)
    {
        context->GetValue<Unit*>("current target")->Set(target);
        ai->ChangeStrategy("+sap", BOT_STATE_NON_COMBAT);
        return true;
    }

    string ccSpell = GetCcSpell(target);
    if (ccSpell.empty())
    {
        ai->TellMaster("I have no crowd control spell");
        return false;
    }

    if (ai->HasAura(ccSpell, target))
    {
        if (!m_ccWithdrawn)
        {
            m_ccTargetGuid = target->GetObjectGuid();
            WithdrawToGroupCenter(target);
            m_ccWithdrawn = true;
        }
        return true;
    }

    if (bot->IsNonMeleeSpellCasted(false))
    {
        return true;
    }

    float range = AI_VALUE2(float, "spell range", ccSpell);
    if (bot->GetDistance(target) > range)
    {
        return MoveTo(target, range);
    }

    uint32 spellId = AI_VALUE2(uint32, "spell id", ccSpell);
    if (!spellId)
    {
        ostringstream msg;
        msg << "I don't know spell " << ccSpell;
        ai->TellMaster(msg.str());
        return false;
    }

    if (m_lastCastAttempt > 0 && time(0) - m_lastCastAttempt < 1)
    {
        return true;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (spellInfo && bot->GetShapeshiftForm() != FORM_NONE)
    {
        if (GetErrorAtShapeshiftedCast(spellInfo, bot->GetShapeshiftForm()) == SPELL_FAILED_NOT_SHAPESHIFT)
        {
            ai->RemoveShapeshift();
        }
    }

    ai->CastSpell(spellId, target);
    m_lastCastAttempt = time(0);

    Spell* pcs = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);

    if (!pcs)
    {
        m_castAttempts++;

        if (m_castAttempts >= 3)
        {
            ai->TellMaster("I cannot cast that");
            m_ccWithdrawn = true;
            return false;
        }
        return true;
    }

    m_castAttempts = 0;

    ostringstream msg;
    msg << "Casting " << ccSpell << " on " << target->GetName();
    ai->TellMasterNoFacing(msg.str());
    return true;
}

string CastCcOnMyTargetAction::GetCcSpell(Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
            return "polymorph";
        case CLASS_WARLOCK:
        {
            uint32 ctype = target->GetCreatureType();
            if (ctype == CREATURE_TYPE_DEMON ||
                ctype == CREATURE_TYPE_ELEMENTAL)
            {
                return "banish";
            }
            return "fear";
        }
        case CLASS_HUNTER:
            return "freezing trap";
        case CLASS_PRIEST:
        {
            if (target->GetCreatureType() == CREATURE_TYPE_UNDEAD)
            {
                return "shackle undead";
            }
            return "";
        }
        case CLASS_DRUID:
        {
            uint32 ctype = target->GetCreatureType();
            if (ctype == CREATURE_TYPE_BEAST ||
                ctype == CREATURE_TYPE_DRAGONKIN)
            {
                return "hibernate";
            }
            return "entangling roots";
        }
        default:
            return "";
    }
}

void CastCcOnMyTargetAction::WithdrawToGroupCenter(Unit* ccTarget)
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        return;
    }

    float myDist = bot->GetDistance(ccTarget);
    bool amClosest = true;

    float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
    int count = 0;

    for (GroupReference* ref = group->GetFirstMember();
         ref; ref = ref->next())
    {
        Player* member = ref->getSource();
        if (!member || member == bot)
        {
            continue;
        }

        float dist = member->GetDistance(ccTarget);
        if (dist < myDist)
        {
            amClosest = false;
        }

        sumX += member->GetPositionX();
        sumY += member->GetPositionY();
        sumZ += member->GetPositionZ();
        count++;
    }

    if (!amClosest || count == 0)
    {
        return;
    }

    float medianX = sumX / count;
    float medianY = sumY / count;
    float medianZ = sumZ / count;

    MoveTo(bot->GetMapId(), medianX, medianY, medianZ);
}

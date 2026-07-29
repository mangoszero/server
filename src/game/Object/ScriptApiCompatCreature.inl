// Creature's share of the old spatial API, for ELUNA only. Eluna is shared upstream
// between several cores and absorbs their differences in its own tree, so it has not
// followed this fork's rename of the respawn pose to a placement.
//
// SD3 no longer needs any of this -- it calls Spawn()/SetSpawn() directly. What is left
// is the two methods Eluna's CreatureMethods.h actually uses. GetCombatStartPosition,
// SetCombatStartPosition, ResetRespawnCoord and the reach family all went with it,
// because nothing called them any more.
//
// DELETE THIS FILE as soon as Eluna is updated. There is nothing here worth keeping.

public:

    void SetRespawnCoord(CreatureCreatePos const& pos) { SetSpawn(pos); }

    void SetRespawnCoord(float x, float y, float z, float ori)
    {
        SetSpawn(Geometry::Vector3(x, y, z), ori);
    }

    void GetRespawnCoord(float& x, float& y, float& z, float* ori = NULL,
                         float* dist = NULL) const
    {
        x = Spawn().X();
        y = Spawn().Y();
        z = Spawn().Z();
        if (ori)
        {
            *ori = Spawn().Facing();
        }
        if (dist)
        {
            *dist = GetRespawnRadius();
        }
    }

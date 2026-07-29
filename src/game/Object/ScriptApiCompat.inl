// THE OLD SPATIAL API, FOR THE SUBMODULES ONLY.
//
// SD3 and Eluna are never modified here -- a local commit inside a submodule makes the
// parent reference an object on no remote -- and both were written against a WorldObject
// that owned its coordinates. This hands that shape back to them. Every one of these is a
// one-line forward to the placement component or to the free functions beside it, defined
// in ScriptApiCompat.cpp, so there is no second implementation of anything.
//
// Included INSIDE the class body under MANGOS_SCRIPT_COMPAT and nowhere else: with the
// scripts off the guard is undefined, these members do not exist, and `ctest -R
// spatial_boundary` still means what it says. New code asks the component --
// obj->Where().DistanceTo(other->Where()) -- whatever is declared here.

public:

    float GetPositionX() const;
    float GetPositionY() const;
    float GetPositionZ() const;
    float GetOrientation() const;
    void GetPosition(float& x, float& y, float& z) const;
    float GetObjectBoundingRadius() const;

    bool IsInMap(WorldObject const* obj) const;
    float GetDistance(WorldObject const* obj) const;
    float GetDistance(float x, float y, float z) const;
    float GetDistance2d(WorldObject const* obj) const;
    float GetDistance2d(float x, float y) const;
    float GetDistanceZ(WorldObject const* obj) const;
    bool GetDistanceOrder(WorldObject const* obj1, WorldObject const* obj2,
                          bool is3D = true) const;

    bool IsWithinDist3d(float x, float y, float z, float dist) const;
    bool IsWithinDist2d(float x, float y, float dist) const;
    bool IsWithinDist(WorldObject const* obj, float dist, bool is3D = true) const;
    bool _IsWithinDist(WorldObject const* obj, float dist, bool is3D) const;
    bool IsWithinDistInMap(WorldObject const* obj, float dist, bool is3D = true) const;
    bool IsWithinLOS(float x, float y, float z) const;
    bool IsWithinLOSInMap(WorldObject const* obj) const;

    float GetAngle(WorldObject const* obj) const;
    float GetAngle(float x, float y) const;
    bool HasInArc(float arc, WorldObject const* obj) const;
    bool IsInFront(WorldObject const* obj, float dist, float arc = M_PI_F) const;
    bool IsInBack(WorldObject const* obj, float dist, float arc = M_PI_F) const;
    bool IsInFrontInMap(WorldObject const* obj, float dist, float arc = M_PI_F) const;
    bool IsInBackInMap(WorldObject const* obj, float dist, float arc = M_PI_F) const;

    bool IsInRange(WorldObject const* obj, float minRange, float maxRange,
                   bool is3D = true) const;
    bool IsInRange2d(float x, float y, float minRange, float maxRange) const;
    bool IsInRange3d(float x, float y, float z, float minRange, float maxRange) const;

    /// Never existed in this fork -- SD3 calls it and carries a TODO saying so. A per-axis
    /// box, which is what the waypoint tolerances in the database have always meant.
    bool IsNearWaypoint(float x, float y, float z, float wpX, float wpY, float wpZ,
                        float tolX, float tolY, float tolZ) const;

    bool IsPositionValid() const;
    void UpdateGroundPositionZ(float x, float y, float& z) const;
    void UpdateAllowedPositionZ(float x, float y, float& z, Map* atMap = NULL) const;

    void GetNearPoint2D(float& x, float& y, float distance2d, float absAngle) const;
    void GetNearPoint(WorldObject const* searcher, float& x, float& y, float& z,
                      float searcherBounding, float distance2d, float absAngle) const;
    void GetClosePoint(float& x, float& y, float& z, float bounding,
                       float distance2d = 0.0f, float angle = 0.0f,
                       WorldObject const* obj = NULL) const;
    void GetContactPoint(WorldObject const* obj, float& x, float& y, float& z,
                         float distance2d = CONTACT_DISTANCE) const;
    void GetRandomPoint(float x, float y, float z, float distance,
                        float& randX, float& randY, float& randZ,
                        float minDist = 0.0f, float const* ori = NULL) const;

    uint32 GetZoneId() const;
    uint32 GetAreaId() const;
    void GetZoneAndAreaId(uint32& zoneid, uint32& areaid) const;

    void Relocate(float x, float y, float z, float o);
    void Relocate(float x, float y, float z);
    void SetOrientation(float o);

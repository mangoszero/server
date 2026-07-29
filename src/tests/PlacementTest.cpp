/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "TestHarness.h"
#include "Geometry/Placement.h"

#include <cmath>

// Every fixture here is a REAL row of the 3.3.5a world database, cited by guid or entry,
// so a failure is a statement about Azeroth and not about an imaginary unit cube. Expected
// values are hand-derived from those coordinates.
//
//   two_world.creature guid 518 / 4089   entry 14991 League of Arathor Emissary, map 0,
//                                        Stormwind; creature_model_info bounding_radius
//                                        0.306, combat_reach 1.5. They stand 2.43 yards
//                                        apart, deliberately facing each other.
//   two_world.creature guid 79680        entry 6174 Stephanie Turner, map 0, radius 0.208
//   two_world.creature guid 79688        entry 1432 Renato Gallina, map 0, radius 0.306
//   two_world.transports entry 175080    the Orgrimmar / Grom'gol zeppelin, "The Iron Eagle"

static const float kPi = Geometry::pif();

static const uint32_t MAP_EASTERN_KINGDOMS = 0;
static const uint32_t MAP_KALIMDOR = 1;
static const uint32_t MAP_NAXXRAMAS = 533;
static const uint64_t ZEPPELIN_IRON_EAGLE = 175080;

static bool Approx(float a, float b, float tol = 1e-3f)
{
    return std::fabs(a - b) < tol;
}

static Geometry::Placement Spawn(float x, float y, float z, float o, float radius,
                                 uint32_t mapId = MAP_EASTERN_KINGDOMS, uint32_t instanceId = 0)
{
    Geometry::Placement p(radius);
    p.EnterFrame(Geometry::Frame::World(mapId, instanceId), Geometry::Vector3(x, y, z), o);
    return p;
}

// two_world.creature guid 518
static Geometry::Placement ArathorEmissary()
{
    return Spawn(-8840.63f, 652.959f, 97.1184f, 5.60251f, 0.306f);
}

// two_world.creature guid 4089 -- the other emissary of the pair
static Geometry::Placement ArathorEmissaryPair()
{
    return Spawn(-8838.80f, 651.394f, 96.7842f, 2.42601f, 0.306f);
}

// two_world.creature guid 79680
static Geometry::Placement StephanieTurner()
{
    return Spawn(-8832.27f, 613.706f, 94.2841f, 2.05949f, 0.208f);
}

// two_world.creature guid 79688
static Geometry::Placement RenatoGallina()
{
    return Spawn(-8848.26f, 614.981f, 95.2698f, 5.11381f, 0.306f);
}

TEST(Placement_StormwindEmissariesStandTwoYardsApart)
{
    // sqrt(1.83^2 + 1.565^2 + 0.3342^2) = 2.431011, less both 0.306 radii.
    CHECK(Approx(ArathorEmissary().DistanceTo(ArathorEmissaryPair()), 1.819011f));
    CHECK(Approx(ArathorEmissaryPair().DistanceTo(ArathorEmissary()), 1.819011f));
}

TEST(Placement_FlatDistanceDropsTheThirdOfAYardOfHeight)
{
    CHECK(Approx(ArathorEmissary().DistanceTo(ArathorEmissaryPair(), false), 1.795930f));
}

TEST(Placement_TheirRadiiSwallowTheHeightDifference)
{
    // 0.3342 yards of z between them, against 0.612 of combined radius.
    CHECK(Approx(ArathorEmissary().HeightGapTo(ArathorEmissaryPair()), 0.0f));
}

TEST(Placement_EmissariesAreInEachOthersMeleeReach)
{
    // combat_reach 1.5 each; they are 1.819 apart surface to surface.
    CHECK(ArathorEmissary().WithinDist(ArathorEmissaryPair(), 1.5f + 1.5f));
    CHECK(!ArathorEmissary().WithinDist(ArathorEmissaryPair(), 1.0f));
}

TEST(Placement_TheTwoEmissariesFaceEachOther)
{
    // Bearing 518 -> 4089 is 5.575685 against a stored orientation of 5.60251, and the
    // reverse is 2.434093 against 2.42601: both off by under 1.6 degrees, which is what
    // "placed facing each other" looks like in the spawn table.
    CHECK(Approx(ArathorEmissary().RelativeBearingTo(ArathorEmissaryPair()), -0.026825f));
    CHECK(Approx(ArathorEmissaryPair().RelativeBearingTo(ArathorEmissary()), 0.008083f));
    CHECK(ArathorEmissary().HasInArc(ArathorEmissaryPair(), 0.1f));
    CHECK(ArathorEmissaryPair().HasInArc(ArathorEmissary(), 0.1f));
}

TEST(Placement_NeitherEmissaryHasTheOtherBehindIt)
{
    CHECK(ArathorEmissary().IsInFront(ArathorEmissaryPair(), 5.0f, kPi));
    CHECK(!ArathorEmissary().IsInBack(ArathorEmissaryPair(), 5.0f, kPi));
}

TEST(Placement_StormwindResidentsAreOutOfInteractionRange)
{
    // Turner to Gallina is 16.07 centre to centre: past INTERACTION_DISTANCE (5 yards),
    // inside the 30-yard band a caster would use.
    CHECK(Approx(StephanieTurner().DistanceTo(RenatoGallina()), 15.557009f));
    CHECK(!StephanieTurner().WithinDist(RenatoGallina(), 5.0f));
    CHECK(StephanieTurner().WithinRange(RenatoGallina(), 5.0f, 30.0f));
    CHECK(!StephanieTurner().WithinRange(RenatoGallina(), 20.0f, 30.0f));
}

TEST(Placement_TurnerIsNearerToGallinaThanToTheEmissary)
{
    CHECK(StephanieTurner().IsNearer(RenatoGallina(), ArathorEmissary()));
    CHECK(!StephanieTurner().IsNearer(ArathorEmissary(), RenatoGallina()));
}

TEST(Placement_ADistanceToABareStormwindPointUsesOnlyTheNpcRadius)
{
    // The emissary to Gallina's exact spot, as a coordinate rather than a creature.
    const Geometry::Placement turner = StephanieTurner();
    CHECK(Approx(turner.DistanceTo(Geometry::Vector3(-8848.26f, 614.981f, 95.2698f)),
                 16.071009f - 0.208f));
    CHECK(Approx(turner.DistanceTo(Geometry::Vector2(-8848.26f, 614.981f)),
                 15.832752f, 2e-3f));
}

TEST(Placement_TheSameStormwindSpotOnKalimdorIsUnreachable)
{
    // Map 0 and map 1 both have coordinates around (-8840, 652); they are not near.
    const Geometry::Placement inStormwind = ArathorEmissary();
    const Geometry::Placement inKalimdor = Spawn(-8840.63f, 652.959f, 97.1184f, 0.0f, 0.306f,
                                                 MAP_KALIMDOR);
    CHECK(std::isinf(inStormwind.DistanceTo(inKalimdor)));
    CHECK(!inStormwind.WithinDist(inKalimdor, 1000.0f));
    CHECK(!inStormwind.HasInArc(inKalimdor, 2.0f * kPi - 0.001f));
}

TEST(Placement_TwoNaxxramasCopiesNeverSeeEachOther)
{
    // Same boss room, two raid instances: identical coordinates, different worlds.
    const Geometry::Placement copy1 = Spawn(3155.0f, -3313.0f, 293.0f, 0.0f, 5.0f, MAP_NAXXRAMAS, 1);
    const Geometry::Placement copy2 = Spawn(3155.0f, -3313.0f, 293.0f, 0.0f, 5.0f, MAP_NAXXRAMAS, 2);
    CHECK(std::isinf(copy1.DistanceTo(copy2)));
    CHECK(!copy1.WithinDist(copy2, 0.001f));
    CHECK(copy1.WithinDist(copy1, 0.001f));
}

TEST(Placement_ZeppelinDeckIsNotTheWorldItFliesOver)
{
    // A passenger's deck offset on The Iron Eagle happens to read like a Durotar
    // coordinate. Same numbers, different frame: no distance exists between them.
    Geometry::Placement onDeck(0.383f);
    onDeck.EnterFrame(Geometry::Frame::Deck(ZEPPELIN_IRON_EAGLE),
                      Geometry::Vector3(5.0f, -2.5f, 8.0f), 0.0f);
    const Geometry::Placement ashore = Spawn(5.0f, -2.5f, 8.0f, 0.0f, 0.383f);
    CHECK(std::isinf(onDeck.DistanceTo(ashore)));
    CHECK(!onDeck.WithinDist(ashore, 100.0f));
    CHECK(onDeck.CurrentFrame().IsDeck());
    CHECK(!ashore.CurrentFrame().IsDeck());
}

TEST(Placement_TwoPassengersOnTheSameZeppelinAreOrdinaryNeighbours)
{
    Geometry::Placement gobber(0.306f);
    gobber.EnterFrame(Geometry::Frame::Deck(ZEPPELIN_IRON_EAGLE),
                      Geometry::Vector3(0.0f, 0.0f, 0.0f), 0.0f);
    Geometry::Placement passenger(0.306f);
    passenger.EnterFrame(Geometry::Frame::Deck(ZEPPELIN_IRON_EAGLE),
                         Geometry::Vector3(4.0f, 0.0f, 0.0f), kPi);
    CHECK(Approx(gobber.DistanceTo(passenger), 4.0f - 0.612f));
    CHECK(gobber.IsInFront(passenger, 5.0f, kPi));
    CHECK(passenger.IsInFront(gobber, 5.0f, kPi));
}

TEST(Placement_ADifferentVesselIsADifferentDeck)
{
    Geometry::Placement onEagle(0.306f);
    onEagle.EnterFrame(Geometry::Frame::Deck(ZEPPELIN_IRON_EAGLE),
                       Geometry::Vector3(1.0f, 1.0f, 0.0f), 0.0f);
    Geometry::Placement onMoonspray(0.306f);   // two_world.transports entry 176244
    onMoonspray.EnterFrame(Geometry::Frame::Deck(176244),
                           Geometry::Vector3(1.0f, 1.0f, 0.0f), 0.0f);
    CHECK(std::isinf(onEagle.DistanceTo(onMoonspray)));
}

TEST(Placement_AnUnspawnedCreatureIsNowhere)
{
    // Constructed but never placed on a map: fails closed rather than sitting at (0,0,0)
    // in the middle of the Eastern Kingdoms.
    const Geometry::Placement fresh(0.306f);
    CHECK(!fresh.IsPlaced());
    CHECK(!fresh.ShareFrame(fresh));
    CHECK(std::isinf(fresh.DistanceTo(ArathorEmissary())));
    CHECK(!ArathorEmissary().WithinDist(fresh, 10000.0f));
}

TEST(Placement_DespawningUnplacesWithoutMovingTheBody)
{
    Geometry::Placement emissary = ArathorEmissary();
    emissary.LeaveFrame();
    CHECK(!emissary.IsPlaced());
    CHECK(Approx(emissary.X(), -8840.63f, 1e-2f));
    CHECK(std::isinf(emissary.DistanceTo(ArathorEmissaryPair())));
}

TEST(Placement_SpawnOrientationsSurviveNormalisation)
{
    // The spawn table stores 0..2*PI, and a script that turns a creature by -PI/2 or by
    // a full turn plus a quarter must land in the same range.
    Geometry::Placement emissary = ArathorEmissary();
    CHECK(Approx(emissary.Facing(), 5.60251f));
    emissary.Face(-0.5f * kPi);
    CHECK(Approx(emissary.Facing(), 1.5f * kPi));
    emissary.Face(2.5f * kPi);
    CHECK(Approx(emissary.Facing(), 0.5f * kPi));
}

TEST(Placement_FacingTowardGallinaTurnsTheEmissaryAround)
{
    Geometry::Placement turner = StephanieTurner();
    turner.FaceToward(RenatoGallina().Pos());
    CHECK(turner.HasInArc(RenatoGallina(), 0.01f));
    CHECK(!turner.IsInBack(RenatoGallina(), 20.0f, kPi));
}

TEST(Placement_WalkingDoesNotChangeWhereTheCreatureBelongs)
{
    Geometry::Placement emissary = ArathorEmissary();
    const Geometry::Frame home = emissary.CurrentFrame();
    emissary.MoveTo(-8830.0f, 640.0f, 94.0f);
    CHECK(emissary.CurrentFrame() == home);
    CHECK(Approx(emissary.Facing(), 5.60251f));
    CHECK(emissary.ShareFrame(ArathorEmissaryPair()));
}

TEST(Placement_ConeOfColdCatchesGallinaOnlyWhenFacedAtHim)
{
    // A 90 degree frontal cone (SPELL_FACING_FLAG_INFRONT style check) at 30 yards.
    const float cone = 0.5f * kPi;
    Geometry::Placement caster = StephanieTurner();
    caster.FaceToward(RenatoGallina().Pos());
    CHECK(caster.IsInFront(RenatoGallina(), 30.0f, cone));
    caster.Face(caster.Facing() + kPi);
    CHECK(!caster.IsInFront(RenatoGallina(), 30.0f, cone));
    CHECK(caster.IsInBack(RenatoGallina(), 30.0f, kPi));
}

TEST(Placement_AGuardPostedAheadOfTheEmissaryLandsOnTheStormwindStreet)
{
    // Five yards along the emissary's own facing, which is where a summon or a follow
    // slot goes before the terrain is asked for a height.
    const Geometry::Placement emissary = ArathorEmissary();
    const Geometry::Vector3 spot = emissary.PointAhead(5.0f);
    CHECK(Approx(emissary.DistanceTo(Geometry::Vector2(spot.x, spot.y)), 5.0f - 0.306f));
    CHECK(Approx(spot.z, 97.1184f, 1e-2f));
    CHECK(Approx(emissary.BearingTo(spot), 5.60251f));
}

TEST(Placement_ContactPointStopsHalfAYardShortOfBothHitboxes)
{
    const Geometry::Placement a = ArathorEmissary();
    const Geometry::Placement b = ArathorEmissaryPair();
    // CONTACT_DISTANCE is 0.5; the spot must clear 0.306 + 0.306 of hitbox as well.
    const Geometry::Vector3 contact = a.ContactPointToward(b, 0.5f);
    CHECK(Approx(a.DistanceTo(Geometry::Vector2(contact.x, contact.y)),
                 0.5f + 0.306f, 2e-3f));
    CHECK(Approx(a.BearingTo(contact), 5.575685f));
}

TEST(Placement_WanderRadiusStaysInsideTheSpawnRing)
{
    // creature.spawndist for a wandering Stormwind npc is 5 yards; the roll is injected.
    const Geometry::Placement emissary = ArathorEmissary();
    const Geometry::Vector3 nearest = emissary.RandomPointAround(0.0f, 5.0f, 1.0f, 0.0f);
    const Geometry::Vector3 farthest = emissary.RandomPointAround(0.0f, 5.0f, 1.0f, 1.0f);
    CHECK(Approx(emissary.DistanceTo(Geometry::Vector2(nearest.x, nearest.y)), 0.0f));
    CHECK(Approx(emissary.DistanceTo(Geometry::Vector2(farthest.x, farthest.y)), 5.0f - 0.306f));
    CHECK(Approx(farthest.z, 97.1184f, 1e-2f));
}

TEST(Placement_GrowingAModelGrowsEveryReachTest)
{
    // A druid in bear form, or a GM-scaled npc: the extent is the component's, so one
    // Resize moves every distance and reach answer at once.
    Geometry::Placement emissary = ArathorEmissary();
    CHECK(!emissary.WithinDist(ArathorEmissaryPair(), 1.0f));
    emissary.Resize(2.0f);
    CHECK(emissary.WithinDist(ArathorEmissaryPair(), 1.0f));
    CHECK(Approx(emissary.DistanceTo(ArathorEmissaryPair()), 2.431011f - 2.0f - 0.306f));
}

TEST(Placement_ARoomIsNotAFrame)
{
    // Both emissaries share Stormwind, so they share a frame; the deck of a zeppelin
    // parked in Orgrimmar does not, whatever its coordinates say.
    CHECK(ArathorEmissary().ShareFrame(ArathorEmissaryPair()));
    Geometry::Placement onDeck(0.306f);
    onDeck.EnterFrame(Geometry::Frame::Deck(ZEPPELIN_IRON_EAGLE),
                      Geometry::Vector3(-8840.63f, 652.959f, 97.1184f), 0.0f);
    CHECK(!ArathorEmissary().ShareFrame(onDeck));
}

TEST(Placement_WaypointToleranceIsPerAxis)
{
    // creature_movement tolerances arrive as database integers, and a zero means "do not
    // test this axis at all" -- with all three zero nothing matches, by design.
    const Geometry::Placement emissary = ArathorEmissary();
    const Geometry::Vector3 target(-8838.80f, 651.394f, 96.7842f);   // the other emissary
    CHECK(emissary.WithinBox(target, Geometry::Vector3(2.0f, 2.0f, 1.0f)));
    CHECK(!emissary.WithinBox(target, Geometry::Vector3(1.0f, 2.0f, 1.0f)));
    CHECK(emissary.WithinBox(target, Geometry::Vector3(0.0f, 2.0f, 0.0f)));
    CHECK(!emissary.WithinBox(target, Geometry::Vector3(0.0f, 0.0f, 0.0f)));
}

TEST(Placement_BasisCarriesASeatOffsetOutToTheWorld)
{
    // A vehicle seat two yards forward and one to the left, on a mount facing due north
    // (PI/2): forward becomes +y, left becomes -x.
    Geometry::Placement mount(0.75f);
    mount.EnterFrame(Geometry::Frame::World(MAP_EASTERN_KINGDOMS, 0),
                     Geometry::Vector3(-8840.0f, 650.0f, 97.0f), 0.5f * kPi);
    const Geometry::Vector3 world = mount.Basis().localToWorld(Geometry::Vector3(2.0f, 1.0f, 0.5f));
    CHECK(Approx(world.x, -8841.0f));
    CHECK(Approx(world.y, 652.0f));
    CHECK(Approx(world.z, 97.5f));
}

TEST(Placement_BasisAndItsInverseRoundTrip)
{
    Geometry::Placement mount(0.75f);
    mount.EnterFrame(Geometry::Frame::World(MAP_EASTERN_KINGDOMS, 0),
                     Geometry::Vector3(-8840.63f, 652.959f, 97.1184f), 2.42601f);
    const Geometry::Vector3 seat(1.5f, -0.5f, 1.25f);
    const Geometry::Vector3 back = mount.Basis().worldToLocal(mount.Basis().localToWorld(seat));
    CHECK(Approx(back.x, seat.x));
    CHECK(Approx(back.y, seat.y));
    CHECK(Approx(back.z, seat.z));
}

TEST(Placement_GapIsTheReachTakenOffASeparation)
{
    // Combat reach replaces the extents for a melee test: 2.43 yards apart, 1.5 + 1.5 of
    // reach plus the melee offset, so the two emissaries are touching.
    CHECK(Approx(Geometry::Placement::Gap(2.431011f, 3.0f), 0.0f));
    CHECK(Approx(Geometry::Placement::Gap(2.431011f, 1.0f), 1.431011f));
}

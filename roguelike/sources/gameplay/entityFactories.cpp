#include "entityFactories.hpp"
#include "components.hpp"

flecs::entity create_monster(flecs::world& world, int x, int y)
{
  return world.entity()
    .set(Position{{x, y}})
    .set(MovePos{{x, y}})
    .set(PatrolPos{4, {x, y}})
    .set(Hitpoints{100.f})
    .set<Visibility>({6})
    .add<ClosestVisibleEnemy, NoVisibleEntity>()
    .set(HitpointsThresholds{25.f, 50.f})
    .set(HitpointsRegen{10.f})
    .set(Action{ActionType::NOP})
    .set(Team{1})
    .set(NumActions{1, 0})
    .set(MeleeDamage{10.f});
}

flecs::entity create_player(flecs::world& world, int x, int y)
{
  return world.entity("player")
    .set(Position{{x, y}})
    .set(MovePos{{x, y}})
    .set(Hitpoints{100.f})
    .set(HitpointsThresholds{50.f, 100.f})
    .set(Action{ActionType::NOP})
    .add<IsPlayer>()
    .set(Team{0})
    .set(NumActions{2, 0})
    .set(MeleeDamage{10.f});
}

flecs::entity create_friend(flecs::world& world, int x, int y)
{
  return world.entity()
    .set(Position{{x, y}})
    .set(MovePos{{x, y}})
    .set(Hitpoints{100.f})
    .set<Visibility>({6})
    .add<ClosestVisibleEnemy, NoVisibleEntity>()
    .add<ClosestVisibleAlly, NoVisibleEntity>()
    .set(HitpointsThresholds{50.f, 75.f})
    .set(HitpointsRegen{10.f})
    .set(Action{ActionType::NOP})
    .set(Team{0})
    .set(NumActions{1, 0})
    .set(MeleeDamage{10.f});
}

void create_heal(flecs::world& world, int x, int y, float amount)
{
  world.entity()
    .set(Position{{x, y}})
    .set(HealAmount{amount})
    .set(Color{0xff4444ff});
}

void create_powerup(flecs::world& world, int x, int y, float amount)
{
  world.entity()
    .set(Position{{x, y}})
    .set(PowerupAmount{amount})
    .set(Color{0xff00ffff});
}

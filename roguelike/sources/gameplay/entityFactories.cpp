#include "entityFactories.hpp"
#include "components.hpp"
#include "actions.hpp"
#include <spdlog/fmt/fmt.h>


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

flecs::entity create_patrool_route(flecs::world& world, std::string_view name,
  SpriteId sprite, std::span<const glm::ivec2> coords)
{
  auto first = world.entity(fmt::format("{}_{}", name, 0).c_str())
    .set(Sprite{sprite})
    .set(Position{coords.front()});
  flecs::entity prev = first;
  for (std::size_t i = 1; i < coords.size(); ++i)
  {
    auto wp = world.entity(fmt::format("{}_{}", name, i).c_str())
      .set(Sprite{sprite})
      .set(Position{coords[i]});
    std::exchange(prev, wp).add<Waypoint>(wp);
  }
  prev.add<Waypoint>(first);
  return first;
}

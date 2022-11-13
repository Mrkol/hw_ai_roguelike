#include "entityFactories.hpp"
#include "components.hpp"
#include "actions.hpp"
#include "gameplay/dungeon/dmaps.hpp"
#include "gameplay/dungeon/dungeon.hpp"
#include <spdlog/fmt/fmt.h>


flecs::entity create_monster(flecs::world& world, glm::ivec2 pos)
{
  return world.entity()
    .set(Position{pos})
    .set(MovePos{pos})
    .set(PatrolPos{4, pos})
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

flecs::entity create_player(flecs::world& world, glm::ivec2 pos)
{
  return world.entity("player")
    .set(Position{pos})
    .set(MovePos{pos})
    .set(Hitpoints{100.f})
    .set(HitpointsThresholds{50.f, 100.f})
    .set(Action{ActionType::NOP})
    .add<IsPlayer>()
    .set(Team{0})
    .set(NumActions{2, 0})
    .set(MeleeDamage{10.f});
}

flecs::entity create_friend(flecs::world& world, glm::ivec2 pos)
{
  return world.entity()
    .set(Position{pos})
    .set(MovePos{pos})
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

void create_heal(flecs::world& world, glm::ivec2 pos, float amount)
{
  world.entity()
    .set(Position{pos})
    .set(HealAmount{amount})
    .set(Color{0xff4444ff});
}

void create_powerup(flecs::world& world, glm::ivec2 pos, float amount)
{
  world.entity()
    .set(Position{pos})
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

flecs::entity create_dmap(flecs::world& world,
  std::string_view name,
  flecs::entity dungeon,
  flecs::query<const Position> starting_points,
  fu2::function<dungeon::dmaps::PotentialFuncSig> potential)
{
  return world.entity(std::string(name).c_str())
    .set(std::move(starting_points))
    .add(flecs::ChildOf, dungeon)
    .set(dungeon::dmaps::PotentialHolder{std::move(potential)})
    .set(dungeon::dmaps::make(dungeon.get<dungeon::Dungeon>()->view));
}

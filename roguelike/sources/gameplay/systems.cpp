#include "systems.hpp"
#include "blackboard.hpp"
#include "components.hpp"
#include "actions.hpp"
#include "dungeon/dungeon.hpp"
#include "dungeon/dungeonUtils.hpp"
#include "gameplay/dungeon/dungeon.hpp"
#include "gameplay/dungeon/dungeonUtils.hpp"
#include "gameplay/dungeon/dmaps.hpp"


struct PerformTurn {};

flecs::entity register_systems(flecs::world& world)
{
  world.component<ClosestVisibleAlly>().add(flecs::Union);
  world.component<ClosestVisibleEnemy>().add(flecs::Union);
  world.component<glm::ivec2>()
    .member<int>("x")
    .member<int>("y");
  world.component<Position>()
    .member<glm::ivec2>("pos");
  world.component<Hitpoints>()
    .member<float>("hitpoints");
  world.component<HitpointsRegen>()
    .member<float>("regenPerTurn");
  world.component<HitpointsThresholds>()
    .member<float>("low")
    .member<float>("high");
  world.component<Visibility>()
    .member<float>("radius");
  world.component<MeleeDamage>()
    .member<float>("damage");
  world.component<Team>()
    .member<int>("team");
  world.component<HealAmount>()
    .member<float>("hitpoints");
  world.component<PowerupAmount>()
    .member<float>("damage");
  world.component<Sprite>()
    .member<std::size_t>("id");

  world.system<Action, Position, MovePos, const MeleeDamage, const Team>("calculate movement")
    .kind<PerformTurn>()
    .each(
      [ checkAttacks = world.query<const MovePos, Hitpoints, const Team>()
      , checkDungeon = world.query<const dungeon::Dungeon>()
      ]
      (flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const MeleeDamage &dmg, const Team &team)
      {
        Position nextPos{move(pos.v, a.action)};
        if (nextPos.v == pos.v)
        {
          mpos.v = nextPos.v;
          return;
        }

        bool blocked = false;
        checkAttacks.each([&](flecs::entity enemy, const MovePos &epos, Hitpoints &hp, const Team &enemy_team)
        {
          if (entity != enemy && epos.v == nextPos.v)
          {
            blocked = true;
            if (team.team != enemy_team.team)
              hp.hitpoints -= dmg.damage;
            if (auto acts = entity.get_mut<NumActions>())
              acts->curActions++;
          }
        });

        checkDungeon.each([&](const dungeon::Dungeon& d)
          {
            if (!dungeon::is_tile_walkable(d, nextPos.v))
              blocked = true;
          });

        if (blocked)
          a.action = ActionType::NOP;
        else
          mpos.v = nextPos.v;
      });

  world.system<Position, const MovePos>("perform movement")
    .kind<PerformTurn>()
    .each([&](Position &pos, const MovePos &mpos)
    {
      pos.v = mpos.v;
    });

  world.system<const Action, Hitpoints, const HitpointsRegen>("perform regen")
    .kind<PerformTurn>()
    .each(
      [](const Action& act, Hitpoints& hp, const HitpointsRegen& regen)
      {
        if (act.action == ActionType::REGEN)
          hp.hitpoints += regen.regenPerTurn;
      });

  world.system<const Action, const HitpointsRegen>("perform heal")
    .kind<PerformTurn>()
    .term<ClosestVisibleAlly>(flecs::Wildcard)
    .each(
      [](flecs::entity e, const Action& act, const HitpointsRegen& regen)
      {
        auto ally = e.target<ClosestVisibleAlly>();
        if (e.has<ClosestVisibleAlly, NoVisibleEntity>() || !ally.is_alive())
          return;

        if (act.action == ActionType::HEAL && ally.has<Hitpoints>())
        {
          ally.get_mut<Hitpoints>()->hitpoints += regen.regenPerTurn;
        }
      });

  world.system<Action>("clear actions")
    .kind<PerformTurn>()
    .each([&](Action &a)
    {
      a.action = ActionType::NOP;
    });

  world.system<const Hitpoints>()
    .kind<PerformTurn>()
    .each(
      [&](flecs::entity entity, const Hitpoints &hp)
      {
        if (hp.hitpoints <= 0.f)
          entity.destruct();
      });

  world.system<const Position, Hitpoints, MeleeDamage>()
    .kind<PerformTurn>()
    .each(
      [healPickup = world.query<const Position, const HealAmount>(),
        powerupPickup = world.query<const Position, const PowerupAmount>()]
      (const Position& pos, Hitpoints& hp, MeleeDamage& dmg)
      {
        healPickup.each([&](flecs::entity entity, const Position &ppos, const HealAmount &amt)
        {
          if (pos.v == ppos.v)
          {
            hp.hitpoints += amt.amount;
            entity.destruct();
          }
        });
        powerupPickup.each([&](flecs::entity entity, const Position &ppos, const PowerupAmount &amt)
        {
          if (pos.v == ppos.v)
          {
            dmg.damage += amt.amount;
            entity.destruct();
          }
        });
      });


  world.system<const Position, const Visibility, const Team>()
    .kind<PerformTurn>()
    .term<ClosestVisibleEnemy>(flecs::Wildcard).or_()
    .term<ClosestVisibleAlly>(flecs::Wildcard).or_()
    .each(
      [qother = world.query_builder<const Position, const Team>().build()]
      (flecs::entity e, const Position& pos1, Visibility vis, const Team& team1)
      {
        bool needsClosestEnemy = e.has<ClosestVisibleEnemy>(flecs::Wildcard);
        bool needsClosestAlly = e.has<ClosestVisibleAlly>(flecs::Wildcard);

        float closestEnemyDist = 0;
        float closestAllyDist = 0;
        const auto noEntity = e.world().component<NoVisibleEntity>();
        flecs::entity closestEnemy = noEntity;
        flecs::entity closestAlly = noEntity;

        qother.each(
          [&](flecs::entity e2, const Position& pos2, const Team& team2)
          {
            if (e == e2)
              return;

            auto dist = glm::length(glm::vec2(pos2.v - pos1.v));
            if (dist > vis.visibility)
              return;

            if (needsClosestEnemy && team1.team != team2.team
              && (closestEnemy == noEntity || dist < closestEnemyDist))
            {
              closestEnemyDist = dist;
              closestEnemy = e2;
            }

            if (needsClosestAlly && team1.team == team2.team
              && (closestAlly == noEntity || dist < closestAllyDist))
            {
              closestAllyDist = dist;
              closestAlly = e2;
            }
          });

        if (needsClosestAlly)
          e.add<ClosestVisibleAlly>(closestAlly);

        if (needsClosestEnemy)
          e.add<ClosestVisibleEnemy>(closestEnemy);
      });

  world.system<flecs::query<const Position>, dungeon::dmaps::Dmap, dungeon::dmaps::PotentialHolder>()
    .each([](flecs::entity e, flecs::query<const Position>& query, dungeon::dmaps::Dmap& dmap,
      dungeon::dmaps::PotentialHolder& potential)
    {
      dungeon::dmaps::clear(dmap.view);
      query.each(
        [&](const Position& pos)
        {
          dmap.view(pos.v.y, pos.v.x) = 0;
        });
      dungeon::dmaps::generate(dmap.view, e.parent().get<dungeon::Dungeon>()->view, potential.potential);
    });

  return world.pipeline()
    .term(flecs::System)
    .term<PerformTurn>()
    .build();
}

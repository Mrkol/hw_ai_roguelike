#include "systems.hpp"
#include "components.hpp"


static Position move_pos(Position pos, ActionType action)
{
  switch (action)
  {
  case ActionType::MOVE_LEFT:
    pos.v.x--;
    break;
  case ActionType::MOVE_RIGHT:
    pos.v.x++;
    break;
  case ActionType::MOVE_UP:
    pos.v.y++;
    break;
  case ActionType::MOVE_DOWN:
    pos.v.y--;
    break;
  }
  return pos;
}

struct PerformTurn {};

flecs::entity register_systems(flecs::world& world)
{
  world.component<ClosestVisibleAlly>().add(flecs::Union);
  world.component<ClosestVisibleEnemy>().add(flecs::Union);

  world.system<Action, Position, MovePos, const MeleeDamage, const Team>("calculate movement")
    .kind<PerformTurn>()
    .each(
      [checkAttacks = world.query<const MovePos, Hitpoints, const Team>()]
      (flecs::entity entity, Action &a, Position &pos, MovePos &mpos, const MeleeDamage &dmg, const Team &team)
      {
        Position nextPos = move_pos(pos, a.action);
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
  
  world.system<const IsPlayer, const Position, Hitpoints, MeleeDamage>()
    .kind<PerformTurn>()
    .each(
      [healPickup = world.query<const Position, const HealAmount>(),
        powerupPickup = world.query<const Position, const PowerupAmount>()]
      (const IsPlayer&, const Position &pos, Hitpoints &hp, MeleeDamage &dmg)
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

  return world.pipeline()
    .term(flecs::System)
    .term<PerformTurn>()
    .build();
}

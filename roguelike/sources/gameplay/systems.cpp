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
  
  world.system<const Action, Hitpoints, const HitpointsRegen>("perform heal")
    .kind<PerformTurn>()
    .each(
      [](const Action& act, Hitpoints& hp, const HitpointsRegen& regen)
      {
        if (act.action == ActionType::REGEN)
          hp.hitpoints += regen.regenPerTurn;
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

  
  world.system<const Position, ClosestVisibleEnemy, const Team>()
    .kind<PerformTurn>()
    .each(
      [qother = world.query_builder<const Position, const Team>().build()]
      (const Position& pos1, ClosestVisibleEnemy& closest, const Team& team1)
      {
        closest.pos = std::nullopt;

        qother.each(
          [&](const Position& pos2, const Team& team2)
          {
            if (team1.team == team2.team)
              return;

            auto distToEnemy = glm::length(glm::vec2(pos2.v - pos1.v));
            if (distToEnemy > closest.visibility)
              return;
            
            if (closest.pos.has_value() && glm::length(glm::vec2(*closest.pos - pos1.v)) < distToEnemy)
              return;
            
            closest.pos = pos2.v;
          });
      });

  return world.pipeline()
    .term(flecs::System)
    .term<PerformTurn>()
    .build();
}

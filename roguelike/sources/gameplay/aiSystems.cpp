#include "aiSystems.hpp"

#include <random>

#include <fmt/format.h>

#include "components.hpp"


struct SimulateAi {};

static ActionType move_towards(const glm::ivec2& from, const glm::ivec2& to)
{
  int deltaX = to.x - from.x;
  int deltaY = to.y - from.y;
  if (glm::abs(deltaX) > glm::abs(deltaY))
    return deltaX > 0 ? ActionType::MOVE_RIGHT : ActionType::MOVE_LEFT;
  return deltaY > 0 ? ActionType::MOVE_UP : ActionType::MOVE_DOWN;
}

static ActionType inverse_move(ActionType move)
{
  return move == ActionType::MOVE_LEFT  ? ActionType::MOVE_RIGHT :
         move == ActionType::MOVE_RIGHT ? ActionType::MOVE_LEFT :
         move == ActionType::MOVE_UP    ? ActionType::MOVE_DOWN :
         move == ActionType::MOVE_DOWN  ? ActionType::MOVE_UP : move;
}

SimulateAiInfo register_ai_systems(flecs::world& world, StateMachineTracker& tracker)
{
  auto eventsPhase = world.entity().add<SimulateAi>();

  using EvList = StateMachineTracker::EventList;
  
  auto enemyNearEvent = world.entity("enemy_near");
  world.system<EvList>("enemy_near event dispatcher")
    .kind(eventsPhase)
    .term(enemyNearEvent)
    .term<ClosestVisibleEnemy>(flecs::Wildcard)
    .each(
      [&tracker, enemyNearEvent]
      (flecs::entity e, EvList& evs)
      {
        // Could be optimized by using `iter`
        if (!e.has<ClosestVisibleEnemy, NoVisibleEntity>())
          evs.events.emplace(enemyNearEvent);
      });

  auto allyNearEvent = world.entity("ally_near");
  world.system<EvList>("ally_near event dispatcher")
    .kind(eventsPhase)
    .term(allyNearEvent)
    .term<ClosestVisibleAlly>(flecs::Wildcard)
    .each(
      [&tracker, allyNearEvent]
      (flecs::entity e, EvList& evs)
      {
        // Could be optimized by using `iter`
        if (!e.has<ClosestVisibleAlly, NoVisibleEntity>())
          evs.events.emplace(allyNearEvent);
      });

  auto hitpointsLowEvent = world.entity("hp_low");
  world.system<const Hitpoints, const HitpointsThresholds, EvList>("hp_low event dispatcher")
    .kind(eventsPhase)
    .term(hitpointsLowEvent)
    .each(
      [&tracker, hitpointsLowEvent]
      (flecs::entity e, const Hitpoints& hp, const HitpointsThresholds& threshold, EvList& evs)
      {
        if (hp.hitpoints < threshold.low)
          evs.events.emplace(hitpointsLowEvent);
      });

  auto hitpointsHighEvent = world.entity("hp_high");
  world.system<const Hitpoints, const HitpointsThresholds, EvList>("hp_high event dispatcher")
    .kind(eventsPhase)
    .term(hitpointsHighEvent)
    .each(
      [&tracker, hitpointsHighEvent]
      (flecs::entity e, const Hitpoints& hp, const HitpointsThresholds& threshold, EvList& evs)
      {
        if (hp.hitpoints > threshold.high)
          evs.events.emplace(hitpointsHighEvent);
      });

  auto allyHpLowEvent = world.entity("ally_hp_low");
  world.system<EvList>("ally_hp_low event dispatcher")
    .kind(eventsPhase)
    .term(allyHpLowEvent)
    .term<ClosestVisibleAlly>(flecs::Wildcard)
    .each(
      [&tracker, allyHpLowEvent]
      (flecs::entity e, EvList& evs)
      {
        auto ally = e.target<ClosestVisibleAlly>();
        if (e.has<ClosestVisibleAlly, NoVisibleEntity>() || !ally.is_alive())
          return;

        auto allyHp = ally.get<Hitpoints>();
        auto allyHpThres = ally.get<HitpointsThresholds>();
        if (allyHp && allyHpThres && allyHp->hitpoints < allyHpThres->low)
          evs.events.emplace(allyHpLowEvent);
      });


  auto allyHpHighEvent = world.entity("ally_hp_high");
  world.system<EvList>("ally_hp_high event dispatcher")
    .kind(eventsPhase)
    .term(allyHpHighEvent)
    .term<ClosestVisibleAlly>(flecs::Wildcard)
    .each(
      [&tracker, allyHpHighEvent]
      (flecs::entity e, EvList& evs)
      {
        auto ally = e.target<ClosestVisibleAlly>();
        if (e.has<ClosestVisibleAlly, NoVisibleEntity>() || !ally.is_alive())
          return;

        auto allyHp = ally.get<Hitpoints>();
        auto allyHpThres = ally.get<HitpointsThresholds>();
        if (allyHp && allyHpThres && allyHp->hitpoints > allyHpThres->high)
          evs.events.emplace(allyHpHighEvent);
      });


  auto stateTransitionPhase = world.entity().add<SimulateAi>().depends_on(eventsPhase);
  auto stateReactionPhase = world.entity().add<SimulateAi>().depends_on(stateTransitionPhase);


  static std::default_random_engine engine;

  auto createReactor =
    [&world, stateReactionPhase]
    <class... Comps>(const char* stateName)
    {
      auto state = world.entity(stateName);
      return world.system<Comps...>(fmt::format("{} reactor", stateName).c_str())
        .kind(stateReactionPhase)
        .term(flecs::Wildcard, state)
        .term(state).optional().read();
    };

  createReactor.operator()<Action, const Position, const PatrolPos>("patrol")
    .each(
       []
       (Action& act, const Position& pos, const PatrolPos& ppos)
       {
         if (glm::length(glm::vec2(pos.v - ppos.pos)) > ppos.patrolRadius)
         {
           act.action = move_towards(pos.v, ppos.pos);
         }
         else
         {
           static std::uniform_int_distribution<> dir(0, 3);
           static ActionType dirs[]
             {
               ActionType::MOVE_UP, ActionType::MOVE_DOWN,
               ActionType::MOVE_LEFT, ActionType::MOVE_RIGHT
             };
           act.action = dirs[dir(engine)];
         }
       });
  
  createReactor.operator()<Action, const Position>("move_to_enemy")
    .term<ClosestVisibleEnemy>(flecs::Wildcard)
    .each(
      []
      (flecs::entity e, Action& act, const Position& pos)
      {
        const auto enemy = e.target<ClosestVisibleEnemy>();
        if (e.has<ClosestVisibleAlly, NoVisibleEntity>() || !enemy.is_alive())
          return;

        act.action = move_towards(pos.v, enemy.get<Position>()->v);
      });
  
  createReactor.operator()<Action, const Position>("flee_from_enemy")
    .term<ClosestVisibleEnemy>(flecs::Wildcard)
    .each(
      []
      (flecs::entity e, Action& act, const Position& pos)
      {
        auto enemy = e.target<ClosestVisibleEnemy>();
        if (e.has<ClosestVisibleAlly, NoVisibleEntity>() || !enemy.is_alive())
          return;

        act.action = inverse_move(move_towards(pos.v, enemy.get<Position>()->v));
      });
  
  createReactor.operator()<Action&>("heal")
    .each(
      [](Action& act)
      {
        act.action = ActionType::REGEN;
      });
  
  createReactor.operator()<Action>("heal_ally")
    .each(
      [](Action& act)
      {
        act.action = ActionType::HEAL;
      });
  
  createReactor.operator()<Action, Position>("follow_player")
    .each(
      [playerq = world.query_builder<const Position>().term<IsPlayer>().build()]
      (Action& act, const Position& pos)
      {
        playerq.each([&](const Position& ppos)
          {
            if (glm::length(glm::vec2(pos.v) - glm::vec2(ppos.v)) > 2)
              act.action = move_towards(pos.v, ppos.v);
          });
      });

  return
    {
      world.pipeline<SimulateAi>()
        .term(flecs::System)
        .term<SimulateAi>()
          .cascade(flecs::DependsOn)
        .build()
        .add<SimulateAi>(),
      stateTransitionPhase
    };
}

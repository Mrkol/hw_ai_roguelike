#include "aiSystems.hpp"

#include <random>

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

  auto enemyNearEvent = world.entity("enemy_near");
  world.system<const ClosestVisibleEnemy>()
    .kind(eventsPhase)
    .term(enemyNearEvent)
    .each(
      [&tracker, enemyNearEvent]
      (flecs::entity e, const ClosestVisibleEnemy& closest)
      {
        if (closest.pos.has_value())
          tracker.handleEvent(enemyNearEvent, e);
      });

  auto hitpointsLowEvent = world.entity("hp_low");
  world.system<const Hitpoints, const HitpointsLowThreshold>()
    .kind(eventsPhase)
    .term(hitpointsLowEvent)
    .each(
      [&tracker, hitpointsLowEvent]
      (flecs::entity e, const Hitpoints& hp, const HitpointsLowThreshold& threshold)
      {
        if (hp.hitpoints < threshold.hitpoints)
          tracker.handleEvent(hitpointsLowEvent, e);
      });


  auto stateTransitionPhase = world.entity().add<SimulateAi>().depends_on(eventsPhase);
  auto stateReactionPhase = world.entity().add<SimulateAi>().depends_on(stateTransitionPhase);


  static std::default_random_engine engine;

  world.system<Action, const Position, const PatrolPos>()
    .kind(stateReactionPhase)
    .term(flecs::Wildcard, world.entity("patrol"))
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
  
  world.system<Action, const Position, const ClosestVisibleEnemy>()
    .kind(stateReactionPhase)
    .term(flecs::Wildcard, world.entity("move_to_enemy"))
     .each(
      []
       (Action& act, const Position& pos, const ClosestVisibleEnemy& epos)
       {
         if (epos.pos.has_value())
           act.action = move_towards(pos.v, *epos.pos);
       });
  
  world.system<Action, const Position, const ClosestVisibleEnemy>()
    .kind(stateReactionPhase)
    .term(flecs::Wildcard, world.entity("flee_from_enemy"))
     .each(
      []
       (Action& act, const Position& pos, const ClosestVisibleEnemy& epos)
       {
         if (epos.pos.has_value())
           act.action = inverse_move(move_towards(pos.v, *epos.pos));
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

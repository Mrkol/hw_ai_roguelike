#pragma once

#include <glm/glm.hpp>


enum class ActionType
{
  NOP = 0,
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_DOWN,
  MOVE_UP,
  REGEN,
  HEAL
};

struct Action
{
  ActionType action = ActionType::NOP;
};

inline ActionType move_towards(const glm::ivec2& from, const glm::ivec2& to)
{
  int deltaX = to.x - from.x;
  int deltaY = to.y - from.y;
  if (glm::abs(deltaX) > glm::abs(deltaY))
    return deltaX > 0 ? ActionType::MOVE_RIGHT : ActionType::MOVE_LEFT;
  return deltaY > 0 ? ActionType::MOVE_UP : ActionType::MOVE_DOWN;
}

inline ActionType inverse_move(ActionType move)
{
  return move == ActionType::MOVE_LEFT  ? ActionType::MOVE_RIGHT :
         move == ActionType::MOVE_RIGHT ? ActionType::MOVE_LEFT :
         move == ActionType::MOVE_UP    ? ActionType::MOVE_DOWN :
         move == ActionType::MOVE_DOWN  ? ActionType::MOVE_UP : move;
}

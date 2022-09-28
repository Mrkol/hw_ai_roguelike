#pragma once

#include <optional>

#include <glm/glm.hpp>


struct MovePos
{
  glm::ivec2 v;
};

struct Position
{
  glm::ivec2 v;
};

struct PatrolPos
{
  float patrolRadius = 4;
  glm::ivec2 pos;
};

struct ClosestVisibleEnemy
{
  float visibility = 6;
  std::optional<glm::ivec2> pos;
};

struct Hitpoints
{
  float hitpoints = 10.f;
};

struct HitpointsLowThreshold
{
  float hitpoints = 1.f;
};

struct HitpointsRegen
{
  float regenPerTurn = 10.f;
};

enum class ActionType
{
  NOP = 0,
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_DOWN,
  MOVE_UP,
  REGEN,
};

struct Action
{
  ActionType action = ActionType::NOP;
};

struct NumActions
{
  int numActions = 1;
  int curActions = 0;
};

struct MeleeDamage
{
  float damage = 2.f;
};

struct HealAmount
{
  float amount = 0.f;
};

struct PowerupAmount
{
  float amount = 0.f;
};

struct Color
{
  uint32_t color;
};

struct IsPlayer {};

struct Team
{
  int team = 0;
};
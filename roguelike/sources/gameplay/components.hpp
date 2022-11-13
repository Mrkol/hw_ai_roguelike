#pragma once

#include <vector>
#include <optional>
#include <variant>
#include <string>

#include <glm/glm.hpp>

#include <sprite.hpp>


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

struct Visibility
{
  float visibility = 6;
};

struct NoVisibleEntity {};
struct ClosestVisibleEnemy {};
struct ClosestVisibleAlly {};

struct Hitpoints
{
  float hitpoints = 10.f;
};

struct HitpointsThresholds
{
  float low = 1.f;
  float high = 10.f;
};

struct HitpointsRegen
{
  float regenPerTurn = 10.f;
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

struct Waypoint {};

struct PowerupAmount
{
  float amount = 0.f;
};

struct Color
{
  uint32_t color;
};

struct Sprite
{
  SpriteId id;
};

struct IsPlayer {};

struct Team
{
  int team = 0;
};

struct SmartMovement
{
  struct Summand
  {
    std::string dmap;
    float coefficient{1.f};
    std::string bbVariableCoefficient;
    float power{1};
  };

  std::vector<Summand> potential;
};

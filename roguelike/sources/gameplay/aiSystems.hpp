#pragma once

#include <flecs.h>

#include "../stateMachine.hpp"


struct SimulateAiInfo
{
  flecs::entity simulateAiPipieline;
  flecs::entity stateTransitionPhase;
};

SimulateAiInfo register_ai_systems(flecs::world& world, StateMachineTracker& tracker);

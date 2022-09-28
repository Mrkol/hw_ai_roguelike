#pragma once

#include <flecs.h>

#include "assert.hpp"
#include "util.hpp"


template<class Derived>
class Ecs
{
 public:
  Ecs()
    : defaultPipeline_{world_.get_pipeline()}
  {
    world_.set<flecs::Rest>({});
  }

  void poll()
  {
    world_.set_pipeline(defaultPipeline_);
    // TODO: time delta?
    world_.progress();
  }

  flecs::world& world() { return world_; }

 private:
  flecs::world world_;
  flecs::entity defaultPipeline_;
};

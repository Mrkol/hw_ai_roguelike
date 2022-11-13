#pragma once

#include "dungeon.hpp"
#include <experimental/mdspan>
#include <vector>
#include <function2/function2.hpp>


namespace dungeon::dmaps
{

static constexpr float INF = 1e6;

using DmapView = std::experimental::mdspan<float, std::experimental::extents<int, std::dynamic_extent, std::dynamic_extent>>;
struct Dmap
{
  std::vector<float> data;
  DmapView view;
  bool debugDraw{false};
};

Dmap make(DungeonView dungeon);
void clear(DmapView dmap);
using PotentialFuncSig = float(float) const;

struct PotentialHolder
{
  fu2::function<PotentialFuncSig> potential;
};

void generate(DmapView map, DungeonView dungeon, fu2::function_view<PotentialFuncSig> potential);

}

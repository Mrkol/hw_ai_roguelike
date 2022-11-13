#include "dmaps.hpp"
#include "gameplay/dungeon/dmaps.hpp"
#include <algorithm>
#include <queue>
#include <glm/glm.hpp>


namespace dungeon::dmaps
{

Dmap make(DungeonView dungeon)
{
  Dmap result{.data = std::vector<float>(dungeon.size())};
  result.view = DmapView{result.data.data(), dungeon.extents()};
  clear(result.view);
  return result;
}

void clear(DmapView dmap)
{
  std::fill_n(dmap.data_handle(), dmap.size(), INF);
}

void generate(DmapView map, DungeonView dungeon, fu2::function_view<PotentialFuncSig> potential)
{
  auto safeValueAt = [&map](glm::ivec2 v)
    {
      if (v.x < 0 || v.y < 0 || v.x > map.extent(1) || v.y > map.extent(0))
        return INF;
      return map(v.y, v.x);
    };

  enum class Visited : char { No, Yes };

  using Pair = std::pair<float, glm::ivec2>;

  struct Comp
  {
    bool operator()(const Pair& a, const Pair& b)
      { return a.first < b.first; }
  };


  std::priority_queue<Pair, std::vector<Pair>, Comp> queue(Comp{},
    [&]()
    {
      std::vector<Pair> container;
      container.reserve(dungeon.extent(0) * 4);
      return container;
    }());

  for (int y = 0; y < map.extent(0); ++y)
    for (int x = 0; x < map.extent(1); ++x)
    {
      if (map(y, x) == 0)
        queue.push({0, {x, y}});
    }

  while (!queue.empty())
  {
    auto[d, current] = queue.top();
    queue.pop();

    auto dist = safeValueAt(current);

    if (dist < d)
      continue;

    for (auto offset : std::array{glm::ivec2{0, 1}, glm::ivec2{0, -1}, glm::ivec2{1, 0}, glm::ivec2{-1, 0}})
    {
      auto neighbor = current + offset;

      // Don't try to relax paths into a wall
      if (dungeon(neighbor.y, neighbor.x) == Tile::Wall)
        continue;

      float neighbor_dist = dist + potential(dist);
      if (neighbor_dist < safeValueAt(neighbor))
      {
        map(neighbor.y, neighbor.x) = neighbor_dist;
        queue.push({neighbor_dist, neighbor});
      }
    }
  }
}

}

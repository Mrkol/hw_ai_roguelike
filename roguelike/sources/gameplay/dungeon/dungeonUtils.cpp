#include "dungeonUtils.hpp"
#include <vector>
#include <random>


namespace dungeon
{

glm::ivec2 find_walkable_tile(flecs::world& ecs)
{
  static auto dungeonDataQuery = ecs.query<const Dungeon>();
  static std::default_random_engine engine;

  glm::ivec2 res{0, 0};
  dungeonDataQuery.each([&](const Dungeon& dd)
  {
    // prebuild all walkable and get one of them
    std::vector<glm::ivec2> posList;

    for (int y = 0; y < dd.view.extent(0); ++y)
      for (int x = 0; x < dd.view.extent(1); ++x)
        if (dd.view(y, x) == Tile::Floor)
          posList.push_back(glm::ivec2{x, y});

    std::uniform_int_distribution<> distr(0, posList.size() - 1);
    res = posList[distr(engine)];
  });
  return res;
}

bool is_tile_walkable(const Dungeon& dd, glm::ivec2 pos)
{
  if (pos.x < 0 || pos.x >= dd.view.extent(1) ||
      pos.y < 0 || pos.y >= dd.view.extent(0))
    return false;
  return dd.view(pos.y, pos.x) == Tile::Floor;
}

Dungeon make_dungeon(int width, int height)
{
  Dungeon result
    {
      .data = std::vector<Tile>(width * height),
    };
  result.view = DungeonView(result.data.data(), width, height);
  return result;
}

}


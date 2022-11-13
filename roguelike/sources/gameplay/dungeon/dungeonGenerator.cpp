#include "dungeonGenerator.hpp"
#include "dungeonUtils.hpp"
#include <cstring>
#include <random>
#include <chrono>
#include <glm/glm.hpp>


namespace dungeon
{

void gen_drunk_dungeon(DungeonView view)
{
  std::memset(view.data_handle(), Tile::Wall, view.size());

  // generator
  unsigned seed = unsigned(std::chrono::system_clock::now().time_since_epoch().count() % INT_MAX);
  std::default_random_engine generator(seed);

  // distributions
  std::uniform_int_distribution<int> widthDist(1, view.extent(1) - 2);
  std::uniform_int_distribution<int> heightDist(1, view.extent(0) - 2);
  std::uniform_int_distribution<int> dirDist(0, 3);
  auto rndWd = [&]() { return widthDist(generator); };
  auto rndHt = [&]() { return heightDist(generator); };
  auto rndDir = [&]() { return dirDist(generator); };

  const int dirs[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

  constexpr std::size_t numIter = 4;
  constexpr std::size_t maxExcavations = 200;
  std::vector<glm::ivec2> startPos;
  for (std::size_t iter = 0; iter < numIter; ++iter)
  {
    // select random point on map
    int x = rndWd();
    int y = rndHt();
    startPos.push_back({int(x), int(y)});
    std::size_t numExcavations = 0;
    while (numExcavations < maxExcavations)
    {
      if (view(y, x) == Tile::Wall)
      {
        numExcavations++;
        view(y, x) = Tile::Floor;
      }
      // choose random dir
      std::size_t dir = rndDir(); // 0 - right, 1 - up, 2 - left, 3 - down
      int newX = std::min(std::max(x + dirs[dir][0], 1), view.extent(1) - 2);
      int newY = std::min(std::max(y + dirs[dir][1], 1), view.extent(0) - 2);
      x = std::size_t(newX);
      y = std::size_t(newY);
    }
  }

  auto distSq = [](auto& a, auto& b){ return glm::dot(glm::vec2(a - b), glm::vec2(a - b)); };

  // construct a path from start pos to all other start poses
  for (const glm::ivec2 &spos : startPos)
    for (const glm::ivec2 &epos : startPos)
    {
      glm::ivec2 pos = spos;
      while (distSq(pos, epos) > 0.f)
      {
        const glm::ivec2 delta = epos - pos;
        if (abs(delta.x) > abs(delta.y))
          pos.x += delta.x > 0 ? 1 : -1;
        else
          pos.y += delta.y > 0 ? 1 : -1;
        view(pos.y, pos.x) = Tile::Floor;
      }
    }
}

}

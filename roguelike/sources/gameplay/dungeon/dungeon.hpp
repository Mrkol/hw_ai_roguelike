#pragma once

#include <span>
#include <vector>
#include <experimental/mdspan>


namespace dungeon
{

enum Tile : char
{
  Wall = '#',
  Floor = ' '
};

using DungeonView = std::experimental::mdspan<Tile, std::experimental::extents<int, std::dynamic_extent, std::dynamic_extent>>;

struct Dungeon
{
  std::vector<Tile> data;
  DungeonView view;
};

}

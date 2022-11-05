#pragma once

#include <glm/glm.hpp>

#include "Allegro.hpp"
#include "Ecs.hpp"
#include "Game.hpp"
#include "stateMachine.hpp"


class Application
  : public Allegro<Application>
  , public Ecs<Application>
  , public Game<Application>
{
public:
  Application(int, char**)
  {
  }

  void drawGui()
  {
    Game::drawGui();
  }

  void draw()
  {
    float scale = camScale * 0.05f * (kWidth + kHeight) / 2.f;

    auto playerPos = Game::cameraPos();
    playerPos.y = -playerPos.y;
    auto worldToScreen =
      [playerPos, scale](glm::vec2 v)
      {
        v.y = -v.y;
        return glm::vec2{kWidth, kHeight}/2.f + (v - playerPos - glm::vec2{.5f, .5f}) * scale;
      };

    Game::draw(worldToScreen);
  }

  int run()
  {
    while (!exit_)
    {
      Allegro::poll();
      Ecs::poll();
      Allegro::draw();
    }

    return 0;
  }

  void close()
  {
    exit_ = true;
  }

public:
  float camScale = 1.f;

private:
  bool exit_ = false;
};

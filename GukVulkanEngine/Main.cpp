#include "Game.h"

int main()
{
    using namespace guk;

    auto game = std::make_unique<Game>();

    game->run();

    return 0;
}
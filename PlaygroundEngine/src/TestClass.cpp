module;

#include "PlaygroundEngine/Log.h"

module Test;

import std;

int PlaygroundEngine::Test::GetA()
{
    LOG_TRACE("test");
    
    std::cout << "test" << "\n";
    return a;
}

//
// RT64
//

#pragma once

#include <functional>

const char *GetHomeDirectory();

namespace apple {
    void dispatchOnMainThread(std::function<void()> func);
};
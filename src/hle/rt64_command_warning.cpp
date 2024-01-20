//
// RT64
//

#include "rt64_command_warning.h"

#include <cstdarg>

namespace RT64 {
    // CommandWarning

    CommandWarning CommandWarning::format(const char *msg, ...) {
        CommandWarning warning;
        va_list va1, va2;
        va_start(va1, msg);
        va_copy(va2, va1);
        int reqSize = vsnprintf(nullptr, 0, msg, va1);
        warning.message.resize(reqSize);
        if (reqSize > 0) {
            vsnprintf(warning.message.data(), reqSize, msg, va2);
        }

        va_end(va1);
        va_end(va2);
        return warning;
    }
};
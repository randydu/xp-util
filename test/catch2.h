#pragma once

#if defined(_MSC_VER)
// workaround of WinSDK issue (ref: https://developercommunity2.visualstudio.com/t/std:c17-generates-warning-compiling-Win/1249671?preview=true)
#pragma warning(disable: 5105)
#endif

#include <catch2/catch.hpp>
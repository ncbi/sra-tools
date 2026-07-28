#pragma once

#if defined( GCC_VERSION ) && GCC_VERSION <= 6
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif


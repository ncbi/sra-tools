
#if LINUX
#if GCC_VERSION <= 6
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif

#if WINDOWS
#include <filesystem>
namespace fs = std::filesystem;
#endif

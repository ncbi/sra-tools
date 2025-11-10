#define TOOL_NAME_COMMON ""
#define TOOL_ARGS_COMMON TOOL_ARGS ( \
    TOOL_ARG("ngc", "", true), \
    TOOL_ARG("cart", "", true), \
    TOOL_ARG("perm", "", true), \
    TOOL_ARG("help", "h?", false), \
    TOOL_ARG("version", "V", false), \
    TOOL_ARG("log-level", "L", true), \
    TOOL_ARG("verbose", "v", false), \
    TOOL_ARG("quiet", "q", false), \
    TOOL_ARG("option-file", "", true), \
    TOOL_ARG("debug", "+", true), \
    TOOL_ARG("no-user-settings", "", false), \
    TOOL_ARG("ncbi_error_report", "", true), \
    TOOL_ARG("location", "", true), \
    TOOL_ARG(0, 0, 0))

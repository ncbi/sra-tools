#define TOOL_NAME_COMMON ""
#define TOOL_ARGS_COMMON TOOL_ARGS ( \
    TOOL_ARG("ngc", "", true, TOOL_HELP("PATH to ngc file.", 0)), \
    TOOL_ARG("cart", "", true, TOOL_HELP("To read a kart file.", 0)), \
    TOOL_ARG("perm", "", true, TOOL_HELP("PATH to jwt cart file.", 0)), \
    TOOL_ARG("help", "h?", false, TOOL_HELP("Output a brief explanation for the program.", 0)), \
    TOOL_ARG("version", "V", false, TOOL_HELP("Display the version of the program then quit.", 0)), \
    TOOL_ARG("log-level", "L", true, TOOL_HELP("Logging level as number or enum string.", 0)), \
    TOOL_ARG("verbose", "v", false, TOOL_HELP("Increase the verbosity of the program status messages.", "Use multiple times for more verbosity.", "Negates quiet.", 0)), \
    TOOL_ARG("quiet", "q", false, TOOL_HELP("Turn off all status messages for the program.", "Negates verbose.", 0)), \
    TOOL_ARG("option-file", "", true, TOOL_HELP("Read more options from the file.", 0)), \
    TOOL_ARG("debug", "+", true, TOOL_HELP("Turn on debug output for module.", "All flags if not specified.", 0)), \
    TOOL_ARG("no-user-settings", "", false, TOOL_HELP("Turn off user-specific configuration.", 0)), \
    TOOL_ARG("ncbi_error_report", "", true, TOOL_HELP("Control program execution environment report generation (if implemented).", "One of (never|error|always). Default is error.", 0)), \
    TOOL_ARG("location", "", true, TOOL_HELP("This is used by sratools during source data lookup. It is not passed to the driven tool.", 0)), \
    TOOL_ARG(0, 0, 0, TOOL_HELP(0)))

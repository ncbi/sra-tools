# vdb-config

## Overview

--prefetch-to-user-repo          Prefetch downloads to public user
- ****: repository when it is set (default).

--proxy <uri[:port]>             Set HTTP proxy server configuration.
--proxy-disable <yes | no>       Enable/disable using HTTP proxy.

--cfg-dir <path>                 Set directory to load configuration.

--root                           Enforce configuration update while being
- ****: run by superuser.

-h|--help                        Output brief explanation for the program.
-V|--version                     Display the version of the program then
- ****: quit.
-L|--log-level <level>           Logging level as number or enum string. One
- ****: of (fatal|sys|int|err|warn|info|debug) or
- ****: (0-6) Current/default is warn.
-v|--verbose                     Increase the verbosity of the program
- ****: status messages. Use multiple times for more
- ****: verbosity. Negates quiet.
-q|--quiet                       Turn off all status messages for the
- ****: program. Negated by verbose.
--option-file <file>             Read more options and parameters from the
- ****: file.

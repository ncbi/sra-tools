# bam-load

## Overview

--threads                        number of threads (3 or greater; can be 0,
- ****: means default: 8)
--extra-logging                  extra_logging
--min-batch-size <count>         Set the minimum batch size for spot
- ****: assembly (default: 10,000,000 spots)
--telemetry <file-name>          Path and Name of the telemetry file.
-z|--xml-log <logfile>           Produce XML-formatted log file.

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

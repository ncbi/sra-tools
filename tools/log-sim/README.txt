======================================================
    log-sim ... a log simulator
======================================================

A tool to simulate the log-output of nginx/apache.

log-sim has 4 modes of operation:

1)  replay-mode ( default, no option necessary )
2)  random-mode ( --function random )
3)  presort-mode ( --function presort )
4)  analyze-mode ( --function analyze )

In 3 of the modes ( the exception is random-mode ) the tool expects
input. The input is an existing log-file. The input can be given as
file-name ( --input /path/to/input-file ) or if the '--input' option
is not found, it reads from STDIN.

In all of the modes the tool produces its output either to STDOUT or
to a given output-file ( --output /path/to/output.txt )

--------------------------------------------------------------------
    replay-mode
--------------------------------------------------------------------

examples:

replay the uncompressed log-file, output to STDOUT:
$log-sim --input /path/to/log-file.txt

replay from STDIN, output to STDOUT:
( because zcat runs in its own process - this can be faster )
$zcat /path/to/file.gz | log-sim > output.txt

replay the given, gzipped log-file, output to file:
$log-sim --input /path/to/file.gz --gunzip --output output.txt

In replay-mode the tool tries to replicate the 'speed' of the
input-file. That means if 2 lines are 1 second apart, the tool waits
( sleeps ) for 1 second between the 2 lines. This speed can be changed
by the '--speed' commandline-switch. This option is a float-value,
which will be multiplied with the wait-time. A factor of 0.5 will
double the replay-speed relative to the input-file, and a factor
of 2.0 will slow the replay-speed down to half the original speed.

examples:

replay the log-file, output to STDOUT, double the speed:
$log-sim --input /path/to/log-file.txt --speed 0.5

replay the log-file, output to STDOUT, half the speed:
$log-sim --input /path/to/log-file.txt --speed 2.0

--------------------------------------------------------------------
    random-mode
--------------------------------------------------------------------

In random-mode the tool produces (pseudo)random events. It will do
this at maximum speed by default, but the speed can be made more
realistic by using the ( --min-wait and --max-wait options ).

examples:

generate random events ( max speed ) to STDOUT
$log-sim --function random > events.txt

generate random events ( with (pseudo)random wait-times ) to file
$log-sim --function random --min-wait 10 --max-wait 2000 --output events.txt

--------------------------------------------------------------------
    presort-mode
--------------------------------------------------------------------

Unfortunately not all recorded log-files have the events in
sequential order. To make it easier to sort the lines, the presort-mode
creates output that has the time ( just the time - not the date ) as
the first field and the original line as the second field in its 
tab-separated output. This output can then be 'piped' to sort, and
from the output of sort we can cut out the now unneccessary first field.
The tool now appears twice in the chain of commands.

example:

$zcat /path/to/file.gz | log-sim --function presort | sort | cut -f 2 | log-sim

--------------------------------------------------------------------
    analyze-mode
--------------------------------------------------------------------

This is just a helper-mode which reports how many lines are in each
second of the input.

example:

$zcat /path/to/file.gz | log-sim --function analyze > report.txt

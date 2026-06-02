# var-expand

## Summary

For each pair (key, variation spec) in input produces the expanded variation spec

## Options

| Option | Description |
|---|---|
| --algorithm &lt;value&gt; | the algorithm to use for searching. "sw"<br>means Smith-Waterman. "ra" means Rolling<br>bulldozer algorithm |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

## Input

the stream of lines in the format: &lt;key&gt; &lt;tab&gt; &lt;input variation&gt;

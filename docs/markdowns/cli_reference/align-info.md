# align-info

## Summary

Print database alignment information

## Usage

```text
align-info [options] <db-path>
```

## Options

| Option | Description |
|---|---|
| -a\|--all | print all information |
| -r\|--ref | print refseq information [default] |
| -b\|--bam | print bam header (if present) |
| -Q\|--qual | print quality statistics (if present) |
| -H\|--headers | print headers for output blocks |
| --ngc &lt;path&gt; | path to ngc file |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

## Parameters

db-path                          Path to the database

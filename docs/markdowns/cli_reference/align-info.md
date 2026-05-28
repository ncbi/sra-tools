# align-info

## Usage

```sh
align-info [options] <db-path>
```

## Summary

Print database alignment information

## Parameters

- **db-path**: Path to the database

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
| -V\|--version | Display the version of the program then&lt;br&gt;quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One&lt;br&gt;of (fatal\|sys\|int\|err\|warn\|info\|debug) or&lt;br&gt;(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program&lt;br&gt;status messages. Use multiple times for more&lt;br&gt;verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the&lt;br&gt;program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the&lt;br&gt;file. |

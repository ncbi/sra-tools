# general-loader

## Summary

Populate a VDB database from standard input

## Usage

```text
general-loader [options]
```

## Options

| Option | Description |
|---|---|
| -I\|--include &lt;path(s)&gt; | Additional directories to search for schema<br>include files. Can specify multiple paths<br>separated by ':'. |
| -S\|--schema &lt;path(s)&gt; | Schema file to use. Can specify multiple<br>files separated by ':'. |
| -T\|--target &lt;path&gt; | Database file to create. Overrides any<br>remote path specifications coming from the<br>input stream |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

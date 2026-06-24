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
| `-I`\|`--include <path(s)>` | Additional directories to search for schema include files. Can specify multiple paths separated by ':'. |
| `-S`\|`--schema <path(s)>` | Schema file to use. Can specify multiple files separated by ':'. |
| `-T`\|`--target <path>` | Database file to create. Overrides any remote path specifications coming from the input stream |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

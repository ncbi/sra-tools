# align-cache

## Summary

Create a cache file for given database < src-db-path >
PRIMARY_ALIGNMENT table and save it into < new-cache-db-path >

## Usage

```text
align-cache [options] <src-db-path> <new-cache-db-path>
```

## Options

| Option | Description |
|---|---|
| `-t`\|`--threshold <value>` | cache PRIMARY_ALIGNMENT records with difference between values of ALIGN_ID and MATE_ALIGN_ID >= the value of 'threshold' option |
| `--cursor-cache <value in MB>` | the size of the read cursor in Megabytes |
| `--min-cache-count <count>` | if the number of primary alignment ids in the src db selected for caching is less than < min-cache-count >, the cache db will not be created at all |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Parameters

- `src-db-path`: Path to the database
- `new-cache-db-path`: Path to the new cache database to be created

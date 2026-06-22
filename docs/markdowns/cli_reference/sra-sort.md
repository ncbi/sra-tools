# sra-sort

## Usage

```text
sra-sort [options] src-object dst-object
       sra-sort [options] src-object [src-object...] dst-dir
```

## Options

| Option | Description |
|---|---|
| `-i`\|`--ignore-failure` | ignore failure when sorting multiple objects i.e. continue in spite of previous errors |
| `-f`\|`--force` | force overwrite of existing destination |
| `--mem-limit <bytes>` | sets limit on dynamic memory usage |
| `--map-file-bsize <cache-size>` | sets id map-file cache size |
| `--max-idx-ids <num-ids>` | sets number of join-index ids to process at a time |
| `--max-ref-idx-ids <num-ids>` | sets number of join-index ids to process within REFERENCE table |
| `--max-large-idx-ids <num-ids>` | sets number of rows to process with large columns |
| `--tempdir <path-to-tmp>` | sets specific directory to use for temporary files |
| `--mmapdir <path-to-mmaps>` | sets specific directory to use for memory-mapped buffers |
| `--unsorted-old-new` | write old=>new index in unsorted order |
| `--column-md5` | generate md5sum compatible checksum files for each column [default] |
| `--no-column-checksum` | disable generation of column checksums |
| `--blob-crc32` | generate CRC32 checksums for each blob [default] |
| `--blob-md5` | generate MD5 checksums for each blob |
| `--no-blob-checksum` | disable generation of blob checksums |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

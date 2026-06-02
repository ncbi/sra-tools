# vdb-copy

## Summary

Copies and row ranges from an SRA data file into SRA archive.

## Usage

```text
vdb-copy <src_path> <dst_path> [options]
```

## Options

| Option | Description |
|---|---|
| -T\|--table &lt;table&gt; | table-name |
| -R\|--rows &lt;rows&gt; | set of rows to be copied(default all) |
| -S\|--schema &lt;schema&gt; | schema-name |
| -a\|--without_accession | without accession-test |
| -r\|--ignore_reject | ignore SRA_FILTER_REJECT values |
| -e\|--ignore_redact | ignore SRA_FILTER_REDACTED values |
| -k\|--kfg_path | use this path to find the file vdb-copy.kfg |
| -m\|--show_matching | show type-matching results |
| -p\|--show_progress | show progress in percent while copying |
| -i\|--ignore_incompatible_columns | ignore incompatible columns |
| -n\|--reindex | reindex columns after copy |
| -w\|--show_redact | show redaction-process |
| -x\|--exclude_columns | exclude these columns from copy |
| -t\|--show_meta | show metadata-copy-process |
| -f\|--force | forces an existing target to be overwritten |
| -u\|--unlock | forces locked target to be unlocked |
| -d\|--md5mode | MD5-mode def.: auto, '1'...forced ON,<br>'0'...forced OFF) |
| -b\|--blob_checksum | Blob-checksum def.: auto, '1'...CRC32,<br>'M'...MD5, '0'...OFF) |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

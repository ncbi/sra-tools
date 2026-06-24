# vdb-validate

## Summary

Validate the contents of an SRA file. Optionally performs referential integrity.

## Usage

```text
vdb-validate [options] path [ path... ]

  Examine directories, files and VDB objects,
  reporting any problems that can be detected.

Components md5s are always checked if present.
```

## Options

| Option | Description |
|---|---|
| `-B`\|`--BLOB-CRC <yes `\|` no>` | Check blobs CRC32 (default: no) |
| `-I`\|`--REFERENTIAL-INTEGRITY <yes `\|` no>` | Check data referential integrity for databases (default: yes) |
| `-C`\|`--CONSISTENCY-CHECK <yes `\|` no>` | Deeply check data consistency for tables (default: no) |
| `-x`\|`--exhaustive` | Continue checking object for all possible errors (default: false) |
| `--sdc:rows <rows>` | Specify maximum amount of secondary alignment table rows to look at before saying accession is good, default 100000. Specifying will iterate the whole table. Can be in percent (e.g. 5%) |
| `--sdc:seq-rows <rows>` | Specify maximum amount of sequence table rows to look at before saying accession is good, default 100000. Specifying will iterate the whole table. Can be in percent (e.g. 5%) |
| `--sdc:plen_thold <threshold>` | Specify threshold for amount of secondary alignment which are shorter (hard-clipped) than corresponding primaries, default 1%. |
| `--ngc <path>` | path to ngc file |
| `--check-redact` | check if redaction of bases has been correctly performed (default: false) |
| `--require-blob-checksums` | Require blob checksums (default: no) |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

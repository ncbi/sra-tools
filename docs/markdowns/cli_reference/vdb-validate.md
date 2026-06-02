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
| -B\|--BLOB-CRC &lt;yes \| no&gt; | Check blobs CRC32 (default: no) |
| -I\|--REFERENTIAL-INTEGRITY &lt;yes \| no&gt; | Check data referential integrity for<br>databases (default: yes) |
| -C\|--CONSISTENCY-CHECK &lt;yes \| no&gt; | Deeply check data consistency for tables<br>(default: no) |
| -x\|--exhaustive | Continue checking object for all possible<br>errors (default: false) |
| --sdc:rows &lt;rows&gt; | Specify maximum amount of secondary<br>alignment table rows to look at before<br>saying accession is good, default 100000.<br>Specifying will iterate the whole table.<br>Can be in percent (e.g. 5%) |
| --sdc:seq-rows &lt;rows&gt; | Specify maximum amount of sequence table<br>rows to look at before saying accession is<br>good, default 100000. Specifying will<br>iterate the whole table. Can be in percent<br>(e.g. 5%) |
| --sdc:plen_thold &lt;threshold&gt; | Specify threshold for amount of secondary<br>alignment which are shorter (hard-clipped)<br>than corresponding primaries, default 1%. |
| --ngc &lt;path&gt; | path to ngc file |
| --check-redact | check if redaction of bases has been<br>correctly performed (default: false) |
| --require-blob-checksums | Require blob checksums (default: no) |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

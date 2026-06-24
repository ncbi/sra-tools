# vdb-dump

## Summary

Examine the data contents of an SRA file.

## Usage

```text
vdb-dump <path> [<path> ...] [options]
```

## Options

| Option | Description |
|---|---|
| `-I`\|`--row_id_on` | print row id |
| `-l`\|`--line_feed <line_feed>` | line-feed's inbetween rows |
| `-N`\|`--colname_off` | do not print column-names |
| `-X`\|`--in_hex` | print numbers in hex |
| `-T`\|`--table <table>` | table-name |
| `-R`\|`--rows <rows>` | rows (default all) |
| `-C`\|`--columns <columns>` | columns (default all) |
| `-S`\|`--schema <schema>` | schema-name |
| `-A`\|`--schema_dump` | dumps the schema |
| `-E`\|`--table_enum` | enumerates tables |
| `-O`\|`--column_enum` | enumerates columns in extended form |
| `-o`\|`--column_enum_short` | enumerates columns in short form |
| `-D`\|`--dna_bases` | force dna-bases if column fits pattern |
| `-M`\|`--max_length <max_length>` | limits line length |
| `-i`\|`--indent_width <indent_width>` | indents the line |
| `-f`\|`--format <format>` | One of csv, xml, json, piped, tab, fastq, fastq1, fasta, fasta1, fasta2, qual, or qual1. See Output Formats. |
| `-r`\|`--id_range` | prints id-range |
| `-n`\|`--without_sra` | without sra-type-translation |
| `-x`\|`--exclude <columns>` | exclude these columns |
| `-b`\|`--boolean <1 or T>` | defines how boolean's are printed (1,T) |
| `-j`\|`--obj_version` | request vdb-version |
| `--obj_timestamp` | request object modification date |
| `-y`\|`--obj_type` | report type of object |
| `-u`\|`--numelem` | print only element-count |
| `-U`\|`--numelemsum` | sum element-count |
| `--phys-blobs` | show physical blobs |
| `--vdb-blobs` | show VDB-blobs |
| `--phys` | enumerate physical columns |
| `--readable` | enumerate readable columns |
| `--idx-report` | enumerate all available index |
| `--idx-range <idx-name>` | enumerate values and row-ranges of one index |
| `--cur-cache <size>` | size of cursor cache |
| `--output-file <filename>` | write output to this file |
| `--output-path <path>` | write output to this directory |
| `--gzip` | compress output using gzip |
| `--bzip2` | compress output using bzip2 |
| `--output-buffer-size <size>` | size of output-buffer, 0...none |
| `--disable-multithreading` | disable multithreading |
| `--info` | print info about run |
| `--spotgroups` | show spotgroups |
| `--merge-ranges` | merge and sort row-ranges |
| `--spread` | show spread of integer values |
| `-a`\|`--append` | append to output-file, if output-file used |
| `--ngc <path>` | path to ngc file |
| `--view <view>` | view-name |
| `--inspect` | inspect data usage inside object |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Output Formats

| Format | Description |
|---|---|
| `csv` | comma separated values on one line |
| `xml` | xml-style without complete xml-frame |
| `json` | json-style |
| `piped` | 1 line per cell: row-id, column-name: value |
| `tab` | 1 line per row: tab-separated values only |
| `fastq` | FASTQ( 4 lines ) for each row |
| `fastq1` | FASTQ( 4 lines ) for each fragment |
| `fasta` | FASTA( 2 lines ) for each fragment if possible |
| `fasta1` | one FASTA-record for the whole accession (REFSEQ) |
| `fasta2` | one FASTA-record for each REFERENCE in cSRA |
| `qual` | QUAL( 2 lines ) for each row |
| `qual1` | QUAL( 2 lines ) for each fragment if possible |

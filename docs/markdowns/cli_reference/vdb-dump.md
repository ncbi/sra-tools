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
| -I\|--row_id_on | print row id |
| -l\|--line_feed &lt;line_feed&gt; | line-feed's inbetween rows |
| -N\|--colname_off | do not print column-names |
| -X\|--in_hex | print numbers in hex |
| -T\|--table &lt;table&gt; | table-name |
| -R\|--rows &lt;rows&gt; | rows (default all) |
| -C\|--columns &lt;columns&gt; | columns (default all) |
| -S\|--schema &lt;schema&gt; | schema-name |
| -A\|--schema_dump | dumps the schema |
| -E\|--table_enum | enumerates tables |
| -O\|--column_enum | enumerates columns in extended form |
| -o\|--column_enum_short | enumerates columns in short form |
| -D\|--dna_bases | force dna-bases if column fits pattern |
| -M\|--max_length &lt;max_length&gt; | limits line length |
| -i\|--indent_width &lt;indent_width&gt; | indents the line |
| -f\|--format &lt;format&gt; | output format:<br>csv ..... comma separated values on one line<br>xml ..... xml-style without complete xml-frame<br>json .... json-style<br>piped ... 1 line per cell: row-id, column-name: value<br>tab ..... 1 line per row: tab-separated values only<br>fastq ... FASTQ( 4 lines ) for each row<br>fastq1 .. FASTQ( 4 lines ) for each fragment<br>fasta ... FASTA( 2 lines ) for each fragment if possible<br>fasta1 .. one FASTA-record for the whole accession (REFSEQ)<br>fasta2 .. one FASTA-record for each REFERENCE in cSRA<br>qual .... QUAL( 2 lines ) for each row<br>qual1 ... QUAL( 2 lines ) for each fragment if possible |
| -r\|--id_range | prints id-range |
| -n\|--without_sra | without sra-type-translation |
| -x\|--exclude &lt;columns&gt; | exclude these columns |
| -b\|--boolean &lt;1 or T&gt; | defines how boolean's are printed (1,T) |
| -j\|--obj_version | request vdb-version |
| --obj_timestamp | request object modification date |
| -y\|--obj_type | report type of object |
| -u\|--numelem | print only element-count |
| -U\|--numelemsum | sum element-count |
| --phys-blobs | show physical blobs |
| --vdb-blobs | show VDB-blobs |
| --phys | enumerate physical columns |
| --readable | enumerate readable columns |
| --idx-report | enumerate all available index |
| --idx-range &lt;idx-name&gt; | enumerate values and row-ranges of one index |
| --cur-cache &lt;size&gt; | size of cursor cache |
| --output-file &lt;filename&gt; | write output to this file |
| --output-path &lt;path&gt; | write output to this directory |
| --gzip | compress output using gzip |
| --bzip2 | compress output using bzip2 |
| --output-buffer-size &lt;size&gt; | size of output-buffer, 0...none |
| --disable-multithreading | disable multithreading |
| --info | print info about run |
| --spotgroups | show spotgroups |
| --merge-ranges | merge and sort row-ranges |
| --spread | show spread of integer values |
| -a\|--append | append to output-file, if output-file used |
| --ngc &lt;path&gt; | path to ngc file |
| --view &lt;view&gt; | view-name |
| --inspect | inspect data usage inside object |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

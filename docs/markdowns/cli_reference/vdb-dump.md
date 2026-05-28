# vdb-dump

## Usage

```sh
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
| -f\|--format &lt;format&gt; | output format:&lt;br&gt;csv ..... comma separated values on one line&lt;br&gt;xml ..... xml-style without complete xml-frame&lt;br&gt;json .... json-style&lt;br&gt;piped ... 1 line per cell: row-id, column-name: value&lt;br&gt;tab ..... 1 line per row: tab-separated values only&lt;br&gt;fastq ... FASTQ( 4 lines ) for each row&lt;br&gt;fastq1 .. FASTQ( 4 lines ) for each fragment&lt;br&gt;fasta ... FASTA( 2 lines ) for each fragment if possible&lt;br&gt;fasta1 .. one FASTA-record for the whole accession (REFSEQ)&lt;br&gt;fasta2 .. one FASTA-record for each REFERENCE in cSRA&lt;br&gt;qual .... QUAL( 2 lines ) for each row&lt;br&gt;qual1 ... QUAL( 2 lines ) for each fragment if possible |
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
| -V\|--version | Display the version of the program then&lt;br&gt;quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One&lt;br&gt;of (fatal\|sys\|int\|err\|warn\|info\|debug) or&lt;br&gt;(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program&lt;br&gt;status messages. Use multiple times for more&lt;br&gt;verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the&lt;br&gt;program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the&lt;br&gt;file. |

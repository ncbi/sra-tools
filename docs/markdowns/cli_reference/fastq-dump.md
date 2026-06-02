# fastq-dump

## Usage

```text
fastq-dump [options] <path> [<path>...]
fastq-dump [options] <accession>
```

## Options

| Option | Description |
|---|---|
| **INPUT** |  |
| -A\|--accession &lt;accession&gt; | Replaces accession derived from &lt;path&gt; in<br>filename(s) and deflines (only for single<br>table dump) |
| --table &lt;table-name&gt; | Table name within cSRA object, default is<br>"SEQUENCE" |
| **PROCESSING** |  |
| **Read Splitting** | Sequence data may be used in raw form or<br>split into individual reads |
| --split-spot | Split spots into individual reads |
| **Full Spot Filters** | Applied to the full spot independently<br>of --split-spot |
| -N\|--minSpotId &lt;rowid&gt; | Minimum spot id |
| -X\|--maxSpotId &lt;rowid&gt; | Maximum spot id |
| --spot-groups &lt;[list]&gt; | Filter by SPOT_GROUP (member): name[,...] |
| -W\|--clip | Remove adapter sequences from reads |
| **Common Filters** | Applied to spots when --split-spot is not<br>set, otherwise - to individual reads |
| -M\|--minReadLen &lt;len&gt; | Filter by sequence length &gt;= &lt;len&gt; |
| -R\|--read-filter &lt;[filter]&gt; | Split into files by READ_FILTER value<br>optionally filter by value:<br>pass\|reject\|criteria\|redacted |
| -E\|--qual-filter | Filter used in early 1000 Genomes data: no<br>sequences starting or ending with &gt;= 10N |
| --qual-filter-1 | Filter used in current 1000 Genomes data |
| **Filters based on alignments** | Filters are active when alignment<br>data are present |
| --aligned | Dump only aligned sequences |
| --unaligned | Dump only unaligned sequences |
| --aligned-region &lt;name[:from-to]&gt; | Filter by position on genome. Name can<br>either be accession.version (ex:<br>NC_000001.10) or file specific name (ex:<br>"chr1" or "1"). "from" and "to" are 1-based<br>coordinates |
| --matepair-distance &lt;from-to\|unknown&gt; | Filter by distance between matepairs.<br>Use "unknown" to find matepairs split<br>between the references. Use from-to to limit<br>matepair distance on the same reference |
| **Filters for individual reads** | Applied only with --split-spot set |
| --skip-technical | Dump only biological reads |
| **OUTPUT** |  |
| -O\|--outdir &lt;path&gt; | Output directory, default is working<br>directory '.' ) |
| -Z\|--stdout | Output to stdout, all split data become<br>joined into single stream |
| --gzip | Compress output using gzip: deprecated, not<br>recommended |
| --bzip2 | Compress output using bzip2: deprecated,<br>not recommended |
| **Multiple File Options** | Setting these options will produce more<br>than 1 file, each of which will be suffixed<br>according to splitting criteria. |
| --split-files | Write reads into separate files. Read<br>number will be suffixed to the file name.<br>NOTE! The `--split-3` option is recommended.<br>In cases where not all spots have the same<br>number of reads, this option will produce<br>files that WILL CAUSE ERRORS in most programs<br>which process split pair fastq files. |
| --split-3 | 3-way splitting for mate-pairs. For each<br>spot, if there are two biological reads<br>satisfying filter conditions, the first is<br>placed in the `*_1.fastq` file, and the<br>second is placed in the `*_2.fastq` file. If<br>there is only one biological read<br>satisfying the filter conditions, it is<br>placed in the `*.fastq` file.All other<br>reads in the spot are ignored. |
| -G\|--spot-group | Split into files by SPOT_GROUP (member name) |
| -R\|--read-filter &lt;[filter]&gt; | Split into files by READ_FILTER value<br>optionally filter by value:<br>pass\|reject\|criteria\|redacted |
| -T\|--group-in-dirs | Split into subdirectories instead of files |
| -K\|--keep-empty-files | Do not delete empty files |
| **FORMATTING** |  |
| **Sequence** |  |
| -C\|--dumpcs &lt;[cskey]&gt; | Formats sequence using color space (default<br>for SOLiD),"cskey" may be specified for<br>translation |
| -B\|--dumpbase | Formats sequence using base space (default<br>for other than SOLiD). |
| **Quality** |  |
| -Q\|--offset &lt;integer&gt; | Offset to use for quality conversion,<br>default is 33 |
| --fasta &lt;[line width]&gt; | FASTA only, no qualities, optional line<br>wrap width (set to zero for no wrapping) |
| --suppress-qual-for-cskey | suppress quality-value for cskey |
| **Defline** |  |
| -F\|--origfmt | Defline contains only original sequence name |
| -I\|--readids | Append read id after spot id as<br>'accession.spot.readid' on defline |
| --helicos | Helicos style defline |
| --defline-seq &lt;fmt&gt; | Defline format specification for sequence. |
| --defline-qual &lt;fmt&gt; | Defline format specification for quality.<br>&lt;fmt&gt; is string of characters and/or<br>variables. The variables can be one of: $ac<br>- accession, $si spot id, $sn spot<br>name, $sg spot group (barcode), $sl spot<br>length in bases, $ri read number, $rn<br>read name, $rl read length in bases. '[]'<br>could be used for an optional output: if<br>all vars in [] yield empty values whole<br>group is not printed. Empty value is empty<br>string or for numeric variables. Ex:<br>@$sn[_$rn]/$ri '_$rn' is omitted if name<br>is empty |
| **Other** |  |
| --ngc &lt;path&gt; | &lt;path&gt; to ngc file |
| --disable-multithreading | disable multithreading |
| -h\|--help | Output brief explanation of program usage |
| -V\|--version | Display the version of the program |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string One<br>of (fatal\|sys\|int\|err\|warn\|info) or (0-5)<br>Current/default is warn |
| -v\|--verbose | Increase the verbosity level of the program<br>Use multiple times for more verbosity |
| --ncbi_error_report | Control program execution environment<br>report generation (if implemented). One of<br>(never\|error\|always). Default is error |
| --legacy-report | use legacy style 'Written spots' for tool |

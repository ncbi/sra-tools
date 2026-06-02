# sra-sort-cg

## Usage

```text
sra-sort-cg [options] src-object dst-object
       sra-sort-cg [options] src-object [src-object...] dst-dir
```

## Options

| Option | Description |
|---|---|
| -i\|--ignore-failure | ignore failure when sorting multiple objects<br>i.e. continue in spite of previous errors |
| -f\|--force | force overwrite of existing destination |
| --mem-limit &lt;bytes&gt; | sets limit on dynamic memory usage |
| --map-file-bsize &lt;cache-size&gt; | sets id map-file cache size |
| --max-idx-ids &lt;num-ids&gt; | sets number of join-index ids to process at<br>a time |
| --max-ref-idx-ids &lt;num-ids&gt; | sets number of join-index ids to process<br>within REFERENCE table |
| --max-large-idx-ids &lt;num-ids&gt; | sets number of rows to process with large<br>columns |
| --tempdir &lt;path-to-tmp&gt; | sets specific directory to use for<br>temporary files |
| --mmapdir &lt;path-to-mmaps&gt; | sets specific directory to use for<br>memory-mapped buffers |
| --unsorted-old-new | write old=&gt;new index in unsorted order |
| --column-md5 | generate md5sum compatible checksum files<br>for each column [default] |
| --no-column-checksum | disable generation of column checksums |
| --blob-crc32 | generate CRC32 checksums for each blob<br>[default] |
| --blob-md5 | generate MD5 checksums for each blob |
| --no-blob-checksum | disable generation of blob checksums |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

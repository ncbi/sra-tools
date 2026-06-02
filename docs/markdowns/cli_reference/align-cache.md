# align-cache

## Summary

Create a cache file for given database &lt;src-db-path&gt;
PRIMARY_ALIGNMENT table and save it into &lt;new-cache-db-path&gt;

## Usage

```text
align-cache [options] <src-db-path> <new-cache-db-path>
```

## Options

| Option | Description |
|---|---|
| -t\|--threshold &lt;value&gt; | cache PRIMARY_ALIGNMENT records with<br>difference between values of ALIGN_ID and<br>MATE_ALIGN_ID &gt;= the value of 'threshold'<br>option |
| --cursor-cache &lt;value in MB&gt; | the size of the read cursor in Megabytes |
| --min-cache-count &lt;count&gt; | if the number of primary alignment ids in<br>the src db selected for caching is less<br>than &lt;min-cache-count&gt;, the cache db will<br>not be created at all |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

## Parameters

- `src-db-path`: Path to the database
- `new-cache-db-path`: Path to the new cache database to be created

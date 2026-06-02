# sharq

## Summary

SharQ

## Usage

```text
sharq [OPTIONS] [files...]
```

## Options

| Option | Description |
|---|---|
| -h,--help | Print this help message and exit |
| -V,--version | Display program version information and exit<br>--print-deflines |
| --output TEXT | Output archive path |
| --platform TEXT | Optional platform |
| --readTypes TEXT | file read types &lt;B\|T(s)&gt; |
| --useAndDiscardNames | Excludes: --name-column<br>Discard file names |
| --allowEarlyFileEnd | Complete load at early end of one of the files<br>--sa,--spot-assembly |
| --help_errors,--help-errors | Print error codes and descriptions |
| --name-column | TEXT:{NONE,NAME,RAW_NAME}=NAME Excludes: --useAndDiscardNames<br>Database name for NAME column |
| -q,--quality INT:{0,33,64} | Interpretation of ascii quality |
| --max-spots UINT:POSITIVE | Maximum spot number for non spot-assembly mode (default: 1,200,000,000) |
| --digest{500000} | Report summary of input data (set optional value to indicate the number of spots to analyze) |
| --threads | UINT:INT in [1 - 256]=24<br>Max number of threads to use (set optional value to indicate the number of threads to use |
| -t,--telemetry TEXT | Telemetry report file |
| --read1PairFiles TEXT | Read 1 files |
| --read2PairFiles TEXT | Read 2 files |
| --read3PairFiles TEXT | Read 3 files |
| --read4PairFiles TEXT | Read 4 files |
| --max-err-count | UINT:UINT in [0 - 4,294,967,295]=100<br>Maximum number of errors allowed |
| --hot-reads-threshold UINT | Hot reads threshold |
| --experiment TEXT | Read structure description |
| **[Option Group: Debugging options]** | Options: |
| --no-timestamp | No time stamp in debug mode |
| --log-level | TEXT:{trace,debug,info,warning,error}=info<br>Log level |
| --hash TEXT | Check hash file |
| --spot_file TEXT | Save spot names |
| --debug | Debug mode |

## Positionals

files TEXT ...              FastQ files to parse

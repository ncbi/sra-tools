# read-filter-redact

## Usage

```text
read-filter-redact [Options] -F <file> <run>
```

## Options

| Option | Description |
|---|---|
| -F\|--file &lt;file&gt; | File containing SpotId-s to redact |
| -r\|--redact | Update the run to mask spots with<br>according to filter list |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

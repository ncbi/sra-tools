# ngs-pileup

## Usage

```text
ngs-pileup <path> [options]
```

## Options

| Option | Description |
|---|---|
| -r\|--aligned-region &lt;region&gt; | Filter by position on genome. Name can<br>either be file specific or canonical (ex:<br>"chr1" or "1"). "from" and "to" are 1-based<br>coordinates |
| --ngc &lt;PATH&gt; | PATH to ngc file |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

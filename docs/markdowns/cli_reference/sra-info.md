# sra-info

## Usage

```text
sra-info <accession> [options]
```

## Options

| Option | Description |
|---|---|
| -P\|--platform | print platform(s) |
| -Q\|--quality | are quality scores stored or generated |
| -A\|--is-aligned | is data aligned |
| -C\|--schema | print schema version and dependencies |
| -S\|--spot-layout | print spot layout(s). Uses CONSENSUS table<br>if present, SEQUENCE table otherwise |
| -T\|--contents | list the contents of the run: databases,<br>tables, columns etc. |
| -F\|--fingerprint | show the fingerprint information. Detail<br>level &lt;0&gt; (default) shows only the current<br>run fingerprint. Description of fingerprint<br>method available here: &lt;LINK TBD&gt; |
| -f\|--format &lt;format&gt; | output format:<br>csv ..... comma separated values on one line<br>xml ..... xml-style<br>json .... json-style<br>tab ..... tab-separated values on one line<br>csv and tab formats can only be used with a single query |
| --schema | does not support csv and tab |
| -l\|--limit &lt;N&gt; | limit output to &lt;N&gt; elements, e.g. &lt;N&gt; most<br>popular spot layouts; &lt;N&gt; must be positive |
| -D\|--detail &lt;N&gt; | detail level, &lt;0&gt; the least detailed<br>output; &lt;N&gt; must be zero or greater;<br>default 3 |
| -R\|--rows &lt;N&gt; | report spot layouts for the first &lt;N&gt; rows<br>of the table |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

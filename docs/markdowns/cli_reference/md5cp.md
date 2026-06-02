# md5cp

## Summary

Copies files and/or directories, creating an md5sum checksum
(named file.md5) for all copied files.

## Usage

```text
md5cp Options [file|directory ...] directory
```

## Options

| Option | Description |
|---|---|
| **Option** |  |
| -f\|--force | overwrite existing columns |
| -p\|--preserve | force replacement of existing modes on files<br>and directories |
| -r\|--recursive | Recurses over source directories<br>(directories are ignored otherwise). |
| -t\|--test | ? |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

# vdb-sql

## Usage

```text
vdb-sql [OPTIONS] FILENAME [SQL]
FILENAME is the name of an SQLite database. A new database is created
if the file does not previously exist.
```

## Options

| Option | Description |
|---|---|
| **OPTIONS include** |  |
| -ascii | set output mode to 'ascii' |
| -bail | stop after hitting an error |
| -batch | force batch I/O |
| -column | set output mode to 'column' |
| -cmd COMMAND | run "COMMAND" before reading stdin |
| -csv | set output mode to 'csv' |
| -echo | print commands before execution |
| -init FILENAME | read/process named file |
| -[no]header | turn headers on or off |
| -help | show this message |
| -html | set output mode to HTML |
| -interactive | force interactive I/O |
| -line | set output mode to 'line' |
| -list | set output mode to 'list' |
| -lookaside SIZE N | use N entries of SZ bytes for lookaside memory |
| -mmap N | default mmap size set to N |
| -newline SEP | set output row separator. Default: '\n' |
| -nullvalue TEXT | set text string for NULL values. Default '' |
| -pagecache SIZE N | use N slots of SZ bytes each for page cache memory |
| -scratch SIZE N | use N slots of SZ bytes each for scratch memory |
| -separator SEP | set output column separator. Default: '\|' |
| -stats | print memory stats before each finalize |
| -version | show SQLite version |
| -vfs NAME | use NAME as the default VFS |

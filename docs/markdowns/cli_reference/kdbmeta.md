# kdbmeta

## Usage

```sh
kdbmeta [Options] <target> [<query> ...]
```

## Summary

Display the contents of one or more metadata stores.
Update metadata.
The target metadata are described by one or more
target specifications, giving the path to a database, a table
or a column. the command and query are executed on each target.

queries name one or more objects, and '*' acts as a wildcard.
query objects are nodes or attributes. nodes are named with a
hierarchical path, like a file-system path. attributes are given
as a node path followed by a '@' followed by the attribute name.

target:
- **path-to-database**: access database metadata
- **path-to-table**: access table metadata
- **path-to-column**: access column metadata
- **accession**: sra global access id

query:
- *****: all nodes and attributes
- **NAME**: a named root node and children
- **PATH/NAME**: an internal node and children
- **<node>@ATTR**: a named attribute
- **<obj>=VALUE**: a simple value assignment where
- ****: value string is text, and binary
- ****: values use hex escape codes

## Options

| Option | Description |
|---|---|
| -T\|--table &lt;table&gt; | table-name |
| -u\|--unsigned | print numeric values as unsigned |
| -r\|--read-only | operate in read-only mode |
| -X\|--output &lt;value&gt; | Output type: one of (xml text):  whether to&lt;br&gt;generate well-formed XML. Default: xml&lt;br&gt;(well-formed) |
| --ngc &lt;path&gt; | path to ngc file |
| --delete &lt;node&gt; | delete node |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then&lt;br&gt;quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One&lt;br&gt;of (fatal\|sys\|int\|err\|warn\|info\|debug) or&lt;br&gt;(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program&lt;br&gt;status messages. Use multiple times for more&lt;br&gt;verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the&lt;br&gt;program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the&lt;br&gt;file. |

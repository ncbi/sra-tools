# sra-search

## Summary

Searches all reads in the accessions and prints Ids of all the fragments that contain a match.

## Usage

```text
sra-search [Options] query accession ...
```

## Options

| Option | Description |
|---|---|
| -h\|--help | Output brief explanation of the program. |
| -a\|--algorithm &lt;alg&gt; | Search algorithm, one of:<br>FgrepStandard (default)<br>FgrepBoyerMoore<br>FgrepAho<br>AgrepDP<br>AgrepWuManber<br>AgrepMyers<br>AgrepMyersUnltd<br>NucStrstr<br>SmithWaterman |
| -e\|--expression &lt;expr&gt; | Query is an expression (currently only supported for NucStrstr) |
| -S\|--score &lt;number&gt; | Minimum match score (0..100), default 100 (perfect match);<br>supported for all variants of Agrep and SmithWaterman. |
| -T\|--threads &lt;number&gt; | The number of threads to use; 2 by deafult |
| --threadperacc | One thread per accession mode (by default, multiple threads per accession) |
| --sort | Sort output by accession/read/fragment |
| --reference [refName,...] | Scan reference(s) for potential matches; all references if none specified |
| -m\|--max &lt;number&gt; | Stop after N matches |
| -U\|--unaligned | Search in unaligned and partially aligned reads only |
| --fasta [ &lt;lineWidth&gt; ] | Output in FASTA format with specified line width (default 70 bases) |

## Example

```sh
sra-search ACGT SRR000001 SRR000002
sra-search "CGTA||ACGT" -e -a NucStrstr SRR000002
```

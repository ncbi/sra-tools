# sra-search

## Usage

```sh
sra-search [Options] query accession ...
```

## Summary

Searches all reads in the accessions and prints Ids of all the fragments that contain a match.

## Example

sra-search ACGT SRR000001 SRR000002
sra-search "CGTA||ACGT" -e -a NucStrstr SRR000002

## Options

| Option | Description |
|---|---|
| -h\|--help | Output brief explanation of the program. |
| -a\|--algorithm &lt;alg&gt; | Search algorithm, one of:&lt;br&gt;FgrepStandard (default)&lt;br&gt;FgrepBoyerMoore&lt;br&gt;FgrepAho&lt;br&gt;AgrepDP&lt;br&gt;AgrepWuManber&lt;br&gt;AgrepMyers&lt;br&gt;AgrepMyersUnltd&lt;br&gt;NucStrstr&lt;br&gt;SmithWaterman |
| -e\|--expression &lt;expr&gt; | Query is an expression (currently only supported for NucStrstr) |
| -S\|--score &lt;number&gt; | Minimum match score (0..100), default 100 (perfect match);&lt;br&gt;supported for all variants of Agrep and SmithWaterman. |
| -T\|--threads &lt;number&gt; | The number of threads to use; 2 by deafult |
| --threadperacc | One thread per accession mode (by default, multiple threads per accession) |
| --sort | Sort output by accession/read/fragment |
| --reference [refName,...] | Scan reference(s) for potential matches; all references if none specified |
| -m\|--max &lt;number&gt; | Stop after N matches |
| -U\|--unaligned | Search in unaligned and partially aligned reads only |
| --fasta [ &lt;lineWidth&gt; ] | Output in FASTA format with specified line width (default 70 bases) |

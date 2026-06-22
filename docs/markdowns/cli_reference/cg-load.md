# cg-load

## Summary

Load a Complete Genomics formatted data files

## Usage

```text
cg-load [options] -m map-dir -o path-to-run
```

## Options

| Option | Description |
|---|---|
| `-m`\|`--map` | MAP input directory path containing files |
| `-o`\|`--output` | output database path |
| `-a`\|`--asm` | ASM input directory path containing files |
| `--load-extra-evidence` | load extra evidence files |
| `-s`\|`--schema` | database schema file name |
| `-k`\|`--refseq-config` | path to file with reference-to-accession list |
| `-i`\|`--refseq-path` | path to directory with reference sequences in fasta |
| `-r`\|`--ref-file` | path to fasta file with references |
| `-f`\|`--force` | force output overwrite |
| `-g`\|`--write-reference` | force reference sequence write |
| `-w`\|`--write-read` | force reads write |
| `-Q`\|`--qual-quant` | quality scores quantization level, can be number (0: none, 1: 2bit, 2: 1bit), or string like '1:10,10:20,20:30,30:-' (which is equivalent to 1) |
| `-G`\|`--no-spotgroup` | do not write source file key to SPOT_GROUP column |
| `-q`\|`--min-mapq` | filter secondary mappings by minimum weight (phred) |
| `-P`\|`--no-secondary` | preserve only one mapping per half-DNB based on weight |
| `--single-mate` | if secondary mates have duplicates preserve only one in each pair based on weight |
| `--cluster-size` | defines cluster window on the reference, records only placement from given cluster size; default is zero which means ignore |
| `-t`\|`--input-no-threads` | disable input files threaded caching |
| `-l`\|`--library` | copy extra file/directory into output |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Example

```sh
cg-load -m build36/MAP -o /tmp/SRZ123456
```

# ref-variation

## Summary

Find a possible indel window

## Options

| Option | Description |
|---|---|
| `-r`\|`--reference-accession <acc>` | look for the variation in this reference |
| `-p`\|`--position <value>` | look for the variation at this position on the reference |
| `--query <string>` | query to find in the given reference ("-" is treated as an empty string, or deletion). Optionally, for non-empty query, the variable number of repetitions can be specified in the following way: "< query >[< min_rep >-< max_rep >]" where < query > is the pattern which should be repeated, < min_rep > is the minimum number of repetiotions and < max_rep > is the maximum number of repetiotions to produce from query pattern. E.g.: "AT[1-3]" produces queries: "AT", "ATAT" and "ATATAT". In this case the output counts will contain as many columns for matched counts as many variations is produced from the given query. |
| `-l`\|`--variation-length <value>` | the length of the variation on the reference (0 means pure insertion) |
| `-t`\|`--threads <value>` | the number of threads to run |
| `-c`\|`--coverage <>` | output coverage (the nubmer of alignments matching the given variation query) for each run |
| `-i`\|`--input-file <string>` | take runs from input file rather than from command line. The file must be in text format, each line should contain three tab-separated values: < run-accession > < run-path > < pileup-stats-path >, the latter two are optional |
| `--count-strand <value>` | controls relative orientation of 3' and 5' fragments. "none" do not count (default). "counteraligned" as in Illumina. "coaligned" as in 454 or IonTorrent. |
| `--algorithm <value>` | the algorithm to use for searching. "sw" means Smith-Waterman. "ra" means Rolling bulldozer algorithm |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Example

```sh
ref-variation -r <reference accession> -p <position on reference> --query <query to look for> -l 0 [<parameters>]
```

## Parameters

optional space-separated list of run accessions in which the query will be looked for

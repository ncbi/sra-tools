# ref-variation

## Summary

Find a possible indel window

## Options

| Option | Description |
|---|---|
| -r\|--reference-accession &lt;acc&gt; | look for the variation in this reference |
| -p\|--position &lt;value&gt; | look for the variation at this position on<br>the reference |
| --query &lt;string&gt; | query to find in the given reference ("-"<br>is treated as an empty string, or<br>deletion). Optionally, for non-empty query,<br>the variable number of repetitions can be<br>specified in the following way:<br>"&lt;query&gt;[&lt;min_rep&gt;-&lt;max_rep&gt;]" where<br>&lt;query&gt; is the pattern which should be<br>repeated, &lt;min_rep&gt; is the minimum number<br>of repetiotions and &lt;max_rep&gt; is the<br>maximum number of repetiotions to produce<br>from query pattern. E.g.: "AT[1-3]"<br>produces queries: "AT", "ATAT" and<br>"ATATAT". In this case the output counts<br>will contain as many columns for matched<br>counts as many variations is produced from<br>the given query. |
| -l\|--variation-length &lt;value&gt; | the length of the variation on the<br>reference (0 means pure insertion) |
| -t\|--threads &lt;value&gt; | the number of threads to run |
| -c\|--coverage &lt;&gt; | output coverage (the nubmer of alignments<br>matching the given variation query) for<br>each run |
| -i\|--input-file &lt;string&gt; | take runs from input file rather than from<br>command line. The file must be in text<br>format, each line should contain three<br>tab-separated values: &lt;run-accession&gt;<br>&lt;run-path&gt; &lt;pileup-stats-path&gt;, the latter<br>two are optional |
| --count-strand &lt;value&gt; | controls relative orientation of 3' and 5'<br>fragments. "none" do not count (default).<br>"counteraligned" as in Illumina.<br>"coaligned" as in 454 or IonTorrent. |
| --algorithm &lt;value&gt; | the algorithm to use for searching. "sw"<br>means Smith-Waterman. "ra" means Rolling<br>bulldozer algorithm |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

## Example

```sh
ref-variation -r <reference accession> -p <position on reference> --query <query to look for> -l 0 [<parameters>]
```

## Parameters

optional space-separated list of run accessions in which the query will be looked for

# :fontawesome-solid-truck-fast: fasterq-dump

## Quick start

A faster fastq-dump
The fasterq-dump tool extracts data in FASTQ- or FASTA-format from SRA-accessions.
It is a commandline-tool that is available for Linux, macOS, and Windows.

The tool has one mandatory argument: the `accession`.

!!! example
    <!-- termynal -->

    ``` sh
    $ fasterq-dump SRR000001
    spots read      : 470,985
    reads read      : 1,883,940
    reads written   : 707,026
    reads 0-length  : 468,635
    technical reads : 708,279
    # This should produce three fastq files. *_1.fastq and *_2.fastq corresponding to paired reads and <acc>.fastq would be unpaired reads.
    $ ls
    SRR000001_1.fastq SRR000001_2.fastq SRR000001.fastq
    ```

    

!!! info
    The spots are split into biological reads, for each read : 4 lines of FASTQ or 2 lines of FASTA  are written. For spots having 2 reads, the reads are written into the *_1.fastq and *_2.fastq files. Unmated reads are placed in *.fastq. If the accession has no spots with one single read, the *.fastq-file will not be created.


!!! warning end fasterq-dump needs the temporary space
    fasterq-dump can use approximately up to 10 times the size of the final output-file. If you do not have enough space in your current directory for the output-file and the temporary files, _the tool will fail_.

!!! tip end Recommended to use prefetch first
    For best performance it is recommended to use `prefetch` to download the accession and then run `fasterq-dump`.


## Advanced usage

Fasterq-dump has many options, you can display them by running:

<!-- termynal -->
``` sh
$ fasterq-dump -h
...
```


or see [fasterq-dump-cli](/markdowns/cli_reference/fasterq-dump.md)

### Changing output directory, temporary directory or threads

You can change location of the output file, temporary direcotry or the number CPU cores used:

!!! example
    <!-- termynal -->
    ``` sh
    $ fasterq-dump --outdir myoutput_dir --temp /tmp/scratch --threads 6 SRR000001
    ```
    

    - Now the temporary files will be created in the `/tmp/scratch` directory. These temporary files will be deleted on finish, but the directory itself will not be deleted. If the temporary directory does not exist, it will be created.
    - The fastq files will be saved in `myoutput_dir`. If parts of the output-path do not exist, it will be created. If the output-files already exist, the tool will not overwrite them, but fail instead. If you want already existing output-files to be overwritten, use the force option `-f`.
    - The above command will use 6 CPUs (also the default). If you have more CPU cores it might help to increase this number. If you have a computer with much more CPU cores, increasing the thread count can lead to diminishing returns, because you exhaust I/O - bandwidth. If you specify a bare accession, there might be no gain in speed.


### Writing to stdout by not splitting paired-end reads

The spots are not split : 4 lines of FASTQ or 2 lines of FASTA are written into one output-file for each spot. This mode allows for the output to be redirected to stdout:

!!! example
    <!-- termynal -->
    ``` sh
    $ fasterq-dump SRR000001 --concatenate-reads --stdout 
    @SRR000001.1 EM7LVYS02FOYNU length=284
    TCAGATTCTCCTAGCCTACATCCGTACGAGTTAGCGTGGGATTACGAGGTGCACACCATTTCATTCCGTACGGGTAAATTTTTGTATTTTTAGCAGACGGCAGGGTTTCACCATGGTTGACCAACGTACTAATCTTGAACTCCTGACCTCAAGTGATTTGCCTGCCTTCAGCCTCCCAAAGTGACTGGGTATTACAGATGTGAGCGAGTTTGTGCCCAAGCCTTATAAGTAAATTTATAAATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG
    +SRR000001.1 EM7LVYS02FOYNU length=284
    ????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
    @SRR000001.2 EM7LVYS02GCAPL length=262
    TCAGTATATTTTCCTTCTTAGATTCCACGGCAGCCCTGTGAGTTAACAATCAACTCTGTTTCAAAGCTGAGGACACTGAGGCTCTAAGAGGTTAAATTATTGACCCAGATCACAAGAATGTTGGAACCGAAAGGGTTTGAATTCAAACCCTTTCGGTTCCAACACAGACTCAACCTGCATAATAAATAACATTGAAACTTAGTTTCCTTCTTGGGCTTTCGGTGAGAAAACATAAGTTAAAACTGAGCGGGCTGGCAAGGCN
    +SRR000001.2 EM7LVYS02GCAPL length=262
    ??????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
    ```
    


### Output unsorted fasta

The spots are split into reads, for each read : 2 lines of FASTA are written into the single output-file. This mode allows for the output to be redirected to stdout via: '--stdout ( -Z )'. This mode is indentical to the split-spot-mode, with the only difference beeing that the original order of the spots and reads is not preserved and it being exlusivly for FASTA. The reason for the existence of this mode is the fact that this mode is faster then the split-spot-mode, and does not use temporary files.

!!! example
    <!-- termynal -->
    ``` sh
    $ fasterq-dump SRR000001 --fasta-unsorted --stdout
    >SRR000001.1/2 EM7LVYS02FOYNU length=280
    ATTCTCCTAGCCTACATCCGTACGAGTTAGCGTGGGATTACGAGGTGCACACCATTTCATTCCGTACGGGTAAATTTTTGTATTTTTAGCAGACGGCAGGGTTTCACCATGGTTGACCAACGTACTAATCTTGAACTCCTGACCTCAAGTGATTTGCCTGCCTTCAGCCTCCCAAAGTGACTGGGTATTACAGATGTGAGCGAGTTTGTGCCCAAGCCTTATAAGTAAATTTATAAATTTACATAATTTAAATGACTTATGCTTAGCGAAATAGGGTAAG
    >SRR000001.2/2 EM7LVYS02GCAPL length=115
    TATATTTTCCTTCTTAGATTCCACGGCAGCCCTGTGAGTTAACAATCAACTCTGTTTCAAAGCTGAGGACACTGAGGCTCTAAGAGGTTAAATTATTGACCCAGATCACAAGAAT
    >SRR000001.2/4 EM7LVYS02GCAPL length=99
    ACAGACTCAACCTGCATAATAAATAACATTGAAACTTAGTTTCCTTCTTGGGCTTTCGGTGAGAAAACATAAGTTAAAACTGAGCGGGCTGGCAAGGCN
    >SRR000001.3/2 EM7LVYS02GJ8F2 length=59
    GGGGGGGGCACCATCTAATCAGCTGCCAGTGCTGCCAGAACATAAAGCAGGCAGAAAAA
    >SRR000001.3/4 EM7LVYS02GJ8F2 length=144
    AAACTGTGGGATATATACATGATGGGATACTACTCAGCCATAAAAGAAAAAAAATGAATTAATGGCATTCACAGCAACCTGGATGGGATTGGAGATTATCACTCCAAGTGAAGTAATTCAGGGATCTGAGCGGGCTGGCAAGGC
    ```
    

### Showing progress bar

In order to give you some information about the progress of the conversion there is a progress-bar that can be activated. The conversion happens in multiple steps, depending on the internal type of the accession. You will see either 2 or 3 progress bars after each other.

!!! example   
    <!-- termynal -->
    ``` sh
    $ fasterq-dump SRR341578 -p
    lookup : 
    ---> 100%
    merge  : 13255208
    join   : 
    ---> 100%
    concat : 
    ---> 100%
    spots read      : 7,549,706
    reads read      : 15,099,412
    reads written   : 15,099,412
    ```

### Customizing the read name

fasterq-dump supports a flexible defline. That means you can supply a user-defined defline for the the sequence and quality sections of FASTQ. There are 2 new commandline-parameters for that:

`--seq-defline FORMAT` and `--qual-defline FORMAT`

The format is a text that may contain these variables:

| Variable name | Description |
| --- | --- |
| `$ac` | the accession |
| `$sn` | the spot-name |
| `$sg` | the spot-group |
| `$si` | the spot-id ( the number of the spot ) |
| `$ri` | the read-id ( the number of a read within a spot ) |
| `$rl` | the read-length |

The accession, spot-id, read-id, and read-length are always available - but the spot-group and/or spot-name might be missing or empty. If a variable is missing or empty it does not produce an error - it will be omitted from the defline.

Please be aware that if the tool is used from within a shell-script, the format-string must be escaped to keep the shell from interpreting the variable names.

If no user-define is given to the tool, the following defaults are used:

FASTQ:

if not splitting `@$ac.$si $sn length=$rl`/`+$ac.$si $sn length=$rl`

if splitting: `@$ac.$si/$ri $sn length=$rl`/`+$ac.$si/$ri $sn length=$rl`

FASTA:

if not splitting `>$ac.$si $sn length=$rl`

if splitting: `>$ac.$si/$ri $sn length=$rl`

Be careful to choose the correct first character `@`/`+`/`>` based on the desired output (FASTQ/FASTA), as the tool will not correct it.

example for a shorter defline ( just containing the spot-name ) :

`$sam-dump SRRXXXXXX --seq-defline '@$sn' --qual-defline '+$sn'`


### Downloading PacBio data

For every version newer and including 3.0.5 of the sra-toolkit, the fasterq-dump tool can handle PacBio accessions. There is no commandline switch neccessary to enable this feature. These PacBio accessions may or may not contain a consensus table in addition to the regular sequence table. Fasterq-dump will produce its output from the consensus table if it is present; otherwise, it will use the regular sequence table. If the user wants the output sourced from the sequence table, even if a consensus table is present, the following command can be used:

fasterq-dump SRRXXXXXX --table SEQUENCE

For every version newer and including 3.0.5 of the sra-toolkit, the fasterq-dump tool has some new functionality regarding references.

### Retriving reference sequences for aligned submissions

The new option '--fasta-ref-tbl' extracts references used to align the accession in FASTA-format. This option only works on accessions which are aligned, and because of that have a reference table. Each reference is extracted into one single FASTA-record in the output file. The output file is named after the accession with the extension 'ref.fasta' appended.

fasterq-dump SRRXXXXXX --fasta-ref-tbl
This command produces a single file named 'SRRXXXXXX.ref.fasta'.

fasterq-dump SRRXXXXXX --fasta-ref-tbl -Z
This command produces the same output on stdout.

fasterq-dump SRRXXXXXX --ref-report
This command enumerates the references used by the accession on stdout. If the accession does not use references, the command will fail with an error message and return a non-zero error code. It can be used to test if an accession does contain references and is aligned.

fasterq-dump SRRXXXXXX --fasta-ref-tbl --internal-ref
This command extracts only internal references into the output file. Internal references are non-standard scaffoldings the submitter included in the submission and the bases of them are stored in the accession.

fasterq-dump SRRXXXXXX --fasta-ref-tbl --external-ref
This command extracts only external references into the output file. External references are canonical RefSeq accessions that are used in the accession. The bases of these references are not stored in the accession.

fasterq-dump SRRXXXXXX --fasta-ref-tbl --ref-name NC_011752.1
This command extracts only the named reference. The '--ref-name' option can be used multiple times per command. If a reference is not found, the option is ignored. If none of the requested references is found, an empty file is produced. Each reference used by an accession can be named in 2 ways: the canonical name like 'NC_001133.9' or the user-supplied name like 'I' or 'chr1'. The user supplied names are non-standard and specific to each accession. The user can use the '--ref-report' option to inspect the names used. Both names can be used to extract a specific reference. By default the canonical name is used in the defline of the FASTA-output. This default can be overwritten with the 'use-name' option. If this option is used, the submitter-supplied name is used in the defline of the FASTA-records.

There is one more option: '--fasta-concat-all'. This option is designed to be used on RefSeq accessions not on regular SRA accession. This option produces one single FASTA-record for the whole RefSeq accession.


## Known limitations

Here are some known limitations:

1. The `-Z|--stdout` option does not work if spliting the read pair (i.e. using `--split-3` or `--split-files` option).
   The tool will fail in these cases.
2. There is no `--gzip|--bizp2` option, you have to compress your files
   explicitly after they have been written.
4. `fasterq-dump` does not take multiple accessions, just one.
5. There is no `-N|--minSpotId` and no `-X|--maxSpotId` option.
   `fasterq-dump` processes always the whole accession, although it may support partial access in future versions.

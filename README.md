# The NCBI SRA (Sequence Read Archive)

### Contact:
email: sra@ncbi.nlm.nih.gov

### Download
Visit our [download page](https://github.com/ncbi/sra-tools/wiki/01.-Downloading-SRA-Toolkit) for pre-built binaries.

### Change Log
Please check the CHANGES.md file for change history.

## The SRA Toolkit
The SRA Toolkit and SDK from NCBI is a collection of tools and libraries for
using data in the INSDC Sequence Read Archives.

### ANNOUNCEMENT:
NIH has released a request for information (RFI) to solicit community feedback on new proposed Sequence Read Archive (SRA) data formats. Learn more and share your thoughts at https://go.usa.gov/xvhdr. The response deadline is July 17th, 2020. We’d encourage you all to share with your colleagues and networks, and respond if you are an SRA submitter or data user.

SRA Toolkit 2.11.0 March 15, 2021

  **fasterq-dump**: does not exit with 0 any more if the given path is not found  
  **fasterq-dump**: does not exit with 0 if accession is not found  
  **fasterq-dump**: does not fail when requested to dump a run file with non-standard name  
  **fasterq-dump**: available on windows  
  **kfg, prefetch, vfs**: resolve WGS reference sequences into "Accession Directory"  
  **kfg, sra-tools, vfs**: dropped support of protected repositories  
  **kns, sra-tools**: fixed formatting of HTTP requests for proxy  
  **ncbi-vdb, ngs, ngs-tools, sra-tools, vdb**: added support for 64-bit ARM (AArch64, Apple Silicon)  
  **prefetch, vfs**: fixed download of protected non-run files  
  **prefetch, vfs**: fixed segfault during download of JWT cart  
  **prefetch, vfs**: respect requested version when downloading WGS files  
  **sra-pileup**: now silent if requested slice has no alignments or reference-name does not exist  
  **sratools**: added description and documentation of the sratools driver tool to GitHub wiki  
  **sra-tools**: created a script to fix names of downloaded sra files  
  **sra-tools**: created a script to move downloaded sra run files into proper directories  
  **sratools**: disable-multithreading option removed from help text for tools that do not support it  
  **sratools**: does not access remote repository when it is disabled  
  **sra-tools, vfs**: recognize sra file names with version  
  **vdb-dump**: exits with no-zero value if asked for non existing column  

SRA Toolkit 2.10.8

kproc, fasterq-dump: fixed problem with seg-faults caused by too small stack used by threads
kdbmeta: allow to work with remote runs
kdb, vdb, vfs, sra-tools: fixed bug preventing use of path to directory created by prefetch if it ends with '/'
vfs, sra-tools, ngs-tools: report an error when file was encrypted for a different ngc file
prefetch: print error message when cannot resolve reference sequence
vfs, prefetch: download encrypted phenotype files with encrypted extension
vdb, sra-docker: config can auto-generate LIBS/GUID when in a docker container

SRA Toolkit 2.10.5
sratools: fixed a potential build problem in libutf8proc
ncbi-vdb, ngs, ngs-tools, sra-tools: all Linux builds now use g++ 7.3 (C++11 ABI)
prefetch: improvements were made to work in environments with bad network connections
prefetch, sratools: fixed the names of the --min-size and --max-size command line arguments when running prefetch

SRA Toolkit 2.10.4
kns, sra-tools:: fixed errors when using ngc file

SRA Toolkit 2.10.3
sraxf, fasterq-dump, fastq-dump, sam-dump: fixed a problem resulting in a segmentation fault

Release 2.10.2 of `sra-tools` provides access to all the **public and controlled-access dbGaP** of SRA in the AWS and GCP environments _(Linux only for this release)_. This vast archive's original submission format and SRA-formatted data can both be accessed and computed on these clouds, eliminating the need to download from NCBI FTP as well as improving performance.

The `prefetch` tool also retrieves **original submission files** in addition to ETL data for public and controlled-access dbGaP data.

With release 2.10.0 of `sra-tools` we have added cloud-native operation for AWS and GCP environments _(Linux only for this release)_, for use with the public SRA. `prefetch` is capable of retrieving original submission files in addition to ETL data.

With release 2.9.1 of `sra-tools` we have finally made available the tool `fasterq-dump`, a replacement for the much older `fastq-dump` tool. As its name implies, it runs faster, and is better suited for large-scale conversion of SRA objects into FASTQ files that are common on sites with enough disk space for temporary files. `fasterq-dump` is multi-threaded and performs bulk joins in a way that improves performance as compared to `fastq-dump`, which performs joins on a per-record basis _(and is single-threaded)_.

`fastq-dump` is still supported as it handles more corner cases than `fasterq-dump`, but it is likely to be deprecated in the future.

You can get more information about `fasterq-dump` in our Wiki at [https://github.com/ncbi/sra-tools/wiki/HowTo:-fasterq-dump](https://github.com/ncbi/sra-tools/wiki/HowTo:-fasterq-dump).

For additional information on using, configuring, and building the toolkit,
please visit our [wiki](https://github.com/ncbi/sra-tools/wiki)
or our web site at [NCBI](http://www.ncbi.nlm.nih.gov/Traces/sra/?view=toolkit_doc)


SRA Toolkit Development Team

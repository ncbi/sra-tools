# NCBI External Developer Release:
## SRA Toolkit 2.6.2
**April 20, 2016**

  **align-cache**: a tool producing vdbcache that reduces effects of random access and improves speed.  
  **bam-load**: Fixed bug in validation code for unsorted bam files  
  **bam-load**: If two (or more) local reference names refer to the same global reference, bam-load will record the first one used and report the change.  
  **bam-load**: Secondary alignment will be used for spot assembly if the loader is explicitly told to use them  
  **bam-load**: The code that had invalid item in index problem was removed in the process of addressing a performance issue.  
  **bam-load**: change reporting of fatal warnings into fatal errors  
  **bam-load**: changed from an error to a warning if using secondary alignments to create spots  
  **bam-load**: low-match secondary alignments are now discarded; low-match primary alignments are logged, and if too many, it will abort the load.  
  **bam-load**: records the contents of the BX tag  
  **bam-load**: rules for spot assembly were reverted to assembling spots only on primary alignments  
  **blast, build**: Improved blast tools: all required libraries are linked statically.  
  **build**: Allow to build sra-tools on systems without static c++ library  
  **check-corrupt, vdb-validate**: Added a new set of checks that can be triggered by using one of two "--sdc:" cmd options  
  **copycat**: now runs on Centos 7; no longer uses system-installed magic file  
  **dbgap-mount**: Added support for standard options  "-L" and "-o", which allow users to determine the logging level and log output file  
  **dbgap-mount**: New optional parameter was introduced '-u' which allows user to unmount FUSE/DOKAN volume. Implemented on linux and windows  
  **fastq-load**: The (old) fastq-loader will properly report multiple reads on the 454 platform (and still fail).  
  **kar**: added '--md5' option to create md5sum compatible auxiliary file  
  **kdb**: Fixed VTableDropColumn, so that it can drop static columns  
  **kfs, kns, ngs, sra-tools**: Fixed thread safety issues for both cache and http files  
  **kget**: has a new option --full to match wget in speed. added examples.sh  
  **kproc**: Fixed KQueue to wake waiters when sealed, fixed KSemaphore to wake waiters when canceled  
  **latf-load**: now allows undescores inside spot group names   
  **latf-load**: now loads data produced by fastq-dump  
  **latf-load**: updated to support Illumina tag line format with identifier at the front  
  **pileup-stats**: added -V (--version) option: prints out the software  
  **pileup-stats**: pileup-stats: added version support (options -V or --version)  
  **prefetch**: Added --eliminate-quals option which speeds up HTTP download by ignoring QUALITY column`s data  
  **prefetch**: Fixed failure when running prefetch.exe "-a<bin|key>" when there is a space after "-a"  
  **prefetch**: messages about maximum size of download are made more user-friendly.  
  **prefetch**: now will download even when caching is disabled  
  **ref-variation**: --input-file option allows to specify input accessions and paths in the file  
  **ref-variation**: added "count-strand" option: it controls relative orientation of 3' and 5' fragments.  
  **ref-variation**: added -c option to flush output immediately; reporting zero matches  
  **ref-variation**: added a way to specify a number of repeats of the query  
  **ref-variation**: improved threading management  
  **ref-variation**: removed irrelevant warnings reported in some cases in debug version only  
  **sam-dump**: Segfault no longer occurs when confronted with large amounts of header lines  
  **sam-dump**: added option to produce MD tags  
  **sam-dump**: filters out duplicates in the rows that it generates  
  **sam-dump**: produces BX-tags if preserved in SRA file by bam-load  
  **sra-sort**: correctly generates spot-id column even in the absence of primary alignments  
  **sra-stat**: no longer fails when CS_NATIVE column is not present.  
  **sra-tools, vdb-config**: Removed dependency of mac binaries on unnecessary libraries, e.g. libxml2.  
  **sra-tools**: [https://github.com/ncbi/sra-tools/issues/27](https://github.com/ncbi/sra-tools/issues/27) : contains short and long examples of how to configure sra-tools build  
  **var-expand**: a tool for batch variation expansion  
  **vdb-config**: now handles standard options such as --option-file  
  **vdb-validate**: Added code to continue with the next row when column has discontiguous blobs  



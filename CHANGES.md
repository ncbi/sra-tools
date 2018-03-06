# NCBI External Developer Release:


## SRA Toolkit 2.9.0
**February 23, 2018**

  **bam-load**: an issue with accessing WGS accessions was fixed  
  **bam-load**: bam-load will generate an error and quit when too many spots have been encountered  
  **bam-load**: renamed an internal function to avoid a name conflict  
  **bam-load, fastq-load**: updated to use better thread termination signaling  
  **bam-load, sra-stat**: Updated sra-stat to extract statistics of alterations made by loaders for inclusion in its report  
  **build**: Created a script that allows to add a new volume to existing repository  
  **build**: Fixed configure allowing to run it on Perl with version >= v5.26 that has "." removed from @INC  
  **build**: added "smoke tests"  
  **build**: recognize version of libhdf5 that does not allow static linking and do not try to use it  
  **build, doc**: added wiki page: Building-from-source-:--configure-options-explained  
  **build, ncbi-vdb, sra-tools**: the installation script now saves configuration files if they were modified by the user  
  **build, sra-tools**: "make runtests" now invokes "make all"  
  **build, vdb-sql**: modified build to avoid vdb-sql in absence of libxml2  
  **fastq-dump**: minor change to help text  
  **fastq-dump, vdb**: Fixed crashing of fastq-dump when dumping multiple runs with -split-3 option specified  
  **fastq-load**: preserves spot names when the platform is Illumina  
  **kfg**: added searching of configuration files in ../etc/ncbi/ relative to the binaries  
  **kfg, prefetch**: set limit of Aspera usage to 450m  
  **kfg, prefetch, remote-fuser, vfs**: Updated resolving of cache location of non-accession objects  
  **klib**: Reverted KTimeMakeTime to use UTC  
  **kns**: Accept the same http_proxy specifications as wget  
  **kns**: Added possibility to report server's IP address after network error  
  **kns**: Ignore HTTP headers sent multiple times  
  **kns**: Improved reporting of network errors  
  **kns**: fixed generation of invalid error code in response to dropped connection  
  **latf-load**: now processing multi-line sequences and qualities  
  **latf-load**: pacbio spot names with a range are now processed correctly  
  **pileup-stats**: pileup-stats now exits with code 3 if called without arguments  
  **prefetch**: fixed a bug in prefech when it printed invalid error messages after failure of reading from server  
  **sra-search**: added option --fasta for output in FASTA format  
  **sra-search**: added option to display version number  
  **sra-search**: added option to search unaligned and partially aligned fragments only  
  **sra-search**: improved performance in reference-driven mode  
  **sra-search**: various efficiency/readability improvements in the code   
  **sra-sort**: Created a separate version  of sra-sort for Complete Genomics  
  **sra-sort**: Fixed race condition in sra-sort when result was not completed when using fast drives  
  **sra-stat**: Added calculation of N50, L50, N90, L90 statistics  
  **sra-stat**: Fixed: sra-stat prints the path of alignment reference when the path is remote (http)  
  **sra-stat**: Improved performance when calculating bases statistics  
  **sra-stat**: The maximum number of reads that can be processed was Increased to 4K.  
  **sra-tools, vdb**: access to vdb/ngs via SQLite  
  **srapath**: srapath allows to get results of name resolver CGI  
  **vdb-config**: vdb-config does not fail when /repository/user/default-path is not set in configuration  
  **vdb-validate**: added a check of sum(READ_LEN) against length(READ)  
  **vfs**: Name resolving service was updated and switched to protocol version 3.0  


## SRA Toolkit 2.8.2
**March 6, 2017**

  **blast**: Updated blast library to be able to process runs having empty rows  
  **blast, build**: removed library dependencies that were preventing users from launching these tools  
  **blast, sra-tools**: Prepared completely static build of blast tools for windows with HTTPS support  
  **build**: **bam-load**: changed memcpy to memmove when regions overlap - seems to corrupt data on centos7  
  **build**: Added ability to specify ncbi-vdb/configure --with-magic-prefix. Look for libraries in (lib lib64) when running "configure --with-...-prefix"  
  **build**: configure detects location of ngs libraries  
  **build**: configure was fixed to skip options unrecognized by gcc 4.4.7  
  **build**: created sra-toolkit Debian package  
  **build**: fixed a bug in 'configure' when in could not find source files in repository saved with non-standard name  
  **build, ncbi-vdb, sra-tools**: installation will back up old configuration files if they differ from the ones being installed  
  **cg-load**: added loading of CG File format v2.4  
  **kns**: SRA tools respect standard set of environment variables for proxy specification  
  **kns**: updated mbedtls library to version 2.4.1  
  **ncbi-vdb, ngs, ngs-tools, sra-tools**: eliminated memcpy from sources due to potential for overlap  
  **ngs, sra-search**: now supports search on reference  
  **ngs-tools**: updated the NCBI download page to incorporate ngs versions into 3rd party package names  
  **prefetch**: Fixed error message "path excessive while opening directory" when prefetch is trying to get vdbcache  
  **prefetch**: Fixed regression in prefetch-2.8.1 when downloading dbGaP files via HTTP  
  **prefetch**: Fixed regression in prefetch-2.8.1 when downloading vdbcache files from dbGaP via HTTP  
  **sam-dump**: consistency of sam-dump in fastq-mod improved  
  **sam-dump**: consistency of sam-dump in fastq-mode improved  
  **sra-stat**: sra-stat does XML-escaping when printing spot-groups  
  **test-sra**: extended test-sra to debug user problems with https connections to NCBI  
  **test-sra**: test-sra print amount of available disk space in user repositories  
  **vdb-config**: vdb-config correctly works when there is non-canonical path in configuration  


## SRA Toolkit 2.8.1-2
**January 19, 2017**

  **prefetch**: fixed download of dbGaP files via HTTP


## SRA Toolkit 2.8.1
**December 22, 2016**

  **bam-load**: the result code updated to indicate empty slice rather than EOF  
  **kfg**: added possibility to create an empty KConfig object that does not try to load any file  
  **latf-load**: fixed an occasional crash on Ubuntu  
  **latf-load, test**: test script no longer executes failing tests for unimplemented features  
  **prefetch**: uses KStream rather than KHttpFile - it fixed environments with proxies non supporting HTTP Range  
  **sam-dump**: use of --seqid options creates now headers consistent with sam-lines  
  **test-sra**: added ability to print http response headers  


## SRA Toolkit 2.8.0-2
**December 8, 2016**

  **bam-load**: changed memcpy to memmove when regions overlap - seems to corrupt data on centos7
  **blastn_vdb, tblastn_vdb**: removed library dependencies that were preventing users from launching these tools
  **build**: fixed a bug in 'configure' when in could not find source files in repository saved with non-standard name
  **cg-load**: added loading of CG File format v2.4


## SRA Toolkit 2.8.0
**October 7, 2016**

### HTTPS-ENABLED RELEASE

  **bam-load**: 10x Genomics CB and UB tags are preserved  
  **bam-load**: Orphaned secondary alignments will be converted to primary alignments  
  **bam-load**: READ_GROUP is populated from 'BC' if 'RG' has no value  
  **bam-load**: fixed support for '-V' and '--version' command-line options  
  **blast**: Updated BLAST engine to 2.5.0+ version  
  **build, ngs-tools**: Now ngs-tools look for its dependencies using their normal build paths and does not reconfigure them  
  **build, ngs-tools**: Now ngs-tools use CMAKE_INSTALL_PREFIX for installation path  
  **build, sra-tools**: Now makefile copies default.kfg file when it is updated  
  **build, sra-tools**: separate decryption package was removed - decryption tools are included as part of sratoolkit.  
  **kfg, kns**: Use environment variables to determine the proxy for a http protocol  
  **kfg, vdb-config**: vdb-config-GUI has now option to pick up proxy-setting from environment  
  **kns**: All tools and libraries now support https  
  **kns**: replaced all direct uses of sleep() within code to enforce standardization upon mS.  
  **kproc, ncbi-vdb**: Fixed KCondition to generate timeout error on Windows when timeout exhausted  
  **latf-load**: now handles column values up to 64MB long  
  **ngs**: Fixed all crashes when using null as string in ngs-java APIs  
  **ngs**: NGS_ReferenceGetChunk() will now return chunks potentially exceeding 5000 bases  
  **ngs**: fixed potential concurrency issues at exit, when called from Java  
  **ngs**: ngs-java and ngs-python auto-download (of native libraries) now works through HTTPS  
  **ngs**: read fragments of length 0 are now ignored  
  **ngs, ngs-tools, ref-variation**: added class ngs-vdb::VdbAlignment, featuring method IsFirst()  
  **ngs-engine**: improved diagnostic messages  
  **ngs-tools**: Fixed Makefiles to keep supporting "./configure; make" build of sra-search, alongside CMake-based build.  
  **prefetch**: Fixed prefetch not to print misleading 'unknown integer storage type' error messages  
  **sam-dump**: CB and UB tags are now created if loaded via bam-load from 10xSingleCell  
  **sra-tools**: presence of ./ncbi (even if empty) subdirectory next to the executable files is no longer required, unless configuration files are needed.  
  **test**: updated tests to not fail outside of NCBI  
  **test-sra**: test-sra prints network information  
  **test-sra**: test-sra prints version of ncbi-vdb or ngs-sdk dynamic library  
  **vdb**: improved parameter checking on VDatabaseOpenTableRead()  
  **vdb**: new function: "VDBManagerDeleteCacheOlderThan()"  
  **vdb**: problem with buffer-overrun when compressing random data fixed  
  **vdb**: remote/aux nodes have been removed from configuration  
  **vdb-dump**: does not ignore table-argument on plain table any more, has to be SEQUENCE on plain tables if used  


## SRA Toolkit 2.7.0
**June 12, 2016**

  **align, bam-load**: Insert-only alignments no longer cause incorrect binning  
  **bam-load**: fixed case where WGS accessions where not being read correctly  
  **bam-load**: will NOT perform spot assembly using hard clipped secondary alignments, even when 'make-spots-with-secondary' is enabled; WILL perform spot assembly using hard-clipped secondary alignments when 'defer-secondary' is enabled  
  **blast, kfg, ncbi-vdb, sra-tools, vfs**: restored possibility to disable local caching  
  **build, sra-tools**: Running make and make install in sra-tools repository prepares all configuration files required to access NCBI repository  
  **doc, ncbi-vdb**: created a Wiki page illustrating how to use the API to set up logging  
  **fastdump, sra-tools**: new tool to perform fast dumps of a whole run in either fastq or a custom format for one of our customers.  
  **kar**: Alter the default ordering of components of an SRA archive for better network performance  
  **kdb, kfs, kns**: Added blob validation for data being fetched remotely  
  **kfg**: When loading configuration files on Windows USERPROFILE environment variable is used before HOME  
  **kfg**: modified auxiliary remote access configuration to use load-balanced servers  
  **kns**: Fixed a bug when KHttpRequestPOST generated an incorrect Content-Length after retry  
  **ngs, search, sra-search**: sra-search was modified to support multiple threads.  
  **ngs-engine, ngs-tools, sra-tools, vfs**: The "auxiliary" nodes in configuration are now ignored  
  **pileup-stats**: updated commandline parser to eat unprocessed parameters  
  **sam-dump**: updated to append asterisks to quality field of SAM output when corrupt original BAM has secondary alignment is shorter than the primary.  
  **search**: now supports multi-threaded search  
  **sra-search**: now supports sorted output  
  **sra-tools**: added possibility to build rpm package in sra-toolkit  
  **sra-tools**: fixed exit codes for a number of applications in response to command line options  
  **vdb-dump**: added tests to verify vdb-dump operation on nested databases  
  **vdb-validate**: A new checks were added for SEQUENCE table  
  **vdb**: fixed a bug in VCursorFindNextRowIdDirect where it returned a wrong rowId  
  **vdb**: fixed a bug in the code used to iterate over blobs where rowmap expansion cache would reset iteration to initial row instead of respecting sequence  
  **vfs**: environment variable VDB_PWFILE is no longer used  


## SRA Toolkit 2.6.3
**May 25, 2016**

  **bam-load**: Corrects an optimization used to compare read lengths when lengths are greater than 255
  **bam-load**: alignments which are below the minimum match count but with at least 1/2 of the aligned bases matching are accepted
  **bam-load**: improved performance of SAM parsing code
  **bam-load**: non-fatal result codes no longer cause the reader thread to quit early
  **bam-load**: will NOT do spot assembly using hard clipped secondary alignments even when make-spots-with-secondary is enabled; WILL do spot assembly using hard-clipped secondary alignments when defer-secondary is enabled
  **build**: MSVS 2013 toolset (12.0) is now supported across all repositories
  **vdb**: Fixed a bound on memory cache that would never flush under certain access modes

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

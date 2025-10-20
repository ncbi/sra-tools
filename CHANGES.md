----
                          PUBLIC DOMAIN NOTICE
             National Center for Biotechnology Information

This software/database is a "United States Government Work" under the
terms of the United States Copyright Act.  It was written as part of
the author's official duties as a United States Government employee and
thus cannot be copyrighted.  This software/database is freely available
to the public for use. The National Library of Medicine and the U.S.
Government have not placed any restriction on its use or reproduction.

Although all reasonable efforts have been taken to ensure the accuracy
and reliability of the software and data, the NLM and the U.S.
Government do not and cannot warrant the performance or results that
may be obtained by using this software or data. The NLM and the U.S.
Government disclaim all warranties, express or implied, including
warranties of performance, merchantability or fitness for any particular
purpose.

Please cite the author in any work or product based on this material.

----
# NCBI External Developer Release:


## SRA Toolkit 3.3.0
**October 28, 2025**

  **fasterq-dump**: allow reads longer than 65k  
  **kdbmeta**: added command to delete a metadata node  
  **kdbmeta, ngs, pileup-stats, prefetch, srapath, vfs**: improved directory path handling  
  **kfg, prefetch**: improved file type handling  
  **ncbi-vdb, sra, sra-info**: added Salus and Geneus platforms  
  **read-filter-redact**: sets quality score to 0 when masking filtered reads with N  
  **SharQ**: reports version in the standard toolkit format  
  **vdb-dump**: fixed segfault in case of a platform column with no value  


## SRA Toolkit 3.2.1
**March 18, 2025**

  **fasterq-dump**: fixed race condition when using WGS references  
  **fasterq-dump**: fixed a problem with '.' in output-path  
  **fasterq-dump, kns, sratools, vdb, vfs**: fixed failure of fasterq-dump when accessing run via HTTP  
  **prefetch**: improved error reporting when failed to download a file or dependency  
  **SharQ**: records fingerprinting information  in the output object's metadata  


## SRA Toolkit 3.2.0
**January 14, 2025**

  **align-info, prefetch, vdb, vfs**: prints an error message when unable to resolve an external reference  
  **bam-load, vdb**: the load process is terminated when background flush terminates with an error  
  **fastq-dump, kfs**: fixed infinite loop  
  **kapp, kns, sra-tools**: always use 3 part tool version in User-Agent request header  
  **kdb, vdb-validate**: will find corruption via the MD5 sums  
  **kdb, vdb, vdb-dump**: fixed request of vdb-version on AD (Accession-as-Directory)  
  **ncbi-vdb, ngs-tools, sra-tools**: fixed error message asking to run configure  
  **sra**: added sequencing platform SINGULAR_GENOMICS  
  **sra-tools, vfs**: fixed resolving of WGS references in AD when not running in the same directory  
  **sra-tools, vfs**: fixed VDB version headers to always include patch version  
  **sra-tools, vfs**: simplified name resolving  
  **sra-tools, ngs-tools**: SRA Tools were moved from ngs-tools to sra-tools


## SRA Toolkit 3.1.1
**May 21, 2024**

  **kns, prefetch**: improved reaction to problems with data  
  **ncbi-vdb, sra-tools**: added build rules for BSD  
  **prefetch**: improved information messages  
  **prefetch**: prints an error message when no response from names resolver  
  **prefetch**: reduced output when using --quiet  
  **prefetch**: tracking of corrupt downloads improved  
  **sra-stat**: allows information about local file  


## SRA Toolkit 3.1.0
**March 5, 2024**

  **cloud, kns, ngs-tools, sra-tools**: don't change global network timeouts when checking cloud location  
  **kdb, kdbmeta, sra-tools**: removed confusing warning in Windows debug build   
  **kns, ngs-tools, sra-tools**: fixed hanging on Mac and BSD when data access is unreliable  
  **prefetch**: use of --eliminate-quals enforces download of SRA Lite files  
  **sra-stat**: fixed load metadata for databases  
  **sra-tools, sratools**: fixed an error on Windows when using a file path  
  **vdb-validate**: will warn about missing checksums  


## SRA Toolkit 3.0.10
**December 19, 2023**

  **cloud, ngs-tools, sra-tools**: accept AWS credentials in CSV format; warn when credentials file cannot be loaded; don't load credentials if user doesn't agree to pay  
  **cloud, ngs-tools, sra-tools**: fixed support of IMDSv2 to allow instance identity on Amazon Linux 2023  
  **fasterq-dump**: does not emit warning when used on a not-kar'ed accession  
  **fasterq-dump**: handles tiny accession correctly  
  **kns, ngs-tools, sra-tools, vfs**: print NCBI_PHID after error in names resolving to simplify troubleshooting  
  **ncbi-vdb, ngs-tools, sra-tools**: added support for Arm64  
  **ngs-tools, sra-tools, vdb, vfs**: avoids multiple calls to resolve the same accession  
  **prefetch**: prints to stdout  
  **vdb-dump**: added support for new platforms  
  **vdb-validate**: optional check for correct redaction added  


## SRA Toolkit 3.0.8
**September 19, 2023**

  **align, bam-load**: 'circular' can be set from configuration for fasta references  


## SRA Toolkit 3.0.7
**August 29, 2023**

  **abi-load, fastq-load, helicos-load, illumina-load, pacbio-load, sff-load, sra, srf-load**: fixed loaders to work without predefined schema  
  **align-info, prefetch, vdb, vfs**: stopped printing incorrect 'reference not found' messages; reduced number of calls to resolve WGS references; align-info don't resolve references remotely if found locally  
  **bam-load**: changed 'secondary' to 'non-primary' in log messages  
  **bam-load, kdb, vdb-validate**: index works correctly with only one key  
  **cloud, kns, ngs-tools, sra-tools**: added support of IMDSv2 to allow to use instance identity on new AWS machines  
  **cloud, ngs-tools, sra-tools, vdb-config**: fixed use of AWS credentials  
  **kns, ngs, ngs-tools, sra-tools**: fixed a bug that caused failure during accession resolution while reading HTTP stream  
  **sra-info**: added parameter --rows to spot layout query  
  **sra-info**: added query --schema  
  **sra-info**: fixed formatting of multiple queries  
  **vdb**: added database-contained view aliases to the schema language  
  **vdb-dump**: new --inspect feature to show column-size and compression ratios  
  **vdb-validate**: compensated for loader optimization that caused a segfault  
  **vdb, vdb-validate**: fixes a memory leak  


## SRA Toolkit 3.0.6
**July 10, 2023**

  **bam-load, sra-stat**: fixed false detection of CMP_BASE_COUNT mismatch for unaligned runs  
  **cloud, sra-tools**: fixed a bug in reading chunked HTTP responses in GCP-related code  
  **fasterq-dump**: now switches to split-files if asked to include technical reads  
  **sra-info**: added query --spot-layout  
  **sratools**: better handling for URL command line arguments  
  **sra-tools wiki**: new features for fasterq-dump appended  
  **vdb-config**: handles path on windows for temp. storage correctly  
  **vdb-copy**: return error code in case of invalid/missing input  


## SRA Toolkit 3.0.5
**May 9, 2023**

  **bam-load**: added option "--min-batch-size" to control spot assembly  
  **copycat**: added support for TAR archives in PAX format; added support for FASTQ format: recognition, defline name and pair extraction; minor bug fixes  
  **fasterq-dump**: added error reports  
  **fasterq-dump**: can now handle native PacBio HDF5-accessions  
  **fasterq-dump**: clearer message if the output-file(s) cannot be overwritten or created  
  **fasterq-dump, fastq-dump, sam-dump**: fixed parameter handling for ngc file  
  **fasterq-dump**: fixed a crash when modifying defline on flat tables  
  **fasterq-dump**: new options to output the references of an accession  
  **fasterq-dump**: new options to produce FASTA for WGS and REFSEQ-objects  
  **fasterq-dump**: new python3 tests against multiple options and accessions  
  **fasterq-dump**: now handles PacBio-accession loaded with bam-load  
  **fasterq-dump**: read-id is now always correctly populated  
  **fastq-load, SharQ**: added new platform Element Aviti  
  **kfg, ngs-tools, sra-tools, vfs**: stopped using old names resolver cgi  
  **latf-load**: will not randomly print "KQueuePop failed" error at the end of processing  
  **read-filter-redact**: stop building on Windows  
  **sam-dump, sra-pileup**: added error report  
  **sra-info**: new tool to report run info  
  **sra-tools**: added schemas to reduce warnings  
  **sratools**: mismatching lite/normalized cache files from SDL are not used  
  **test-sra**: updated to call modern names service; fixed malformed XML  
  **vdb-validate**: fixed a potential crash due to memory corruption  
  **vdb-validate**: removed warning message about no platform column  


## SRA Toolkit 3.0.3
**January 3, 2023**

  **sra-stat**: fixed crash caused by a regression when processing runs with a default SPOT_GROUP in metadata  


## SRA Toolkit 3.0.2
**December 12, 2022**

  **prefetch**: extended URL buffer size that caused 'buffer insufficient while converting string within text module' failure on Mac  


## SRA Toolkit 3.0.1
**November 15, 2022**

  **bam-load**: bam-load is not built if openssl is not found  
  **bam-load**: distributing bam-load for Mac stopped  
  **bam-load**: reference resolution honors more the configuration information  
  **bam-load**: spot assembly redesign  
  **build**: added support for generating coverage reports  
  **build**: added support for overriding cmake and ctest commands  
  **build**: improvements to build system. see `make help`  
  **build**: now supports custom library installation directory  
  **build**: will use a system-provided libmbedtls, otherwise the copy included in the source code will be used  
  **fasterq-dump**: having the same sequence in the sequence-table as well as in the alignment table is tolerated now  
  **fasterq-dump**: includes accession in help-text now  
  **kff**: now using system-provided libmagic if present  
  **kfg**: fixed a bug that caused override of user configuration  
  **kfg, sra-tools**: removed interactive requirement to configure SRA Toolkit  
  **kfs**: fixed bug with long paths  
  **kns, sra-tools**: allow to use AWS data with compute environment via proxy  
  **latf-load**: fixed a bug causing a potential freeze on reaching a maximum number of errors  
  **pacbio-load, vdb**: hdf5 support is now a part of sra-tools/pacbio-load; uses system-provided libhdf5   
  **ref-variation**: added libraries and tools: ngs-vdb, general-writer, ref-variation, sra-search, general-loader, pileup-stats  
  **sam-dump**: missing quality score printed as *  
  **sam-dump**: new option to output '*' for quality scores  
  **SharQ**: supports nanopore FASTQ format  
  **sra-pileup, vfs**: fixed crash when working with no-quality run  
  **sra-stat**: prints detailed information for detected mismatch of recorded and calculated statistics  
  **sra-tools**: new parameters to make:  BUILD_TOOLS_LOADERS: build loaders (OFF by default);  BUILD_TOOLS_INTERNAL: build internal pipeline tools (OFF by default);  BUILD_TOOLS_TEST_TOOLS: build internal test tools (OFF by default)  
  **sra-tools, vdb**: retired vdb-get  
  **sra-tools, vfs**: fixed infinite loop when processing bad response when resolving accession  
  **test**: stopped distribution of Makefiles for NGS C++ examples  
  **vdb**: kxml and kxfs are combined and moved to sra-tools  
  **vdb**: updated zlib version to fix vulnerability  
  **vfs**: now recognizes sralite.vdbcache files  


## SRA Toolkit 3.0.0
**February 10, 2022**

  **blast**: BLAST tools are now distributed by Blast team  
  **build**: clarified the output of the configure script  
  **build, ncbi-vdb, sra-tools**: added support for building with XCode  
  **build, ncbi-vdb, sra-tools**: the build system has been converted to use CMake  
  **build, ngs, sra-tools**: ngs project has been moved to sra-tools  
  **fasterq-dump**: fails if stdout requested, but there are multiple output streams  
  **fasterq-dump, fastq-dump, sratools**: fixed a bug that caused multiple accession to cause the tool to exit with an error  
  **fasterq-dump**: fixed problem where files could be deleted if output directory was a symlink  
  **fasterq-dump**: now with pre-flight mode  
  **fasterq-dump, vfs**: recognize prefetched dbGaP runs  
  **fastq-dump**: create correct name when processing protected run  
  **latf-load**: added support for interleaved file I/O  
  **latf-load**: added support for long barcodes with '+' in them  
  **latf-load**: updated spot assembly for reads with # style barcodes  
  **latf-load**: updated spot assembly to pair /1 and /2 reads  
  **latf-load**: will no longer accept duplicate read names  
  **ngs**: clients will not download dynamic libraries anymore  
  **prefetch**: fixed checking of dependencies for absolute path  
  **prefetch, vdb**: fixed downloading reference sequences into "Accession-Directory" when zero-quality is preferred  
  **remote-fuser**: was retired  
  **sratools**: seq-defline and qual-defline options now work  
  **sratools**: tries harder to find the executable path on macOS  
  **vdb-decrypt**: remove "_dbGaP" suffix when decrypting protected run  
  **vdb-dump**: exits with non-zero return code when reference sequences are not found  
  **kfs, sra-tools**: fixed a bug in handling of long URLs on Windows  


## SRA Toolkit 2.11.3
**October 25, 2021**

  **fasterq-dump, sratools**: fasta and fasta-unsorted parameters work correctly  


## SRA Toolkit 2.11.2
**October 7, 2021**

  **fasterq-dump**: added flexible defline, fasta-unsorted, only-aligned, only-unaligned  
  **fasterq-dump**: new output format available: --fasta  
  **fasterq-dump**: option -t sets directory of all temp files (including VDB cache)  
  **klib, ngs-tools, sra-tools**: status messages (-v) are printed to stderr rather than stdout  
  **kns, ngs-tools, sra-tools**: old verbose messages now happen at verbosity > 1  
  **ncbi-vdb, ngs-tools, sra-tools, vdb, vfs**: added  support of SRA Lite files with simplified base quality scores  
  **prefetch, vfs**: accept Percent Encoding in source URLs  
  **sam-dump**: fixed wrong value for SAM RNEXT field for unaligned records  
  **sra-tools, sratools**: verbose option displays information about the type of quality scores contained in the files being used  
  **sratools**: temporarily changes quality score preference when quality scores are not being used  
  **sra-tools, vdb**: environment variable NCBI_TMP_CACHE sets the caching directory, overriding any other setting  
  **vdb-dump**: --info produces valid JSON  


## SRA Toolkit 2.11.1
**August 17, 2021**

  **align, axf, sra-pileup, vdb, vfs**: resolve reference sequences within output directory  
  **cloud, kns, sra-tools**: do not acquire CE more often than necessary  
  **kget**: renamed to vdb-get  
  **kns, sra-tools**: improved reporting of peer certificate information  
  **kns, sra-tools**: improved timeout management in CacheTeeFile  
  **ncbi-vdb, ngs, ngs-tools, sra-tools**: configure prints the version of compiler  
  **prefetch**: better control of reference sequences  
  **prefetch**: fixed failure when protected repository exists  
  **prefetch, vdb, vfs**: prefetch with "-O" will now correctly place references in output directory  
  **prefetch, vfs**: fixed error message 'multiple response SRR URLs for the same service...' when downloading  
  **prefetch**: will download any missed dependencies  
  **prefetch**: will not hang when failing to download dependencies  
  **sratools**: allow driver tool to handle debug output for modules  
  **vdb-dump**: using --info with URLs now works correctly  
  **vfs, sra-tools**: updated interaction with SRA Data Locator  


## SRA Toolkit 2.11.0
**March 15, 2021**

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


## SRA Toolkit 2.10.9
**December 16, 2020**

  **align, vdb**: fixed situation where network access could drastically slow down reading references  
  **build**: added configure option to produce build in output directory relative to sources  
  **fasterq-dump**: better recognizes pacbio-runs  
  **fasterq-dump**: ignore .sra-extension of input-filename in output-filename  
  **fasterq-dump**: non-zero return-code if no arguments given  
  **fastq-dump**: fasta parameter will complain about invalid argument  
  **kar**: added availability to open remote files on cloud  
  **kns, sra-tools, vdb**: added a loop to retry failed connections when fetching SRA files  
  **latf-load**: added an option to drop read names (--no-readnames), preserve them by default  
  **prefetch**: support of ETL - BQS runs  
  **sra-docker**: documentation for toolkit docker  
  **sratools**: driver tool passes all arguments to the driven tool  
  **sratools**: fixed bug that prevented the `concatenate-reads` option from working  
  **sratools**: fixed typo `split-e` for option `split-3`  
  **sratools**: tools can be executed with no arguments  
  **sratools**: transport option is now deprecated and silently ignored  
  **sratools**: verbosity argument is passed on to driven tool  
  **vdb-config**: added a new option to force use of full qualities   
  **vdb**: prefetch is used for test  
  **vfs**: allow to find local files when remote repository is disabled  
  **vfs**: not to call names.cgi but SDL when resolving runs and reference sequences  


## SRA Toolkit 2.10.8
**June 29, 2020**

  **kproc, fasterq-dump**: fixed problem with seg-faults caused by too small stack used by threads  
  **kdbmeta**: allow to work with remote runs  
  **kdb, vdb, vfs, sra-tools**: fixed bug preventing use of path to directory created by prefetch if it ends with '/'  
  **vfs, sra-tools, ngs-tools**: report an error when file was encrypted for a different ngc file  
  **prefetch**: print error message when cannot resolve reference sequence  
  **vfs, prefetch**: download encrypted phenotype files with encrypted extension  
  **vdb, sra-docker**: config can auto-generate LIBS/GUID when in a docker container  


## SRA Toolkit 2.10.7
**May 20, 2020**

  **sratools**: fixed issue with some runs not working correctly and fixed typo in fasterq-dump command line  
  **kns, sra-tools, ngs-tools**: added new header to HTTP requests to communicate VDB version


## SRA Toolkit 2.10.6
**May 18, 2020**

  **align, sra-tools, ngs-tools**: fixed fetching of reference sequences from cloud  
  **align, sra-tools, vfs**: fixed resolving of hs37d5 reference sequence  
  **build, sra-tools**: installation script works when libmagic is not present  
  **kar**: errors fixed in kar utility  
  **kfg, sra-tools**: ignore configuration with invalid protected user repository having a single 'root' node  
  **kns, sra-tools, ngs-tools**: added new header to HTTP requests to communicate SRA version  
  **kns, sra-tools**: close socket when accessing GCP files  
  **kns, sra-tools, ngs-tools**: introduced a additional configurable network retry loop  
  **krypto, sra-tools, vfs**: fixed decryption when password contains # symbol  
  **prefetch**: allow to resume interrupted download, validate downloaded file   
  **sratools**: sra-tools are now available on Windows  
  **sratools, vdb-dump, vfs**: fixed vdb-dump <accession of prefetched run>  
  **sra-tools, vdb**: restored possibility to cache WGS references to user repository  
  **sra-tools, vfs**: fixed working with runs having WGS reference sequences  


## SRA Toolkit 2.10.5
**April 1, 2020**

  **build, sratools**: fixed a potential build problem in libutf8proc  
  **ncbi-vdb, ngs, ngs-tools, sra-tools**: all Linux builds now use g++ 7.3 (C++11 ABI)  
  **prefetch**: improvements were made to work in environments with bad network connections  
  **prefetch, sratools**: fixed the names of the --min-size and --max-size command line arguments when running prefetch


## SRA Toolkit 2.10.4
**February 26, 2020**

  **kns, sra-tools:**: fixed errors when using ngc file


## SRA Toolkit 2.10.3
**February 18, 2020**

  **sraxf, fasterq-dump, fastq-dump, sam-dump**: fixed a problem resulting in a segmentation fault 


## SRA Toolkit 2.10.2
**January 15, 2020**

  **cloud, kfg, vdb-config**: added command line options for cloud configuration  
  **fasterq-dump**: fixed bug of random error at startup  
  **kfg, kns, krypto, prefetch, vdb-config, vfs**: names service URL was updated to locate.ncbi.nlm.nih.gov  
  **kfg, prefetch, vfs**: fixed possibility to prefetch karts with genotype and phenotype files  
  **latf-load**: latf-load now preserves read names  
  **prefetch**: accepts JWT cart command line argument  
  **prefetch**: accepts JWT cart command line argument plus accession filter  
  **prefetch**: fixed crash when run with --output-file option  
  **prefetch**: make sure to accept old style kart file; added --cart command line option  
  **prefetch**: the download transport has been limited to https and the eliminate-quals option has been temporarily disabled  
  **prefetch, vfs**: allow to specify file type to resolve  
  **prefetch, vfs**: allow to use "prefetch --type all" to request download of all file types  
  **sra-pileup**: printing bases inbetween slices when user-defined ref-name given fixed  
  **sra-stat**: can accept any number of reads  
  **srapath**: fixed regression when an extra vdbcache URL is printed  
  **srapath**: fixed regression when resolving protected data  
  **sratools**: add --perm option to all accessor tools  
  **sratools**: added --ngc command line argument  
  **sratools**: fastq-dump accepts both -v and --verbose to enable verbose mode  
  **sratools**: sratools accept --ngc to specify the ngc file  
  **sratools**: sratools accepts -Z for fasterq-dump  
  **sratools**: sratools was rewritten as a stand-alone binary  
  **sratools, vdb-config**: dbGaP page removed  
  **vdb, vdb-config**: GUID shown in vdb-config or created if not yet present  
  **vdb, vdb-dump**: fixed an error reporting bug  
  **vdb-config**: allow multiple saves in interactive mode  
  **vdb-config**: disabled possibility to import ngc file  
  **vdb-config**: fixed bug in file-select-dialog  
  **vdb-config**: wording of public user-repository changed to just user-repository  
  **vdb-dump**: fixed bug in -X ( hex ) mode  
  **vdb-dump, vfs**: addressed obscure bug preventing access to a single external reference  
  **vdb-validate**: fixed handling of empty cells affecting certain databases  


## SRA Toolkit 2.10.1
**December 16, 2019**

  **sra-tools**: changed version to match that of _ncbi-vdb_


## SRA Toolkit 2.10.0
**August 19, 2019**

  **bam-load**: SAM/BAM record sizes are limited only by available memory  
  **fastq-dump**: fixed help text  
  **kfg, sra-tools**: use trace.ncbi.nlm.nih.gov to call names service  
  **kfg, vdb-config**: alternative remote repository URL was added to default configuration   
  **kfs**: introduced readahead strategy for cloud storage  
  **klib, vdb**: error report is saved to ncbi_error_report.txt  
  **kns**: We now use system root CA certs on Unix   
  **kns**: introduced configurable controls over network timeouts  
  **prefetch**: fixed crash when run with --output-file option  
  **prefetch**: updated prefetch help text  
  **prefetch, srapath**: support for original submission files in cloud storage
  **prefetch, srapath, vfs**: added possibility to specify data location  
  **prefetch, vdb**: adjustments for latest name resolution service  
  **prefetch, vfs**: added support of "run accession as directory"  
  **prefetch, vfs**: added support of download of reference sequences in "run accession as directory"  
  **prefetch, vfs**: fixed regression when prefetch does not download vdbcache  
  **sra-stat**: don't print mismatch warning for old runs having a single SEQUENCE table and CMP_BASE_COUNT=BIO_BASE_COUNT while it should be 0  
  **sra-stat**: fixed a bug when processing runs having spots with first reads with length 0  
  **srapath, sratools**: Added support of "run accession as directory"  
  **sratools**: SRA tools are executed by a driver tool called sratools  
  **sratools**: installation creates symlinks to invoke some tools via a driver tool (sratools)  
  **test**: created a standard way to detect ascp availability  
  **tui, vdb-config**: new look and cloud specific options in 'vdb-config -i'  
  **vdb**: make greater use of data returned by latest name resolver  
  **vdb-validate**: referential integrity checker prints out completion message, if it printed out any progress  
  **vfs**: added possibility to set resolver version from configuration  
  **vfs**: allow to use SDL as remote service  


## SRA Toolkit 2.9.6
**March 18, 2019**

  **prefetch**, **vfs**: fixed regression that prevented re-download of incomplete files  


## SRA Toolkit 2.9.5
**March 6, 2019**

  **prefetch**: fixed regression that caused download of incomplete files  


## SRA Toolkit 2.9.4-1
**March 4, 2019**

  **sra-tools, vfs**: fixed regression introduced in 2.9.4 release thas caused delay when starting sra tools  


## SRA Toolkit 2.9.4
**January 31, 2019**

  **fasterq-dump**: improved handling of temp files in case of multiple instances  
  **fasterq-dump**: produces same output as fastq-dump on SRR000001 (empty reads)  
  **fastq-dump**: updated typo in error report  
  **sra-tools, vfs**: added support of realign objects 


## SRA Toolkit 2.9.3
**October 11, 2018**

  **kns**: added possibility to skip server's certificate validation  
  **kns**: expect to receive HTTP status 200 when sending range-request that includes the whole file  
  **vdb**: fixed a bug in accessing pagemap process request for cursors which do not have pagemap thread running 


## SRA Toolkit 2.9.2-2
**September 26, 2018**

  **read-filter-redact**: Fixed to update HISTORY metadata


## SRA Toolkit 2.9.2
**July 23, 2018**

  **kfg, vfs**: Introduced enhanced handling of download-only NGC files that lack read/decrypt permissions


## SRA Toolkit 2.9.1-2
**July 17, 2018**

  **bam-load**: fixed a bug preventing early termination on error


## SRA Toolkit 2.9.1-1
**June 26, 2018**

  **prefetch**: restored download of dependencies when running "prefetch 'local file'"


## SRA Toolkit 2.9.1
**June 15, 2018**

  **build**: 'make install' ignore ROOT environment variable  
  **build**: sra-toolkit GUI is now integrated into the regular build  
  **fasterq-dump**: a tool to dump a whole run in fastq by using a simple query engine approach  
  **kar**: Reduced memory consumption for extract operations  
  **kfg, vdb-config**: name resolver service now makes use of fcgi  
  **kfg, vfs**: Fixed a bug that prevented decryption of objects encrypted with non-UTF8 text password keys  
  **kns**: Randomly select from multiple proxies in configuration  
  **ngs-tools**: all tools now report their version correctly  
  **prefetch**: allows user to specify output file or directory  
  **prefetch**: fixed leaking of file descriptors during ascp download  
  **prefetch**: now supports download from URL  
  **prefetch**: relays error messages generated by ascp and prints them to prefetch error log.  


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

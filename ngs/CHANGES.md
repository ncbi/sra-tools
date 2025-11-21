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


### NGS SDK 3.3.0
**November 25, 2025**

  SRA toolkit release 3.3.0.


### NGS SDK 3.2.1
**March 18, 2025**

  SRA toolkit release 3.2.1.


### NGS SDK 3.2.0
**January 14, 2025**

  SRA toolkit release 3.2.0.


### NGS SDK 3.1.1
**May 21, 2024**

  SRA toolkit release 3.1.1.


### NGS SDK 3.1.0
**March 5, 2024**

  SRA toolkit release 3.1.0.


### NGS SDK 3.0.10
**December 19, 2023**

  SRA toolkit release 3.0.10.


### NGS SDK Internal Release 3.0.8
**September 19, 2023**

  SRA toolkit internal release 3.0.8.


### NGS SDK 3.0.7
**August 29, 2023**

  SRA toolkit release 3.0.7.


### NGS SDK 3.0.6
**July 10, 2023**

  SRA toolkit release 3.0.6.


### NGS SDK 3.0.5
**May 9, 2023**

  SRA toolkit release 3.0.5.


### NGS SDK 3.0.1
**November 15, 2022**

  SRA toolkit release 3.0.1.


### NGS SDK 3.0.0
**February 10, 2022**

  **ngs**: https://github.com/ncbi/ngs repository is now frozen. The NGS project has moved to https://github.com/ncbi/sra-tools/ngs.


## NGS SDK 2.11.2
**October 7, 2021**

  **ngs**: changed version to match that of _ncbi-vdb_


## NGS SDK 2.11.1
**August 17, 2021**

  **ncbi-vdb, ngs, ngs-tools, sra-tools**: configure prints the version of compiler  
  **ngs**: added information about building and running examples  


## NGS SDK 2.11.0
**March 15, 2021**

  **build, ncbi-vdb, ngs, ngs-tools**: introduced an additional external library, libncbi-ngs  
  **ncbi-vdb, ngs, ngs-tools, sra-tools, vdb**: added support for 64-bit ARM (AArch64, Apple Silicon)  


## NGS SDK 2.10.9
**December 16, 2020**

  **build**: added configure option to produce build in output directory relative to sources  
  **ngs**: created NGS release 2.10.8 Linux tarball from build of the libraries by gcc-7.3.0  


## NGS SDK 2.10.8
**June 29, 2020**

  **ngs**: changed version to match that of _ncbi-vdb_


## NGS SDK 2.10.5
**April 1, 2020**

  **ngs, ncbi-vdb, sra-tools, ngs-tools**: all Linux builds now use g++ 7.3 (C++11 ABI)


## NGS SDK 2.10.4
**February 26, 2020**

  **ngs**: changed version to match that of _ncbi-vdb_


## NGS SDK 2.10.2
**January 15, 2020**

  **ngs**: test-code is now python3-compatible  


## NGS SDK 2.10.1
**December 16, 2019**

  **ngs**: changed version to match that of _ncbi-vdb_


## NGS SDK 2.10.0
**August 19, 2019**

  **ngs**: updated for _ncbi-vdb 2.10.0_


## NGS SDK 2.9.6
**March 18, 2019**

  **ngs**: changed version to match that of _ncbi-vdb_


## NGS SDK 2.9.4
**January 31, 2019**

  **ngs, ngs-tools**: added an option to skip non-local references  


## NGS SDK 2.9.3
**October 17, 2018**

  **ngs**: changed version to match that of _ncbi-vdb_


## NGS SDK 2.9.2
**July 23, 2018**

  **ngs**: changed version to match that of _ncbi-vdb_


## NGS SDK 2.9.1
**June 15, 2018**

  **build**: 'make install' ignore ROOT environment variable  


## NGS SDK 2.9.0
**February 23, 2018**

  **version**: changed version to match that of _ncbi-vdb_
  **build**: Fixed configure allowing to run it on Perl with version >= v5.26 that has "." removed from @INC  
  **kfg**: added searching of configuration files in ../etc/ncbi/ relative to the binaries  
  **kfs**: fix to improve on windows  
  **klib**: Reverted KTimeMakeTime to use UTC  
  **kns**: Accept the same http_proxy specifications as wget  
  **kns**: Added possibility to report server's IP address after network error  
  **kns**: Ignore HTTP headers sent multiple times  
  **kns**: Improved reporting of network errors  
  **kns**: fixed generation of invalid error code in response to dropped connection  
  **ncbi-vdb**: fixed bug of directory not found on mac  
  **ncbi-vdb, ngs-engine**: improved handling of blobs inside the NGS engine   
  **ngs**: Examples for python 2.6 were removed  
  **ngs**: Python examples work with Python 2.6.6  
  **ngs-engine**: improved performance when iterating through partially aligned and unaligned reads  
  **ngs-engine**: optimized filtered access to unaligned runs  
  **vfs**: Name resolving service was updated and switched to protocol version 3.0  


## NGS SDK 1.3.0
**October 7, 2016**

### HTTPS-ENABLED RELEASE

  **build, ngs-tools**: Now ngs-tools look for its dependencies using their normal build paths and does not reconfigure them  
  **build, ngs-tools**: Now ngs-tools use CMAKE_INSTALL_PREFIX for installation path  
  **kns**: All tools and libraries now support https  
  **ngs**: Fixed all crashes when using null as string in ngs-java APIs  
  **ngs**: NGS_ReferenceGetChunk() will now return chunks potentially exceeding 5000 bases  
  **ngs**: fixed potential concurrency issues at exit, when called from Java  
  **ngs**: ngs-java and ngs-python auto-download (of native libraries) now works through HTTPS  
  **ngs**: read fragments of length 0 are now ignored  
  **ngs, ngs-tools, ref-variation**: added class ngs-vdb::VdbAlignment, featuring method IsFirst()  
  **ngs-engine**: improved diagnostic messages  
  **ngs-tools**: Fixed Makefiles to keep supporting "./configure; make" build of sra-search, alongside CMake-based build.  


## NGS SDK 1.2.5
**July 12, 2016**

  **blast, kfg, ncbi-vdb, sra-tools, vfs**: restored possibility to disable local caching  
  **htsjdk**: Several JVM crashes related to a number of open files were fixed. New property to disable auto-download was integrated into HTSJDK  
  **kfg**: When loading configuration files on Windows USERPROFILE environment variable is used before HOME  
  **ngs, search, sra-search**: sra-search was modified to support multiple threads.  
  **ngs-engine, ngs-tools, sra-tools, vfs**: The "auxiliary" nodes in configuration are now ignored  
  **ngs-engine**: Added support for blob-by-blob access to SEQUENCE table  
  **ngs-engine**: removed a potential memory leak in NGS_CursorMake()  
  **ngs**: Fixed a bug in ngs::Alignment::getMateReferenceSpec() affecting pre-March 2015 runs  
  **ngs**: now supports parallel compilation with "make -j N"  
  **vfs**: environment variable VDB_PWFILE is no longer used  


## NGS SDK 1.2.4
**May 25, 2016**

  **build**: MSVS 2013 toolset (12.0) is now supported across all repositories  
  **doc, ngs**: updated javadoc to include throws and other missing tags  
  **examples, ngs**: added DumpReferenceFASTA.py example  
  **htsjdk**: added code to HTS-JDK  to avoid involving NGS unless we are sure that it is being requested  
  **ngs, ngs-engine**: Added filtering to NGS of secondary alignments that do not have primary alignments  
  **ngs, test, ngs-python**: fixed bug in String processing for Python 3.x  
  **ngs-engine**: ncbi-ngs engine was updated - fixed a bug that made NGS read iterator return 0 reads on WGS accessions.  
  **ngs**: Improved native library load in ngs-java, enhanced its error reporting and added a mode to disable auto-download of native libraries  
  **ngs**: Python code will check for the latest version of the libraries and update if newer ones are available  
  **ngs**: check for the latest version of the libraries and update if newer ones are available  
  **ngs**: simplified ngs-python bindings  

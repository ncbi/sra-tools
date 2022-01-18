# The NCBI SRA (Sequence Read Archive)

### Contact:
email: sra@ncbi.nlm.nih.gov

### Download
Visit our [download page](https://github.com/ncbi/sra-tools/wiki/01.-Downloading-SRA-Toolkit) for pre-built binaries.

### Change Log
Please check the [CHANGES.md](CHANGES.md) file for change history.

## The SRA Toolkit
The SRA Toolkit and SDK from NCBI is a collection of tools and libraries for
using data in the INSDC Sequence Read Archives.

## How To Build From Source

To build from source, you need one of the supported operating systems (Linux, Windows, MacOS) and CMake (minimum version 3.16). On Linux and MacOS you need the GNU C/C++ toolchain (On MacOS, you may also use CMake to generate an XCode project), on Windows a set of MS Build Tools for Visual Studio 2017 or 2019.

As a prerequisite, you need a set of libraries and headers from the NCBI SRA SDK (https://github.com/ncbi/ncbi-vdb). The easiest way to build the toolkit from the sources is to checkout the SDK and the toolkit side-by side (ncbi-vdb/ and sra-tools/ under a common parent directory) and run configure/make first in ncbi-vdb/ and then in sra-tools/, as described below.

### Linux, MacOS (gmake)

0. Build or download the SDK. For instructions, see README.md in https://github.com/ncbi/ncbi-vdb

1. In the root of the sra-tools checkout, run:

```
./configure
```

Use ```./configure -h``` for the list of available optins

If the SDK is checked out side-by-side with the toolkit (i.e. ncbi-vdb/ and sra-tools/ share a common parent directory), the configuration step will locate it and set the internal variables to point to its headers and libraries. Otherwise, you would need to specify their location using the configuration option --with-ncbi-vdb-prefix=```<path-to-sdk-checkout>```

2. Once the configuration script has successfully finished, run:

```
make
```

This will invoke a Makefile that performs the following sequence:
* retrieve all the settings saved by the configuration script
* pass the settings to CMake
  * if this is the first time CMake is invoked, it will generate a project tree. The project tree will be located underneath the directory specified in the ```--build-prefix``` option of the configuration. The location can be displayed by running ```make config``` or ```make help``` inside the source tree.
  * build the CMake-generated project

Running ```make``` from any directory inside the source tree will invoke the same sequence but limit the scope of the build to the sub-tree with the current directory as the root.

The ```make``` command inside the source tree supports several additional targets; run ```make help``` for the list and short descriptions.

### MacOS (XCode)

To generate an XCode project, check out sra-tools and run the standard CMake out-of-source build. For that, run CMake GUI and point it at the checkout directory.
Add entries ```VDB_BINDIR=<path-to-sdk-build>``` and ```VDB_INCDIR=<path-to-sdk-headers>``` to the list of cache variables. Choose Xcode as the generator and click "Generate". Once the CMake generation succeeds, there will be an XCode project file ```ncbi-vdb.xcodeproj``` in the build's binary directory. You can open it with XCode and build from the IDE.

Alternatively, you can configure and build from the command line, in which case you would need to provide the 2 paths to the SDK's libraries and headers:

```
cmake <path-to-sra-tools> -G Xcode -DVDB_BINDIR=<path-to-sdk-build> -DVDB_INCDIR=<path-to-sdk-headers>
cmake --build . --config Debug      # or Release
```

### Windows (Visual Studio)

To generate an MS Visual Studio solution, check out sra-tools and run the standard CMake out-of-source build. For that, run CMake GUI and point it at the checkout directory. Add entries ```VDB_BINDIR=<path-to-sdk-build>``` and ```VDB_INCDIR=<path-to-sdk-headers>``` to the list of cache variables. Now, choose one of the supported Visual Studio generators (see NOTE below) and a 64-bit generator, and click on "Configure" and then "Generate". Once the CMake generation succeeds, there will be an MS VS solution file ```sra-tools.sln``` in the build's binary directory. You can open it with the Visual Studio and build from the IDE.

NOTE: This release supports generators ```Visual Studio 15 2017``` and ```Visual Studio 16 2019```, only for 64 bit platforms.


Alternatively, you can configure and build from the command line (assuming the correct MSVS Build Tools are in the %PATH%), in which case you would need to provide the 2 paths to the SDK's libraries and headers:

```
cmake <path-to-sra-tools> -G "Visual Studio 16 2019" -A x64 -DVDB_BINDIR=<path-to-sdk-build> -DVDB_INCDIR=<path-to-sdk-headers>
cmake --build . --config Debug      # or Release
```


### ANNOUNCEMENT:

December 9, 2021. Upcoming changes to the source build system:

NCBI’s SRA will change the source build system to use CMake in the next toolkit release, 3.0.0. This change is an important step to improve developers’ productivity as it provides unified cross platform access to support multiple build systems. This change affects developers building NCBI SRA tools from source. Old makefiles and build systems will no longer be supported after we make this change.

This change will also include the structure of GitHub repositories, which will undergo consolidation to provide an easier environment for building tools and libraries (NGS libs and dependencies are consolidated). Consolidation of NGS libraries and dependencies provides better usage scope isolation and makes building more straightforward.

### **Affected repositories**

1) ncbi/ngs (https://github.com/ncbi/ngs)

   This repository will be frozen, and all the code moved to Github repository ncbi/sra-tools, under subdirectory ngs/. All future modifications will take place in sra-tools

2) ncbi/ncbi-vdb (https://github.com/ncbi/ncbi-vdb)

   This project’s build system will be based on CMake. The libraries supporting access to VDB data via NGS API will be moved to Github repository
   ncbi/sra-tools.

The projects to move are:

| Old location (base URL: https://github.com/ncbi/ncbi-vdb) | New location (base URL: https://github.com/ncbi/sra-tools) |
| --------------------------------------------------------- | ---------------------------------------------------------- |
| libs/ngs | ngs/ncbi/ngs |
| libs/ngs-c++ | ngs/ncbi/ngs-c++ |
| libs/ngs-jni | ngs/ncbi/ngs-jni |
| libs/ngs-py | ngs/ncbi/ngs-py |
| libs/vdb-sqlite | libs/vdb-sqlite |
| test/ngs-java | test/ngs-java |
| test/ngs-python | test/ngs-python |


3) ncbi/sra-tools (https://github.com/ncbi/sra-tools)

   This project’s build system will be based on CMake. The project will acquire some new components:

      3a) NGS SDK (now under ngs/, formerly in Github repository ncbi/ngs)

      3b) NGS-related VDB access libraries and their dependents, formerly in Github repository ncbi/ncbi-vdb, as listed in the table above.


October 25, 2021. SRA Toolkit 2.11.3:
fixed a bug in fasterq-dump: fasta and fasta-unsorted parameters work correctly.

October 7, 2021. SRA Toolkit 2.11.2:

SRA data are now available either with full base quality scores (SRA Normalized Format), or with simplified quality scores (SRA Lite), depending on user preference. Both formats can be streamed on demand to the same filetypes (fastq, sam, etc.), so they are both compatible with existing workflows and applications that expect quality scores. However, the SRA Lite format is much smaller, enabling a reduction in storage footprint and data transfer times, allowing dumps to complete more rapidly. The SRA toolkit defaults to using the SRA Normalized Format that includes full, per-base quality scores, but users that do not require full base quality scores for their analysis can request the SRA Lite version to save time on their data transfers. To request the SRA Lite data when using the SRA toolkit, set the "Prefer SRA Lite files with simplified base quality scores" option on the main page of the toolkit configuration- this will instruct the tools to preferentially use the SRA Lite format when available (please be sure to use toolkit version 2.11.2 or later to access this feature). The quality scores generated from SRA Lite files will be the same for each base within a given read (quality = 30 or 3, depending on whether the Read Filter flag is set to 'pass' or 'reject'). Data in the SRA Normalized Format with full base quality scores will continue to have a .sra file extension, while the SRA Lite files have a .sralite file extension. For more information please see our [data format](https://www.ncbi.nlm.nih.gov/sra/docs/sra-data-formats/) page.

August 17, 2021: SRA Toolkit 2.11.1.

March 15, 2021: SRA Toolkit 2.11.0.

December 16, 2020: SRA Toolkit 2.10.9.

June 29, 2020: SRA Toolkit 2.10.8.

May 20, 2020: SRA Toolkit 2.10.7.

May 18, 2020: SRA Toolkit 2.10.6.

April 1, 2020: SRA Toolkit 2.10.5.

February 26, 2020: SRA Toolkit 2.10.4.

February 18, 2020: SRA Toolkit 2.10.3.

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

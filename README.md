# The NCBI SRA (Sequence Read Archive)

### Contact:
email: sra@ncbi.nlm.nih.gov

### Download
Visit our [download page](https://github.com/ncbi/sra-tools/wiki/01.-Downloading-SRA-Toolkit) for pre-built binaries.

### Change Log
Please check the [CHANGES.md](CHANGES.md) file for change history.

----

## The SRA Toolkit
The SRA Toolkit and SDK from NCBI is a collection of tools and libraries for
using data in the INSDC Sequence Read Archives.

----
_May 21, 2024_: **SRA Toolkit Release 3.1.1**

Improved prefetch error and information messages for users.

Fixed errors and warnings when building on Windows. 

----
_March 5, 2024_: **SRA Toolkit Release 3.1.0**

Using prefetch --eliminate-quals will now dowload SRA Lite data or report that a Lite version is not available.

Reduced frequency of global timeouts for cloud users.

vdb-validate will report an error if data (blob) checksums are missing.

Added support for AlmaLinux.

Fixed hanging on macOS and BSD. 

----
_December 19, 2023_: **SRA Toolkit Release 3.0.10**

Fixed a bug in using JWT with some cloud storage.

Added build support for arm64 processors.

----

_August 29, 2023_: **SRA Toolkit 3.0.7**

Updated vdb-config to improve AWS credential interface and usage by SRA Toolkit.

Fixed a bug in AWS credentials with prefetch.

Fixed a bug resulting in 'reference not found' messages for reference sequences stored inside a run.

----

_July 10, 2023_: **SRA Toolkit 3.0.6**

Prefetch now supports the latest GCP access tokens.

Fixed a bug in vdb-config for Windows users.

To ensure output of techinical reads, fasterq-dump will now automatically switch to --split-files mode if --include-technical option is used.

----

_May 9, 2023_: **SRA Toolkit 3.0.5**

Added support for PacBio to fasterq-dump.

Added features to output reference sequences to fasterq-dump.

Fixed a bug in dbGaP data access when using ngc files.

----

_January 3, 2023_ : **SRA Toolkit 3.0.3**

Fixed a regression in sra-stat.

----

_December 12, 2022_ : **SRA Toolkit 3.0.2**

Fixed 'buffer insufficient while converting string within text module' failure for prefetch on Mac.

----

_November 15, 2022_ : **SRA Toolkit 3.0.1**

Removed interactive requirement to configure SRA Toolkit.

Changes to the repository structure:

To better serve disparate groups of users, the tools/ directory of the sra-tools repository is divided into several subdirectories:
- external/ - the tools that comprise the end user facing sra-toolkit. These are the tools that are installed on a toolkit user's machine. This is the default make target
- internal/ - the tools oriented towards the toolkit's developers and NCBI-internal users
- loaders/ - the tools used in archive loading pipelines, such as the NCBI SRA
- test-tools/ - the tools used in the NCBI-internal testing of the toolkit.

The default 'make' command will now only build the external tools. To build other categories of tools, use these targets/flags:

- 'make all' - to build everything, including the test projects (located in sra-tools/test/)
- 'make BUILD_TOOLS_INTERNAL=ON' - to build the external and the internal tools
- 'make BUILD_TOOLS_LOADERS=ON' - to build the external tools and the loaders
- 'make BUILD_TOOLS_TEST_TOOLS=ON' - to build the external tools and the test tools
- 'make TOOLS_ONLY=ON' - to skip building the test projects

The build flags shown above can be combined on the same command line, for instance 'make BUILD_TOOLS_LOADERS=ON BUILD_TOOLS_INTERNAL=ON TOOLS_ONLY=ON' will build everything except the test tools and the test projects.

----

_August 4, 2022_ : Security Update

Due to updated security at NCBI, versions of the SRA Toolkit 2.9.6 and older will no longer be able to connect to the NCBI data location service. We advise impacted users to update to the [latest version](https://github.com/ncbi/sra-tools/wiki/01.-Downloading-SRA-Toolkit) of the SRA Toolkit.

----

_February 10, 2022_ : **SRA Toolkit 3.0.0**

NCBI's SRA changed the source build system to use CMake in toolkit release 3.0.0. This change is an important step to improve developers' productivity as it provides unified cross platform access to support multiple build systems. This change affects developers building NCBI SRA tools from source. Old makefiles and build systems are no longer supported.

This change also includes the structure of GitHub repositories, which underwent consolidation to provide an easier environment for building tools and libraries (NGS libs and dependencies are consolidated). Consolidation of NGS libraries and dependencies provides better usage scope isolation and makes building more straightforward.

#### **Affected repositories**

1) [ncbi/ngs](https://github.com/ncbi/ngs)

   This repository is frozen. All future development will take place in GitHub repository ncbi/sra-tools (this repository), under subdirectory `ngs/`.

2) [ncbi/ncbi-vdb](https://github.com/ncbi/ncbi-vdb)

   This project's build system is based on CMake. The libraries providing access to SRA data in VDB format via the NGS API have moved to GitHub repository
   [ncbi/sra-tools](https://github.com/ncbi/ncbi-vdb).

   | Old (base URL: https://github.com/ncbi/ncbi-vdb) | New (base URL: https://github.com/ncbi/sra-tools) |
   | -------------------------------------------------| ------------------------------------------------- |
   | `libs/ngs`        | `ngs/ncbi/ngs`     |
   | `libs/ngs-c++`    | `ngs/ncbi/ngs-c++` |
   | `libs/ngs-jni`    | `ngs/ncbi/ngs-jni` |
   | `libs/ngs-py`     | `ngs/ncbi/ngs-py`  |
   | `libs/vdb-sqlite` | `libs/vdb-sqlite`  |
   | `test/ngs-java`   | `test/ngs-java`    |
   | `test/ngs-python` | `test/ngs-python`  |


3) [ncbi/sra-tools](https://github.com/ncbi/sra-tools) (This repository)

   This project's build system is based on CMake. The project acquired some new components, as listed in the table above.

----

October 25, 2021. SRA Toolkit 2.11.3:

fixed a bug in fasterq-dump: fasta and fasta-unsorted parameters work correctly.

----

October 7, 2021. SRA Toolkit 2.11.2:

SRA data are now available either with full base quality scores (SRA Normalized Format), or with simplified quality scores (SRA Lite), depending on user preference. Both formats can be streamed on demand to the same filetypes (fastq, sam, etc.), so they are both compatible with existing workflows and applications that expect quality scores. However, the SRA Lite format is much smaller, enabling a reduction in storage footprint and data transfer times, allowing dumps to complete more rapidly. The SRA toolkit defaults to using the SRA Normalized Format that includes full, per-base quality scores, but users that do not require full base quality scores for their analysis can request the SRA Lite version to save time on their data transfers. To request the SRA Lite data when using the SRA toolkit, set the "Prefer SRA Lite files with simplified base quality scores" option on the main page of the toolkit configuration- this will instruct the tools to preferentially use the SRA Lite format when available (please be sure to use toolkit version 2.11.2 or later to access this feature). The quality scores generated from SRA Lite files will be the same for each base within a given read (quality = 30 or 3, depending on whether the Read Filter flag is set to 'pass' or 'reject'). Data in the SRA Normalized Format with full base quality scores will continue to have a .sra file extension, while the SRA Lite files have a .sralite file extension. For more information please see our [data format](https://www.ncbi.nlm.nih.gov/sra/docs/sra-data-formats/) page.

----

August 17, 2021: SRA Toolkit 2.11.1.

----

March 15, 2021: SRA Toolkit 2.11.0.

----

December 16, 2020: SRA Toolkit 2.10.9.

----

June 29, 2020: SRA Toolkit 2.10.8.

----

May 20, 2020: SRA Toolkit 2.10.7.

----

May 18, 2020: SRA Toolkit 2.10.6.

----

April 1, 2020: SRA Toolkit 2.10.5.

----

February 26, 2020: SRA Toolkit 2.10.4.

----

February 18, 2020: SRA Toolkit 2.10.3.

----

Release 2.10.2 of `sra-tools` provides access to all the **public and controlled-access dbGaP** of SRA in the AWS and GCP environments _(Linux only for this release)_. This vast archive's original submission format and SRA-formatted data can both be accessed and computed on these clouds, eliminating the need to download from NCBI FTP as well as improving performance.

The `prefetch` tool also retrieves **original submission files** in addition to ETL data for public and controlled-access dbGaP data.

----

With release 2.10.0 of `sra-tools` we have added cloud-native operation for AWS and GCP environments _(Linux only for this release)_, for use with the public SRA. `prefetch` is capable of retrieving original submission files in addition to ETL data.

----

With release 2.9.1 of `sra-tools` we have finally made available the tool `fasterq-dump`, a replacement for the much older `fastq-dump` tool. As its name implies, it runs faster, and is better suited for large-scale conversion of SRA objects into FASTQ files that are common on sites with enough disk space for temporary files. `fasterq-dump` is multi-threaded and performs bulk joins in a way that improves performance as compared to `fastq-dump`, which performs joins on a per-record basis _(and is single-threaded)_.

`fastq-dump` is still supported as it handles more corner cases than `fasterq-dump`, but it is likely to be deprecated in the future.

You can get more information about `fasterq-dump` in our Wiki at [https://github.com/ncbi/sra-tools/wiki/HowTo:-fasterq-dump](https://github.com/ncbi/sra-tools/wiki/HowTo:-fasterq-dump).

----

For additional information on using, configuring, and building the toolkit,
please visit our [wiki](https://github.com/ncbi/sra-tools/wiki)
or our web site at [NCBI](http://www.ncbi.nlm.nih.gov/Traces/sra/?view=toolkit_doc)


SRA Toolkit Development Team

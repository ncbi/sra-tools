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

April 19, 2021
SRA data is stored and freely available worldwide from Amazon Web Services (AWS)'s Open Data Sponsorship Program. The data being retrieved and delivered by the SRA Toolkit comes from AWS storage and is also free for users everywhere.

NIH has released a request for information (RFI) to solicit community feedback on new proposed Sequence Read Archive (SRA) data formats. Learn more and share your thoughts at https://go.usa.gov/xvhdr. The response deadline is July 17th, 2020. We’d encourage you all to share with your colleagues and networks, and respond if you are an SRA submitter or data user.

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

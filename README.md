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

SRA Toolkit 2.10.8
kproc, fasterq-dump: fixed problem with seg-faults caused by too small stack used by threads
kdbmeta: allow to work with remote runs
kdb, vdb, vfs, sra-tools: fixed bug preventing use of path to directory created by prefetch if it ends with '/'
vfs, sra-tools, ngs-tools: report an error when file was encrypted for a different ngc file
prefetch: print error message when cannot resolve reference sequence
vfs, prefetch: download encrypted phenotype files with encrypted extension
vdb, sra-docker: config can auto-generate LIBS/GUID when in a docker container

`fastq-dump` is still supported as it handles more corner cases than `fasterq-dump`, but it is likely to be deprecated in the future.

You can get more information about `fasterq-dump` in our Wiki at [https://github.com/ncbi/sra-tools/wiki/HowTo:-fasterq-dump](https://github.com/ncbi/sra-tools/wiki/HowTo:-fasterq-dump).

For additional information on using, configuring, and building the toolkit,
please visit our [wiki](https://github.com/ncbi/sra-tools/wiki)
or our web site at [NCBI](http://www.ncbi.nlm.nih.gov/Traces/sra/?view=toolkit_doc)


SRA Toolkit Development Team

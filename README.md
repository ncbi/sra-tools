# The NCBI SRA (Sequence Read Archive)

### Contact:
email: sra-tools@ncbi.nlm.nih.gov
or visit the [SRA Tools web site](http://ncbi.github.io/sra-tools)

### Download
Visit our [download page](https://github.com/ncbi/sra-tools/wiki/Downloads) for pre-built binaries.

### Change Log
Please check the CHANGES.md file for change history.

## The SRA Toolkit
The SRA Toolkit and SDK from NCBI is a collection of tools and libraries for
using data in the INSDC Sequence Read Archives.

Much of the data submitted these days contain alignment information, for example
in BAM, Illumina export.txt, and Complete Genomics formats. With aligned data,
NCBI uses Compression by Reference, which only stores the differences in base
pairs between sequence data and the segment it aligns to.  The process to
restore original data, for example as FastQ, requires fast access to the
reference sequences that the original data was aligned to.  NCBI recommends that
SRA users dedicate local disk space to store references downloaded from the NCBI
SRA site.  As of February 2015, the complete collection of these reference sequences
is 98 GB.  While it isn't usually necessary to download the entirety of the
reference sequences, this should give you an idea of the scale of the storage
requirement.  By default, the Toolkit will download missing reference sequences
on demand and cache them in the user's home directory.  The location of this
cache is configurable, as is whether the download is automatic or manual.

For additional information on using, configuring, and building the toolkit,
please visit our [wiki](https://github.com/ncbi/sra-tools/wiki)
or our web site at [NCBI](http://www.ncbi.nlm.nih.gov/Traces/sra/?view=toolkit_doc)


SRA Toolkit Development Team

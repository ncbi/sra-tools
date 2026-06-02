# :fontawesome-solid-file-zipper: SRA data format

!!! tip
    If you want to learn about SRA data formats in detail, please take a look at our [technical manual](https://zenodo.org/records/15677383/files/SRA-Technical-Manual-v1.1.pdf) 


The SRA Normalized Format was created to support FAIR (Findable, Accessible, Interoperable, Reusable) principles, and newer, efficiently sized SRA formats continue this support, making it easier to manipulate and analyze large datasets while also reducing file size and bandwidth requirements. Full base quality scores are not needed for many bioinformatic use cases and workflows, and data formats with simplified scores reduce the typical SRA file footprint by ~60% with commensurate reductions in transfer times when accessing the data. SRA Lite and SRA Normalized Format files are both fully accessible and stream-able using the SRA toolkit.

## SRA Normalized Format - original format with full base quality scores
This is the format provided since the inception of the SRA. It contains base calls, full base quality scores, and alignments.
This format has a .sra file extension and is available from cloud providers and via the SRA Toolkit.

## SRA Lite - smaller format with simplified quality scores
This new format contains base calls, simplified quality scores, and alignments. This format has a .sralite file extension and is available from cloud providers and NCBI via the SRA Toolkit.

Output files derived from this format contain simplified quality scores.

SRA Lite files are produced from SRA Normalized Format by assessing overall read quality and setting a per-read quality flag (Read_Filter). In the resulting files, all reads have a Read_Filter flag with value pass or reject. Importantly, it is still possible to produce fastq formatted files from SRA Lite format using the SRA toolkit. In this case, each read will have a constant quality score set to 30 for reads with Read_Filter value "pass" or 3 for reads with a value "reject".

Illumina fastq and sam/bam specifications support a quality bit that is set by the sequencing instrument and SRA Lite stores this as a "pass"/"reject" Read_Filter value. If this bit is set in the submitted fastq or bam file, the value is retained. If it is not, SRA will set a pass/reject value based on the quality score distribution within each read. Reads that have more than half of quality score values <20 are flagged "reject". Reads that begin or end with a run of more than 10 quality scores <20 are also flagged "reject". Reads that pass these quality checks are flagged "pass". When dumping data using the fastq-dump, fasterq-dump, or sam-dump utilities in the SRA toolkit, all reads are included by default. 

## cSRA - compressed aligned format

The cSRA format uses compression by reference to reduce the size of the SRA 
storage footprint. Compression by reference uses the reference sequences in the 
alignment to reduce the storage size of the SRA runs themselves. Compression by 
reference requires a reference sequence, either in the REFERENCE table within the 
cSRA or as an external VDB format file, as a method to compress the sequence data 
information.  Consequently, reading the data in a cSRA archive file may require the 
availability and download of many additional references. 


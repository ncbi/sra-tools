# :fontawesome-solid-fish-fins: SharQ

!!! note
    It is an NCBI internal tool used for processing submitted files into SRA format.

## Introduction

SharQ is a verification parser for certain flavors of FastQ formatted submissions. SharQ parser verifies the structure of file and how it corresponds to formal FastQ syntax. It also does data extraction and validation for deflines (description lines) from different known manufacturers. 

SharQ is a utility that supports and is run by the ETL pipeline in addition to other loaders.

## FastQ format

[https://en.wikipedia.org/wiki/FASTQ\_format](https://en.wikipedia.org/wiki/FASTQ_format)

  

```
@SEQ_ID
GATTTGGGGTTCAAAGCAGTATCGATCAAATAGTAAATCCATTTGTTCAACTCACAGTTT
+
!''*((((***+))%%%++)(%%%%).1***-+*''))**55CCF>>>>>>CCCCCCC65
```

  

*   Line 1 begins with a '@' character and is followed by a sequence identifier and an *optional* description (like a [FASTA](https://en.wikipedia.org/wiki/FASTA_format) title line).
*   Line 2 is the raw sequence letters.
*   Line 3 begins with a '+' character and is *optionally* followed by the same sequence identifier (and any description) again.
*   Line 4 encodes the quality values for the sequence in Line 2, and must contain the same number of symbols as letters in the sequence.

## Defline Validation

SharQ performs pattern matching against known/implemented define formats. SharQ validation allows switching between formats on the fly: one single submission can use different supported define formats. SharQ is able to process that as long as all patterns are part of processing model (known platforms).

#### Acceptable platforms and extracted fields

| Platform Name | RegEx | Extracted Fields |
| --- | --- | --- |
| BgiNew | ^\[@>+\](\\S{1,3}\\d{9}\\S{0,3})(L\\d{1})(C\\d{3})(R\\d{3})(\[\_\]?\\d{1,8})(\\S\*)(\\s+\|\[\_\|-\])(\[12345\]\|):(\[NY\]):(\\d+\|O):?(\[!-~\]\*?)(\\s+\|$)) | flowcell, lane, column, row, readNo, suffix, readNum, filterRead,  spotGroup |
| BgiOld | ^\[@>+\](\\S{1,3}\\d{9}\\S{0,3})(L\\d{1})(C\\d{3})(R\\d{3})(\[\_\]?\\d{1,8})(#\[!-~\]\*?\|)(/\[1234\]\\S\*\|)(\\s+\|$)) | flowcell, lane, column, row, readNo, spotGroup, readNum |
| IlluminaNew | ^\[@>+\](\[!-~\]+?)(\[:\_\])(\\\\d+)(\[:\_\])(\\\\d+)(\[:\_\])(\-?\\\\d+\\\\.?\\\\d\*)(\[:\_\])(\-?\\\\d+\\\\.\\\\d+\|\\\\d+)(\\\\s+\|\[\_\|-\])(\[12345\]\|):(\[NY\]):(\\\\d+\|O):?(\[!-~\]\*?)(\\\\s+\|$) | InstrumentName,  lane, tile, x-coordinate, y-coordinate, ReadNum, ReadFilter, SpotGroup |
| IlluminaNewNoPrefix | ^\[@>+\](\[!-~\]\*?)(:?)(\\\\d+)(\[:\_\])(\\\\d+)(\[:\_\])(\\\\d+)(\[:\_\])(\\\\d+)(\\\\s+\|\_)(\[12345\]\|):(\[NY\]):(\\\\d+\|O):?(\[!-~\]\*?)(\\\\s+\|$) | InstrumentName,  lane, tile, x-coordinate, y-coordinate, ReadNum, ReadFilter, SpotGroup |
| illuminaNewWithSuffix | ^\[@>+\](\[!-~\]+)(\[:\_\])(\\\\d+)(\[:\_\])(\\\\d+)(\[:\_\])(-?\\\\d+\\\\.?\\\\d\*)(\[:\_\])(-?\\\\d+\\\\.\\\\d+\|\\\\d+)(\[!-~\]+?\\\\s+\|\[!-~\]+?\[:\_\|-\])(\[12345\]\|):(\[NY\]):(\\\\d+\|O):?(\[!-~\]\*?)(\\\\s+\|$) | InstrumentName,  lane, tile, x-coordinate, y-coordinate, ReadNum, ReadFilter, SpotGroup |
| illuminaNewWithPeriods | ^\[@>+\](\[!-~\]+?)(\\\\.)(\\\\d+)(\\\\.)(\\\\d+)(\\\\.)(\\\\d+)(\\\\.)(\\\\d+)(\\\\s+\|\_)(\[12345\]\|)\\\\.(\[NY\])\\\\.(\\\\d+\|O)\\\\.?(\[!-~\]\*?)(\\\\s+\|$) | InstrumentName,  lane, tile, x-coordinate, y-coordinate, ReadNum, ReadFilter, SpotGroup |
| illuminaNewWithUnderscores | ^\[@>+\](\[!-~\]+?)(\_)(\\\\d+)(\_)(\\\\d+)(\_)(\\\\d+)(\_)(\\\\d+)(\\\\s+\|\_)(\[12345\]\|)\_(\[NY\])\_(\\\\d+\|O)\_?(\[!-~\]\*?)(\\\\s+\|$) | InstrumentName,  lane, tile, x-coordinate, y-coordinate, ReadNum, ReadFilter, SpotGroup |
| IlluminaOldWithSuffix | ^\[@>+\]?(\[!-~\]+?)(:)(\\d+)(:)(\\d+)(:)(-?\\d+\\.?\\d\*)(:)(-?\\d+\\.\\d+\|-?\\d+)(#\[!-~\]\*?\|)(/\[12345\]\[!-~\]+)(\\s+\|$) | prefix, lane, tile, x, y, spotGroup, readNum |
| IlluminaOldColon | ^\[@>+\]?(\[!-~\]+?)(:)(\\d+)(:)(\\d+)(:)(-?\\d+\\.?\\d\*)(\[-:\])(-?\\d+\\.\\d+\|-?\\d+)\_?\[012\]?(#\[!-~\]\*?\|)\\s?(/\[12345\]\|\\\\\[12345\])?(\\s+\|$) | prefix, lane, tile, x, y, spotGroup, readNum |
| IlluminaOldUnderscore | ^\[@>+\]?(\[!-~\]+?)(\_)(\\d+)(\_)(\\d+)(\_)(-?\\d+\\.?\\d\*)(\_)(-?\\d+\\.\\d+\|-?\\d+)(#\[!-~\]\*?\|)\\s?(/\[12345\]\|\\\\\[12345\])?(\\s+\|$) | prefix, lane, tile, x, y, spotGroup, readNum |
| IlluminaOldWithSuffix2 |   ^\[@>+\]?(\[!-~\]+?)(:)(\\d+)(:)(\\d+)(:)(-?\\d+\\.?\\d\*)(:)(-?\\d+\\.?\\d\*\[!-~\]+?)(#\[!-~\]\*?\|)\\s?(/\[12345\]\|\\\\\[12345\])?(\\s+\|$) | prefix, lane, tile, x, y, spotGroup, readNum |
| IlluminaOldNoPrefix | ^\[@>+\]?(\[!-~\]\*?)(:?)(\\d+)(:)(\\d+)(:)(-?\\d+\\.?\\d\*)(:)(-?\\d+\\.\\d+\|-?\\d+)(#\[!-~\]\*?\|)\\s?(/\[12345\]\|\\\\\[12345\])?(\\s+\|$) | prefix, lane, tile, x, y, spotGroup, readNum |
| illuminaNewDataGroup | ^\[@>+\](\[!-~\]+?)(\\\\s+\|\[\_\|\])(\[12345\]\|):(\[NY\]):(\\\\d+\|O):?(\[!-~\]\*?)(\\\\s+\|$) | SpotName, ReadNum, ReadFilter, SpotGroup |
| LS454 | ^\[@>+\](\[!-~\]+\_\|)(\[A-Z0-9\]{7})(\\d{2})(\[A-Z0-9\]{5})(/\[12345\])?(\\s+\|$) | prefix, dateAndHash454, region454, xy454, readNum |
| PacBio | ^\[@>+\](m\\d{5,6}\_\\d{6}\_\[!-~\]+?\_c\\d{33}\_s\\d+\_\[pX\]\\d/\\d+/?\\d\*\_?\\d\*\|m\\d{6}\_\\d{6}\_\[!-~\]+?\_c\\d{33}\_s\\d+\_\[pX\]\\d\[\|/\]\\d+\[\|/\]ccs\[!-~\]\*?)(\\s+\|$) | spotName |
| PacBio2 | ^\[@>+\](\[!-~\]\*?m\\d{5,6}\\S{0,3}\_\\d{6}\_\\d{6}\[/\_\]\\d+\[!-~\]\*?)(\\s+\|$) | spotName |
| PacBio3 | ^\[@>+\](\[!-~\]\*?m\\d{5,6}\\S{0,3}\_\\d{6}\_\\d{6}\[/\_\]\\d+/ccs\[!-~\]\*?)(\\s+\|$ | spotName |
| PacBio4 | ^\[@>+\](\[!-~\]\*?m\\d{5,6}\\S{0,3}\_\\d{6}\_\\d{6}\[/\_\]\\d+/\\d+\_\\d+\[!-~\]\*?)(\\s+\|$) | spotName |
| IonTorrent2 |  ^\[@>+\](\[A-Z0-9\]{5})(:)(\\d{1,5})(:)(\\d{1,5})(\\s+\|\[\_\|\])(\[12345\]\|):(\[NY\]):(\\d+\|O):?(\[!-~\]\*?)(\\s+\|$) | runId, row, column, readNum, filterRead, spotGroup  |
| IonTorrent | ^\[@>+\](\[A-Z0-9\]{5})(:)(\\d{1,5})(:)(\\d{1,5})(/\[12345\]\|\\\\\[12345\]\|\[LR\])?(\\s+\|$) | runId, row, column, readNum |
| Nanopore1 | \[@>+\]+?(channel\_)(\\d+)(\_read\_)?(\\d+)?(\[!-~\]\*?)(\_twodirections\|\_2d\|-2D\|\_template\|-1D\|\_complement\|-complement\|\\.1C\|\\.1T\|\\.2D)?(:\[!-~ \]+?\_ch\\d+\_file\\d+\_strand.fast5)?(\\s+\|$) | poreStart, channel, poreMid, readNo, poreEnd, poreRead, poreFile |
| Nanopore2 | \[@>+\](\[!-~\]\*?ch)(\\d+)(\_file)(\\d+)(\[!-~\]\*?)(\_twodirections\|\_2d\|-2D\|\_template\|-1D\|\_complement\|-complement\|\\.1C\|\\.1T\|\\.2D)(:\[!-~ \]+?\_ch\\d+\_file\\d+\_strand.fast5)?(\\s+\|$) | poreStart, channel, poreMid, readNo, poreEnd, poreRead, poreFile |
| Nanopore3 | \[@>+\](\[!-~\]\*?)\[: \]?(\[!-~\]+?Basecall)(\_\[12\]D\[\_0\]\*?\|\_Alignment\[\_0\]\*?\|\_Barcoding\[\_0\]\*?\|)(\_twodirections\|\_2d\|-2D\|\_template\|-1D\|\_complement\|-complement\|\\.1C\|\\.1T\|\\.2D\|)\[: \](\[!-~\]\*?)\[: \]?(\[!-~ \]+?\_ch)\_?(\\d+)(\_read\|\_file)\_?(\\d+)(\_strand\\d\*.fast5\|\_strand\\d\*.\*\|)(\\s+\|$) | poreStart, channel, poreMid, readNo, poreEnd, poreRead, poreFile |
| Nanopore3\_1 | \[@>+\](\[!-~\]+?)\[: \]?(\[!-~\]+?Basecall)(\_\[12\]D\[\_0\]\*?\|\_Alignment\[\_0\]\*?\|\_Barcoding\[\_0\]\*?\|)(\_twodirections\|\_2d\|-2D\|\_template\|-1D\|\_complement\|-complement\|\\.1C\|\\.1T\|\\.2D\|)\[: \](\[!-~\]\*?)\[: \]?(\[!-~ \]+?\_read\_)(\\d+)(\_ch\_)(\\d+)(\_strand\\d\*.fast5\|\_strand\\d\*.\*)(\\s+\|$) | poreStart, channel, poreMid, readNo, poreEnd, poreRead, poreFile |
| Nanopore4 | \[@>+\](\[!-~\]\*?\\S{8}-\\S{4}-\\S{4}-\\S{4}-\\S{12}\\S\*\[\_\]?\\d?)\[\\s+\[!-~ \]\*?\|\]$) | poreStart, channel, poreMid, readNo, poreEnd, poreRead, poreFile |
| Nanopore5 | \[@>+\](\[!-~\]\*?\[0-9a-fA-F\]{8}-\[0-9a-fA-F\]{4}-\[0-9a-fA-F\]{4}-\[0-9a-fA-F\]{4}-\[0-9a-fA-F\]{12}\_Basecall)(\_\[12\]D\[\_0\]\*?\|\_Alignment\[\_0\]\*?\|\_Barcoding\[\_0\]\*?)(\_twodirections\|\_2d\|-2D\|\_template\|-1D\|\_complement\|-complement\|\\.1C\|\\.1T\|\\.2D)\\S\*?$ | poreStart, channel, poreMid, readNo, poreEnd, poreRead, poreFile |

## Validation of read structure

SharQ performs validation of spot-read structures based on the expectation that the read represents a DNA sequence.   
Validation of DNA sequences is done via comparison against DNA sequence alphabet: "ATGCNM".  
If a particular letter does not match the sequence alphabet, SharQ discards the read..

## Validation of Quality Score values

SharQ validates that the quality score values are consistent (numerical or character representation) and fall within a range for the given platform.

## Spot Name Validation 

After consuming all input data, SharQ runs a check to validate spot name uniqueness across the whole set. 

## Error handling 

SharQ stops after the parsing errors count exceeds the threshold. The threshold is defined by a parameter (--max-err-count) with default value equal to 100.   
The following errors are recognized:

| Code | Description |
| --- | --- |
| 0 | Runtime error. |
| 10 | Number of comma-separated files in all readNPairFiles parameters is expected to be the same. |
| 11 | Input files are clustered into groups. Number of files in each groups is expected to be the same. |
| 20 | '--readTypes' parameter is expected if readNPairFiles parameters are present. |
| 30 | '--readTypes' number should match the number the number of reads. |
| 40 | Failure to find input file passed in the parameters. |
| 50 | No reads found in the file. |
| 70 | Input files have deflines from different platforms. |
| 80 | 10x input files are mixed with different types (check file names). |
| 100 | SharQ failed to parse defline. |
| 110 | FastQ read has no sequence data. |
| 111 | FastQ read has no quality scores. |
| 120 | Quality score is out of expected range. |
| 130 | Quality score length exceeds sequence length. |
| 140 | Quality score contains unexpected characters. |
| 150 | Unexpected '--readTypes' parameter values. |
| 160 | Sequence contains non-alphabetical character. |
| 170 | Collation check found duplicated spot name. |
| 180 | One of the files is shorter than the other. Use '--allowEarlyFileEnd' to allow load to finish. |
| 190 | Usupported interleaved file with orphans. |
| 200 | Failure to calculate quality score encoding. |
| 210 | Assembled spot has more than 4 reads. |

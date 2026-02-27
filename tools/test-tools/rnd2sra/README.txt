rnd2sra

The purpose of this tool "rnd2sra" is to produce artificial accessions.
These accessions can be used to test tools like "fastq-dump", "fasterq-dump", and "sra2fastq".
The accessions produced by this tool do not share the same schema as the "production" accessions produced by the loaders.
The accessions produced by this tool have the same tables and columns that "fastq-dump", "fasterq-dump", and "sra2fastq" require.
However, the schema used for the artificial accession is much simpler than the production schemas.
Specifically, artificial cSRA-accessions have simple data columns where the "production" accessions have computed columns.

This tool produces the accession in "directory-tree" form.
If the "kar'ed-up" form is needed; the accession has to be processed by the "kar"-tool.

The tool needs 2 parameters:
	- an INI-file
	- the output-path

example:	$rnd2sra simple.ini MYACC

The tool will read the details of the accession it produces from the file "simple.ini".
The tool will create an artifical accession named "MYACC" in the current directory.
The INI-file as well as the output-path can also be relative or absolute paths.

The most minimal INI-file looks like this:

----------------------------------------------------------------------------
product = flat
----------------------------------------------------------------------------

The product key is the only required key. It defines which kind of accession will be produced.
There are 3 valid values: "flat", "db", and "cSRA".

The value "flat" produces a table-only (legacy) SRA-accession like SRR000001.
The value "db" produces a database with a single SEQUENCE-table like SRR5056543.
The value "cSRA" produces a cSRA-database like SRR341578.

For the "flat" and "db" values the tool will produce 10 rows by default.
The spot layout will be TBTB ( T...technical, B...biological ) by default.
There will be a name column in the output.

The number of rows can be set with the rows key - example:
----------------------------------------------------------------------------
product = flat
rows = 100
----------------------------------------------------------------------------

A missing name column can be forced by using the with_name key - example:
----------------------------------------------------------------------------
product = flat
rows = 100
with_name = no
----------------------------------------------------------------------------
The default is: with_name = yes.
If the name column is not disabled, a random string of 25 characters
will be generated.

The length of the generated name can be changed with the name_len key -
example:
----------------------------------------------------------------------------
product = flat
name_len = 12
----------------------------------------------------------------------------

The name can also be defined by the name_pattern key - example:
----------------------------------------------------------------------------
product = flat
name_pattern = NN#_NO%_&&_$$_test
----------------------------------------------------------------------------
Every occurance of '#' is filled with an auto-increment value
Every occurance of '%' is filled with a random-value between 1 and 100
Every occurance of '$' is filled with a random-char between 'a' ... 'z'
Every occurance of '&' is filled with a random-char between 'A' ... 'Z'
Any character other than '#%$&' is unchanged.
The name_len key is ignored if the name_pattern key is used.

Different spot-layouts can be created with the layout key
'B' ... biological READ
'T' ... technical READ
example:
----------------------------------------------------------------------------
product = flat
rows = 100
layout = B70 : T5 : B50
----------------------------------------------------------------------------

The layout key can be used multiple times - example:
----------------------------------------------------------------------------
product = flat
rows = 100
layout = B70 : T5 : B50
layout = B50 : T5 : B70
layout = B80 : T5 : B60
----------------------------------------------------------------------------
The tool will randomly pick one of the used layouts for each row.

The length of each section ( 'B' or 'T' ) of the layout can be within a given
range by specifying the minimum and maximum count of bases - example:
----------------------------------------------------------------------------
product = flat
rows = 100
layout = B60-70 : T5 : B40-50
layout = T5-10 : B65-75 : T5 : B70-80
----------------------------------------------------------------------------

Spotgroups can be added with the spotgroup key - example:
----------------------------------------------------------------------------
product = flat
rows = 100
layout = B70 : T5 : B50
layout = B50-60 : T4-5 : B70
spotgroup = SG1
spotgroup = SG2
----------------------------------------------------------------------------
If no spotgroup key is used, the accession is created without spotgroups.
For each row, the tool will randomly pick one of the defined spotgroups.

The same keys are valid for the product type 'flat' and 'db'.

The random-number generator can be initialized to a custom value - example:
----------------------------------------------------------------------------
product = db
seed = 1111
----------------------------------------------------------------------------
The default value for seed is '1010101'.

The content of the INI-file can be printed for debugging - example:
----------------------------------------------------------------------------
product = db
echo = yes
----------------------------------------------------------------------------

For testing purposes, it may be necessary to introduce errors in the length
of the READ and/or QUALITY columns - example:
----------------------------------------------------------------------------
product = db
qual_offset = 1, 3
read_offset = 4, 1
----------------------------------------------------------------------------
In row number 1 the QUALITY column will be 3 Phred values longer than the
READ column.
In row number 4 the READ column will be 1 base ( 'B' or 'T' ) longer than
the QUALITY column.
These keys can only be used once per INI-file.

The use of MD5-checksums can be controlled - example:
----------------------------------------------------------------------------
product = db
checksum = MD5
----------------------------------------------------------------------------
The default value is "none", which means only CRC32 checksums are used.

The creation of metadata-nodes for base-counts can be supressed - example:
----------------------------------------------------------------------------
product = db
do_not_write_meta = yes
----------------------------------------------------------------------------
The default value is "no".


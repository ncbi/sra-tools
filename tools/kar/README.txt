That is README on delite process which will help with use 'sra_delite.sh' 

The delite process is three stage process :

1) unpacking original KAR archive, which always includes downloading it from
   remote repository
2) editing resulting database, which could include rename columns and change
   metadata
3) packing modified KAR archive with/without reduced data, testing resulting
   KAR archive with VDB-DIFF program

Contents:

I.    Script requirements, environment and configuring.
II.   Script command line.
III.  Script configuration file
IV.   Unpacking original KAR archive
V.    Editing resulting database
V|.   Exporting data
VII.  Status
VIII. Physical requirements (important, read it)
IX.   Error codes
X.    Limitations: what to expect and what to not

I.  Script requirements, environment and configuring.
=============================================================================
First step in script execution is parsing command line arguments. Some of
arguments will be interpreted imediately, for configuration. Other arguments
will be treated after environment will be set and script will start working.

Generally, script will have large comment sections, which will describe which
part of processing it belongs. You may navigate throug these section by
searching line "###<<>>###"

Script is self-configurable. That means it keeps all necessary information
for executing. User could change internal settings in two ways: provide
configuration file in standard place, so script will pick it up, or user
can use '--config' parameter in command script to set appropriate config file.
The format of config file will be described later

Script presumes that it is starts from directory where following standard VDB
utilities are located :
            kar+
            kar+meta
            vdb-lock
            vdb-unlock
            vdb-validate
            prefetch
            vdb-diff
If one of these utilities does not exists, or permissions for execution for
that utility are missed, script will exit with error message. You may alter
location of VDB utilities by exporting DELITE_BIN_DIR environment variable
which points to directory with utilities. DELITE_BIN_DIR variable has higher
priority than local utilities, however there will be a message from script
that it is using ateternate bin directory. User also can set that variable
in configuration file.

Script will require two mandatory parameters path to source KAR archive, 
which could be accession, and path to directory, which will be used as working
directory. Script will/may create different directories in working dir.


II.  Script command line 
=============================================================================
The script command line:

    sra_delite.sh action [ options ]
or
   sra_delite.sh -h | --help

Action is a word which defines a procedure which script will follow. There is
list of possible actions :

    import - script will download and/or unpack archive to working directory
    delite - script will perform DELITE on database content
    export - script will create 'delited' KAR archive
    status - script will report some status, or whatever.

Options could be different for each action, many of them will be discussed
later. There is a list of options.

    -h | --help        - prints help message.
    --accession <name> - accession name for run to delite
                         String, mandatory for 'import' action only.
    --target <path>    - path to directory, where script will put it's output.
                         String, mandatory.
    --config <path>    - path to existing configuration file.
                         String, optional.
    --schema <path>    - path to directory with schemas to use for delite
                         String, mandatory for 'delite' action only.
    --force            - flag to force process, does not matter what
    --skiptest         - flag to skip using vdb-diff to test resulting archive

III.  Script configuration file
=============================================================================
Script configuration file is a simple text file. Script can recognize following
entries in configuration file : commentaries, empty strings, translations,
excludes and environment variable definitions.

There is example of configuration file :

        ### Standard configuration file.
        ### '#'# character in beginning of line is treated as a commentary

        ### Schema traslations
        ### Syntax: translate SCHEMA_NAME OLD_VER NEW_VER
        translate NCBI:SRA:GenericFastq:consensus_nanopore        1.0     2.0
        translate NCBI:SRA:GenericFastq:sequence  1.0     2.0
        translate NCBI:SRA:GenericFastq:sequence_log_odds 1.0     2
        translate NCBI:SRA:GenericFastq:sequence_nanopore 1.0     2.0
        translate NCBI:SRA:GenericFastq:sequence_no_name  1.0     2.0

        ### Columns to drop
        ### Syntax: exclude COLUMN_NAME
        exclude QUALITY
        exclude QUALITY2

        ### Rejected platforms
        ### Syntax: reject PLATFORM_NAME ["optional platform description"]
        ## reject SRA_PLATFORM_454 "454 architecture run"
        reject SRA_PLATFORM_ABSOLID "colorspace run"

        ### Columns to skip during vdb-diff testing
        ### Syntax: diff-exclude COLUMN_NAME
        diff-exclude CLIPPED_QUALITY
        diff-exclude SAM_QUALITY

        ### Environment definition section.
        # DELITE_BIN_DIR=/panfs/pan1/trace_work/iskhakov/Tundra/KAR+TST/bin
        # USE_OWN_TEMPDIR=1

Each line of configuration file is a separate entty. Script does not support
multiline entries.

Each line of Configuration file started with character '#' will be treated
as a commentary

Empty line is a line which may contain 'space', 'tab', and/or 'newline' symbols.
Because script is using internal bas 'read' command, it will automatically trim
these character.

Line started with word 'translate' will be treated as translation definition.
It should contain exactly four words, and it's format is:

<translate> <schema_name> <old_version> <new_version>

Line started with word 'exclude' will be treated as definition of column to
drop during delite process. It should contain exactly two words, and it's 
format is:

<exclue> <column_name>

Line started with word 'reject' will be treated as definition of architecture
which should not be delited. For now it is only colorspace platform OBSOLID,
That line should contain two or three parameters, and format is:

<reject> <platform_name> ["commentary which will be shown in messages"]

Line started with word 'diff-exclude" will be treated as a name of column
which will be excluded from testing by 'vdb-diff' command. It should contain
exactly two words, and it's format is:

<diff-exclude> <column_name>

Line, which consist from single word and contains symbol '=' inside will be
treated as environment variable definition. It's format is:

<VARIABLE>=<VALUE>

NOTE, SPACES ARE NOT ALLOWED IN ENVIRONMENT VARIABLE DEFINITION

If user does not like to keep config files, he may edit script directly,
the configuration part of script is exactly the same as configuration file,
and it is located in bash function 'print_config_to_stdout()'

User can provide configuration file in two ways: with argument '--config CONFIG',
or put default config file 'sra_delite.kfg' in the directory with script.
At the first, script will try to load user defined config, next it will try 
to load default config, and if there is none of such it will use hardcoded
parameters.


IV.   Unpacking original KAR archive
=============================================================================
Action 'import' is responsible for unpacking original KAR archive. That action
requires at least two parameters, and it's syntax is following:

sra_delite.sh import [ --force ] [ --config CONFIG ] --accession ACCESSION --target TARGET

The flag --force is optional. If TARGET directory exists, script will reject
to work unless that flag is provided. In that case the old TARGET directory
and all it's content will be destroyed.

The ACCESSION parameter is accession name on existing KAR archive, which
will be resolved, and downloaded.

The TARGET parameter is a reference to directory, which will be created by
script, and the content of SOURCE KAR archive will be unpacked into it's
subdirectory 'orig', so full path of that objec will be TARGET/orig

The downloading process is performed by 'prefetch' utility, which will
resolve and download local copy of KAR archive, as a references which archive
depends on.

The unpacking process is performed by kar+ utility.

V.    Editing resulting database
=============================================================================
Action 'delite' is responsible for editing unpacked database. That action
requires at least two parameters, and it's syntax is following:

sra_delite.sh delite [ --config CONFIG ] --schema SCHEMA --target TARGET

--schema parameter of that script shows path to directory where VDB schemas
are stored. It is mandatory parameter. If You does not know where schemas
are located, try to use vdb-config utility, or ask senpai. 

--target parameter is the same as target parameter for action 'import'.

During execution that 'action' script will scan TARGET/orig directory and
perform following:

    Rename all "QUALITY" columns to "ORIGINAL_QUALITY"

    For all tables and databases, which includes columns configured fof
    dropping, will be updated schema, if there exists translation for
    that schema version.


V|.   Exporting data
=============================================================================
Action 'export' will export delited data into KAR archive and test result.
There is syntax of that command:

sra_delite.sh export [ --config CONFIG ] --target TARGET [--force] [--skiptest]

By default that command will create KAR archive with name "TARGET/new.kar".
That archive will have modified schemas and all columns, listed in configuration,
will be dropped from archive.

In regular mode, if there already exists KAR archive, script will report error
and will exit. To force script work and overwrite files, user should use
'--force' option

The resulting "TARGET/new.kar" archive will be tested in two ways. First test
will be done by 'vdb-validate' utility. That test will check structure of archive,
and consistency of schemas, it could take several minutes. The second test will
be done by 'vdb-dump' utility. That test will perform record-by-record data
comparation for both original and new KAR archives. It is longest test and can
take more than several minutes. User can skip testing by adding flag '--skiptest'.


V|I.   Status
=============================================================================
Action 'status' will display status report on targeted directory. Syntax of
that command is :

sra_delite.sh status --target TARGET


V|II.  Physical requirements
=============================================================================
The delite process is quite lightweight. All utilities used does not require
more than 1GB(usually less than 200MB) of virtual memory, and uses not
more than 50MB(usually less than 20MB) of resident memory. However, testing 
process could be both long and consume a lot of memory, our tests shows up to
10G resident memory for some runs.

The delite process is very sencitive to disk space. In default case it will
require 3X disk space than size of original SRA archive. User could estimate
necessary disk space by this formula :

    REQUIRED_SIZE= 3 * ORIGINAL_KAR_SIZE


IX.  Error codes
=============================================================================
If script failed and it will return some error code. There are three types of
errors, which script could return: 

100-122 - invalid setup or configuration
  80-89 - invalid data to process or processed
  60-73 - vdb/sra/kdv program exited with non-zero return code

There is a list of all error codes which script can return.

100 - invalid orguments, missed parameters
101 - invalid ACCESSION format
102 - parameter configuration file defined, but file does not exist
103 - error in configuration file
104 - can not stat necessary executable or has not permissions to execute
105 - can not stat file or directory
106 - directory or file exist
107 - can not delete file or directory
108 - can not copy file or directory
109 - can not create file or directory
110 - can not rename file or directory

80 - unsupported architecture
81 - try to process rejected run
82 - try to process delited run
83 - run object has not QUALITY column
84 - can not find schema for table
85 - run check failed
86 - try to kar undelited run
87 - corrupted input data
88 - invalid output KAR archive

60 - prefetch failed
61 - 'ln -s' failed
62 - kar+ failed
63 - kar+meta failed
64 - make_read_filter failed
65 - vdb-unlock failed
66 - vdb-lock failed
67 - vdb-validate failed
68 - vdb-diff failed
69 - signal handled


X.    Limitations: what to expect and what to not
=============================================================================
Not every run can be delited. There are many limitations and exceptions.

First it should be noted here that run can be delited only if it has QUALITY
column. That means, that unpacked run should have one of these two :

    RUNXXX/col/QUALITY                  /* run is a SEQUENCE table */
or
    RUNXXX/tbl/SEQUENCE/col/QUALITY     /* run is a database */

After delite process downloaded kar archive with run and unpacked it, the first
thing scritp does, is checking of existance of QUALITY directory. If script
is unable to locate that directory, it will exit with return code 83.

Next condition which run should comply with is: there should be substutute
schema defined for table which contains quality column.

User should remember that there are exists runs with two defined quality columns:
SEQUENCE/col/QUALITY and CONSENSUS/col/QUALITY. If run contains two quality
columns, there should exists substitute for schema for both.

Colorspace runs can not be delited, and it is second check which script is
doing before starting delite process. If run PLATFORM type is SRA_PLATFORM_ABSOLID
then run is colorspace, and script will exit with return code 80.
User can configure list of platforms to reject in configuration file.

Next two check are necessary to recognize non ABSOLID colorspace runs : script 
will check if run contains column CS_NATIVE, or it has physical column CSREAD.
If run do have, script will fall with return code 80.

There are two old type Illumina tables which script will not process, because
they aren't supported : NCBI:SRA:Illumina:tbl:q4 and NCBI:SRA:Illumina:tbl:q1:v2.
In the case if that type of table was detected, script will exit with exit
code 80.

TRACE type archives can not be delited and script will exit with code 80.

Delite process itself consists of several steps, during which different utility
could be involved to modify metadata, schema, and/or build read filter. After 
each step, script will try to read QUALITY from freshly editet run, and if
qualities are unreadable, script will exit with code 85
there will be 

After that, script will run standard checks as for: vdb-validate and vdb-diff to
be sure that delite process finished correctly

ENJOY

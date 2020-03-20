That is README on delite process which will help with use 'sra_delite.sh' 

The delite process is three stage process :

1) unpacking original KAR archive, which could include downloading it from
   remote repository
2) editing resulting database, which could include rename columns and change
   metadata
3) packing modified KAR archive with/without reduced data

Contents:

I.   Script requirements, environment and configuring.
II.  Script command line.
III. Script configuration file
IV.  Unpacking original KAR archive
V.   Editing resulting database

I. Script requirements, environment and configuring.
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


II. Script command line 
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

    -h | --help      - prints help message.
    --source <name>  - path to KAR archive, which could be as accesssion as
                       local path.
                       String, mandatory for 'import' action only.
    --target <path>  - path to directory, where script will put it's output.
                       String, mandatory.
    --config <path>  - path to existing configuration file.
                       String, optional.
    --schema <path>  - path to directory with schemas to use for delite
                       String, mandatory for 'delite' action only.
    --force          - flag to force process, does not matter what
    --preserve       - flag to preserve dropped columns in separated 
                       KAR archive
    --writeall       - flag to write KAR file including all columns

III. Script configuration file
=============================================================================
Script configuration file is a SHELL script, and it is loading through inclusion
That means, it should be interpretable, or scirpt will fail.

Because configuration file is a shell scipt, You may use here shell style
commentaries as add own shell code

Configuration file contains three general section: schema translation table,
list of columns to drop and rename, and common setting, like DELITE_BIN_DIR,
etc ...

The schema translation table is a set of entries which contains schema name,
old version of schem and new version of schema. It stored as a BASH array.
Example:

    TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:consensus_nanopore        1.0     2.0"
    TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence  1.0     2.0"
    TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence_log_odds 1.0     2"
    TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence_nanopore 1.0     2.0"
    TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence_no_name  1.0     2.0"
    TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Helicos:tbl:v2 1.0.4   2"

The drop column list looks exactly the same :

    DROPCOLUMNS[${#DROPCOLUMNS[*]}]=QUALITY
    DROPCOLUMNS[${#DROPCOLUMNS[*]}]=QUALITY2
    DROPCOLUMNS[${#DROPCOLUMNS[*]}]=CMP_QUALITY
    DROPCOLUMNS[${#DROPCOLUMNS[*]}]=POSITION
    DROPCOLUMNS[${#DROPCOLUMNS[*]}]=SIGNAL

And, common settings looks like:

DELITE_BIN_DIR="/panfs/pan1/trace_work/iskhakov/Tundra/KAR+TST/bin"

Anyway, if You do not like to keep config files, You may edit script directly,
the configuration part of script is exactly the same as configuration file.

NOTE, if You do not know bash shell language, please, contact author and ask
to help with modifications. Also, most of these settings could be overriden
through command line

User can provide configuration file in two ways: with argument '--config CONFIG',
or put default config file 'sra_delite.kfg' in the directory with script.
At the first, script will try to load user defined config, next it will try 
to load default config, and if there is none of such it will use hardcoded
parameters.


IV.  Unpacking original KAR archive
=============================================================================
Action 'import' is responsible for unpacking original KAR archive. That action
requires at least two parameters, and it's syntax is following:

sra_delite.sh import [ --force ] [ --config CONFIG ] --source SOURCE --target TARGET

The flag --force is optional. If TARGET directory exists, script will reject
to work unless that flag is provided. In that case the old TARGET directory
and all it's content will be destroyed.

The SOURCE parameter is reference on existing KAR archive, and it could be
both as local file and as remote. In the case of remote file, it will be
resolved and downloaded.

The TARGET parameter is a reference to directory, which will be created by
script, and the content of SOURCE KAR archive will be unpacked into it's
subdirectory 'orig', so full path of that objec will be TARGET/orig


V.   Editing resulting database
=============================================================================
Action 'delite' is responsible for editing unpacked database. That action
requires at least two parameters, and it's syntax is following:

sra_delite.sh delite [ --config CONFIG ] [ --origqual ] --schema SCHEMA --target TARGET



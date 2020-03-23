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
V|.  Exporting data
VII. Status

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
Script configuration file is a simple text file. Script can recognize following
entries in configuration file : commentaries, empty strings, translations,
excludes and environment variable definitions.

There is example of configuration file :

        ### Standard configuration file.
        ### '#'# character in beginning of line is treated as a commentary

        ### Schema traslations
        translate NCBI:SRA:GenericFastq:consensus_nanopore        1.0     2.0
        translate NCBI:SRA:GenericFastq:sequence  1.0     2.0
        translate NCBI:SRA:GenericFastq:sequence_log_odds 1.0     2
        translate NCBI:SRA:GenericFastq:sequence_nanopore 1.0     2.0

        ### Columns to drop
        exclude QUALITY
        exclude QUALITY2

        ### Environment definition section.
        # DELITE_BIN_DIR=/panfs/pan1/trace_work/iskhakov/Tundra/KAR+TST/bin

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


V|.  Exporting data
=============================================================================
Action 'export' will export delited data into KAR archive. There is syntax of
that command:

sra_delite.sh export [ --condig CONFIG ] --target TARGET --force --writeall --preserve

By default that command will create KAR archive with name "TARGET/new.kar".
That archive will have modified schemas and all columns, listed in configuration,
will be dropped from archive.

if '--writeall' option is defined, script will also created KAR file with all
columns included and will store it in "TARGET/all.kar"

if '--preserve' option is defined, scipr will create KAR file with only dropped
columns included, it will be saved in file "TARGET/preserved.kar"

In regular mode, if there already exists KAR archive, script will report error
and will exit. To force script work and overwrite files, user should use
'--force' option


V|I.  Status
=============================================================================
Action 'status' will display status report on targeted directory. Syntax of
that command is :

sra_delite.sh status --target TARGET



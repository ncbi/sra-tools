# kar

## Summary

Create, extract from, or test an archive.

## Usage

```text
kar [OPTIONS] -c|--create <Archive> -d|--directory <Directory> [Filter ...]
kar [OPTIONS] -x|--extract <Archive> -d|--directory <Directory>
kar [OPTIONS] -t|--test|--long-list <Archive>
```

## Options

| Option | Description |
|---|---|
| -c\|--create &lt;archive&gt; | Create new archive. |
| -x\|--extract &lt;archive&gt; | Extract the contents of an archive into<br>directory. |
| -t\|--test &lt;archive&gt; | Check the structural validity of an archive |
| -d\|--directory &lt;Directory&gt; | The next token on the command line is the<br>name of the directory to extract to or<br>create from |
| -f\|--force | (no parameter) this will cause the extract<br>or create to over-write existing files unless<br>they are write-protected. Without this<br>option the program will fail if the archive<br>already exists for create or the target<br>directory exists for an extract |
| -l\|--long-list | more information will be given on each file<br>in test/list mode. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |
| -Z\|--stdout | Direct output to stdout |
| --md5 | create md5sum-compatible checksum file |

## Example

```sh
To create an archive named 'example.sra' that contains the same
contents as a subdirectory 'example' of the current directory

$ kar --create example.sra --directory example

To replace an existing archive named 'example.sra' with another that contains
the same contents as a subdirectory 'example' of the current directory

$ kar -f -c example.sra -d example

To examine in detail the contents of an archive named 'example.sra'

$ kar --long-list --test example.sra

To extract the files from an archive named 'example.sra' into
a subdirectory 'example' of the current directory.
NOTE: all extracted files will be read only.

$ kar --extract example.sra --directory example
```

## Archive Command

All of these options require the next token on the command line to be
the name of the archive

## Archive

Path to a file that will/does hold the archive of other files.
This can be a full or relative path.

## Directory

Required for create or extract command, ignored for test command.
This can be a full or relative path.

## Filters

When present these act as include filters.
Any file name will be included in the extracted files, created archive
or test operation listing
Any directory will be included as well as its contents

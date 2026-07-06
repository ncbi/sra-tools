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
| `-c`\|`--create <archive>` | Create new archive. |
| `-x`\|`--extract <archive>` | Extract the contents of an archive into directory. |
| `-t`\|`--test <archive>` | Check the structural validity of an archive |
| `-d`\|`--directory <Directory>` | The next token on the command line is the name of the directory to extract to or create from |
| `-f`\|`--force` | (no parameter) this will cause the extract or create to over-write existing files unless they are write-protected. Without this option the program will fail if the archive already exists for create or the target directory exists for an extract |
| `-l`\|`--long-list` | more information will be given on each file in test/list mode. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |
| `-Z`\|`--stdout` | Direct output to stdout |
| `--md5` | create md5sum-compatible checksum file |

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

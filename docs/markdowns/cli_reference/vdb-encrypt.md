# vdb-encrypt

## Summary

Encrypt a file or all the files (recursively) in a directory

## Usage

```text
vdb-encrypt [options] <source-file>
vdb-encrypt [options] <source-file> <destination-file>
vdb-encrypt [options] <source-file> <destination-directory>
vdb-encrypt [options] <directory>
```

## Options

| Option | Description |
|---|---|
| `-f`\|`--force` | Force overwrite of existing files |
| `--ngc <PATH>` | PATH to ngc file |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Parameters

- `source-file`: file to encrypt
- `destination-file`: name of resulting file
- `destination-directory`: directory of resulting file
- `directory`: directory to encrypt

## Details

All encryptions are non-destructive until successful. No files are deleted or
replaced until the encryptions are complete.

The extension '.ncbi_enc' will be added when a file is encrypted.

NCBI Archive files that contain NCBI database objects will not have the
.ncbi_enc extension added.

If the only parameter is a file name then it will be replaced by a file that
is encrypted with a possible changed extension.

If the only parameter is a directory, all files in that directory including
all files in subdirectories will be replaced with a possible change
in the extension.

If there are two parameters  a copy is made but the copy will be encrypted.
If the second parameter is a directory the new file might have a different
extension. If it is not a directory, the extension will be as given in the
the parameter.

Missing directories in the destination path will be created.

Already existing destination files will cause the program to end with
an error and will be left unchanged unless the --force option is used to
force the files to be overwritten.

## Encryption key (file password)

The encryption key or file password is handled by configuration. If not yet
set, this program will fail.

Please consult configuration page at
https://trace.ncbi.nlm.nih.gov/Traces/sra/sra.cgi?view=toolkit_doc&amp;f=std or
https://github.com/ncbi/sra-tools/wiki/Toolkit-Configuration

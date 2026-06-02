# copycat

## Summary

Copies files and/or directories, creating a catalog of the copied files.
-x|--cache-dir &lt;dir-path&gt;        location of output cached files
-f|--force                       force overwrite of existing files
-o|--output &lt;file-path&gt;          location of output
-e|--extract &lt;dir-path&gt;          location of extracted files
-E|--extract-to-dir              extracted directories match normal XML
-X|--xml-dir                     XML matches extracted files
--input-buffer &lt;size-in-KB&gt;      system file reads are of blocks of this size
--output-buffer &lt;size-in-KB&gt;     system file writes are of blocks of this<br>size
--no-bzip2                       do not decompress files compressed with<br>bzip2
--no-md5                         do not calculate md5 hashes
-h|--help                        Output brief explanation for the program.
-V|--version                     Display the version of the program then<br>quit.
-L|--log-level &lt;level&gt;           Logging level as number or enum string. One<br>of (fatal|sys|int|err|warn|info|debug) or
(0-6) Current/default is warn.
-v|--verbose                     Increase the verbosity of the program<br>status messages. Use multiple times for more
verbosity. Negates quiet.
-q|--quiet                       Turn off all status messages for the<br>program. Negated by verbose.
--option-file &lt;file&gt;             Read more options and parameters from the<br>file.

## Usage

```text
copycat [options] src-file dst-file
copycat [options] src-file [src-file...] dst-dir
copycat [options] -o dst-dir src-file [src-file...]
```

## Use

Copy and catalog:<br>Some quick examples:<br>copycat dir/file.tar other-dir/file.tar<br>copy file.tar from dir to other-dir and write the catalog to stdout
copycat dir/file.tar otherdir/<br>the same
copycat "ncbi-file:dir/file.tar.nenc?encrypt&amp;pwfile=pw other-dir.file.tar<br>copy and decrypt file.tar.nenc from dir to other-dir and catalog
copycat dir/file.tar "ncbi-file:other-dir/file.tar.nenc?encrypt&amp;pwfile=pw<br>copy and encrypt file.tar from dir to other-dir/file.tar.nenc and catalog
copycat "ncbi-file:dir/file.tar.nenc?encrypt&amp;pwfile=pw1 \<br>"ncbi-file:other-dir/file.tar.nenc?encrypt&amp;pwfile=pw2
copy the file as above while changing the encryption

## Use

Copy source file[s] to a destination file or directory.
File names can either be typical path names or they can be URLs (IRLs) using
the standard "file" or extended "ncbi-file" schemes.
The catalog is XML output sent by default to stdout.
As UTF-8 is accepted in the paths they are IRLs for International Resource
Locators.

If the specified destination does not exist, there could be an ambiguity
- `whether the destination is supposed to be a file or directory.`: If the<br>entered path ends in a '/' character or if there is more than one source
- `it is assumed to mean a directory and is created as such.`: If neither of<br>of those apply it is assumed to be a file.

The sources or destination may also be special Unix devices:<br>/dev/stdin is supported as a source.
/dev/stdout and /dev/stderr is supported as a destination.
Other file descriptor devices can be used in the form:<br>/dev/fd/&lt;fd-number&gt;
For example /dev/stdin is synonymous with /dev/fd/0 as a source.
If /dev/stdout or /dev/fd/1 is used as the destination then the XML
output is redirected to /dev/stderr (/dev/fd/2).
Device /dev/null as the destination is treated as a file with only one
- `source but as a directory if more than one source.`: Using this device<br>means no actual file will be copied but the cataloging will be done but<br>xml-base-node must be used.

These special devices can be entered using the URL (IRL) schemes if
- `desired.`: This allows the use of 'query' decorators.

If a query is added to the URL it will need to be enclosed within '"'
characters on a command line to prevent premature interpretation.
The query for the 'ncbi-file' extension to the 'file' scheme allows
- `encryption and decryption.`: The supported query is introduced by the<br>standard URI/IRI syntax of a '?' character with a '&amp;' character<br>separating individual query-entries.

The supported query entries are:<br>'encrypt' or 'enc' to mean the input may be encrypted or the output<br>will be encrypted,
'pwfile=&lt;path&gt;' gives the path to file containing the password.
'pwfd=&lt;FD&gt;' gives the numerical file descriptor from which to read<br>the password,

In this program the encrypted input can apply to a file contained within
- `the source rather than just the source file itself.`: The tool is fully<br>compatible with all versions of NCBI encryption.

If the output is to be encrypted only the newer FIPS compliant encryption
will be used and applies to the whole file.

## Note

Not all combinations of URL specifications will work at this point.

## Note

using the same file descriptor for multiple sources or overlapping with<br>stdin/stdout/stderr may cause undefined behavior including hanging the
the program.

The '-x' option allows small files that are typed as eligible for
caching to be copied to the cache directory provided. the directory
will be created if necessary.
the intent is to capture top-level files, such that files are copied
into the flat cache directory without regard to where they were found
in the input hierarchy. in the case of name conflict, output files will
be renamed.

To prevent internal decompression of bzipped files, use the option<br>'--no-bzip2'

To prevent calculation of MD5 hashes, use the option<br>'--no-md5'

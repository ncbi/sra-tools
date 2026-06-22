# prefetch

## Summary

Retrieve data and pre-requisites from NCBI or other cloud instances of the SRA data.

## Usage

```text
prefetch [options] <SRA accession> [...]
Download SRA files and their dependencies

prefetch [options] --perm <JWT cart file> <SRA accession> [...]
Download SRA files and their dependencies from JWT cart

prefetch [options] --cart <kart file>
Download cart file

prefetch [options] <URL> --output-file <FILE>
Download URL to FILE

prefetch [options] <URL> [...] --output-directory <DIRECTORY>
Download URL or URL-s to DIRECTORY

prefetch [options] <SRA file> [...]
Check SRA file for missed dependencies and download them
```

## Options

| Option | Description |
|---|---|
| `-T`\|`--type <value>` | Specify file type to download. Default: sra |
| `-t`\|`--transport <http`\|`fasp`\|`both>` | Transport: one of: fasp; http; both [default]. (fasp only; http only; first try fasp (ascp), use http if cannot download using fasp). |
| `--location <value>` | Location of data. |
| `-N`\|`--min-size <size>` | Minimum file size to download in KB (inclusive). |
| `-X`\|`--max-size <size>` | Maximum file size to download in KB (exclusive). Default: 20G |
| `-f`\|`--force <yes`\|`no`\|`all`\|`ALL>` | Force object download: one of: no, yes, all, ALL. no [default]: skip download if the object if found and complete; yes: download it even if it is found and is complete; all: ignore lock files (stale locks or it is being downloaded by another process use at your own risk!); ALL: ignore lock files, restart download from beginning. |
| `-r`\|`--resume <yes`\|`no>` | Resume partial downloads: one of: no, yes [default]. |
| `-C`\|`--verify <yes`\|`no>` | Verify after download: one of: no, yes [default]. |
| `-p`\|`--progress` | Show progress. |
| `-H`\|`--heartbeat <value>` | Time period in minutes to display download progress. (0: no progress), default: 1 |
| `--eliminate-quals` | Download SRA Lite files with simplified base quality scores, or fail if not available. |
| `-c`\|`--check-all` | Double-check all refseqs. |
| `-S`\|`--check-rs <yes`\|`no`\|`smart>` | Check for refseqs in downloaded files: one of: no, yes, smart [default]. Smart: skip check for large encrypted non-sra files. |
| `-l`\|`--list` | List the content of kart file. |
| `-n`\|`--numbered-list` | List the content of kart file with kart row numbers. |
| `-s`\|`--list-sizes` | List the content of kart file with target file sizes. |
| `-o`\|`--order <kart`\|`size>` | Kart prefetch order when downloading kart: one of: kart, size. (in kart order, by file size: smallest first), default: size. |
| `-R`\|`--rows <rows>` | Kart rows to download (default all). Row list should be ordered. |
| `--perm <PATH>` | PATH to jwt cart file. |
| `--ngc <PATH>` | PATH to ngc file. |
| `--cart <PATH>` | To read kart file. |
| `-a`\|`--ascp-path <ascp-binary`\|`private-key-file>` | Path to ascp program and private key file (aspera_tokenauth_id_rsa) |
| `--ascp-options <value>` | Arbitrary options to pass to ascp command line. |
| `-O`\|`--output-directory <DIRECTORY>` | Save files to DIRECTORY/ |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

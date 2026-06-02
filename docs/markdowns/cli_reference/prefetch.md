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
| -T\|--type &lt;value&gt; | Specify file type to download. Default: sra |
| -t\|--transport &lt;http\|fasp\|both&gt; | Transport: one of: fasp; http; both<br>[default]. (fasp only; http only; first try<br>fasp (ascp), use http if cannot download<br>using fasp). |
| --location &lt;value&gt; | Location of data. |
| -N\|--min-size &lt;size&gt; | Minimum file size to download in KB<br>(inclusive). |
| -X\|--max-size &lt;size&gt; | Maximum file size to download in KB<br>(exclusive). Default: 20G |
| -f\|--force &lt;yes\|no\|all\|ALL&gt; | Force object download: one of: no, yes,<br>all, ALL. no [default]: skip download if the<br>object if found and complete; yes: download<br>it even if it is found and is complete; all:<br>ignore lock files (stale locks or it is<br>being downloaded by another process use<br>at your own risk!); ALL: ignore lock files,<br>restart download from beginning. |
| -r\|--resume &lt;yes\|no&gt; | Resume partial downloads: one of: no, yes<br>[default]. |
| -C\|--verify &lt;yes\|no&gt; | Verify after download: one of: no, yes<br>[default]. |
| -p\|--progress | Show progress. |
| -H\|--heartbeat &lt;value&gt; | Time period in minutes to display download<br>progress. (0: no progress), default: 1 |
| --eliminate-quals | Download SRA Lite files with simplified<br>base quality scores, or fail if not<br>available. |
| -c\|--check-all | Double-check all refseqs. |
| -S\|--check-rs &lt;yes\|no\|smart&gt; | Check for refseqs in downloaded files: one<br>of: no, yes, smart [default]. Smart: skip<br>check for large encrypted non-sra files. |
| -l\|--list | List the content of kart file. |
| -n\|--numbered-list | List the content of kart file with kart<br>row numbers. |
| -s\|--list-sizes | List the content of kart file with target<br>file sizes. |
| -o\|--order &lt;kart\|size&gt; | Kart prefetch order when downloading<br>kart: one of: kart, size. (in kart order, by<br>file size: smallest first), default: size. |
| -R\|--rows &lt;rows&gt; | Kart rows to download (default all). Row<br>list should be ordered. |
| --perm &lt;PATH&gt; | PATH to jwt cart file. |
| --ngc &lt;PATH&gt; | PATH to ngc file. |
| --cart &lt;PATH&gt; | To read kart file. |
| -a\|--ascp-path &lt;ascp-binary\|private-key-file&gt; | Path to ascp program and<br>private key file (aspera_tokenauth_id_rsa) |
| --ascp-options &lt;value&gt; | Arbitrary options to pass to ascp command<br>line. |
| -O\|--output-directory &lt;DIRECTORY&gt; | Save files to DIRECTORY/ |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

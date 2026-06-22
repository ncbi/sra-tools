# meta-repair

## Options

| Option | Description |
|---|---|
| `-m`\|`--mode <check`\|`fix`\|`check-and-fix>` | Run mode: required. |
| `-i`\|`--input <acc`\|`dir`\|`file>` | Input: required. Directory will be updated. |
| `-F`\|`--fix-file <path>` | File with fix data. |
| `-o`\|`--output-file <file>` | Create output as kar arhive. |
| `-O`\|`--output-directory <directory>` | Create output as unkared directory. |
| `-f`\|`--force` | Forces an existing target to be overwritten. |
| `-u`\|`--update` | Confirm update of input directory. |
| `-t`\|`--temp <path>` | Where to put temporary files. Default is current directory. |
| `-C`\|`--verify` | Run sra-stat on output targets. |
| `-I`\|`--info` | Print report for all fields examined for mismatch even if the old value is correct. |
| `-P`\|`--print-output` | Print output of executed commands. |
| `--dryrun` | Dry run the application: don't execute commands. |
| `-z`\|`--xml-log <logfile>` | Produce XML-formatted log file. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

## Example

```sh
  To generate a fix file named 'fix.file' for an input archive 'SRR1215779':
     if exit code is 0 - fix is not needed;
     if exit code is not 0 - fix is needed or failure.
    If fix is needed - returned rc is 'data unequal while validating data'.

  $ meta-repair --mode check -i SRR1215779 > fix.file 2>&1


  To replace an archive named 'SRR1215779' with fix file named 'fix.file' as file 'SRR1215779.sra'

  $ meta-repair --mode fix -i SRR1215779 -F fix.file -o SRR1215779.sra


  You can pipe input file as:

  $ cat fix.file | meta-repair --mode fix -i SRR1215779 -F - -o SRR1215779.sra


Use --info to report (field, old value, correct value) for all fields examined even if the old value is correct.
```

## Binaries needed

- for check mode:
-  sra-stat
- for fix mode:
-  kar
-  kdbmeta
-  prefetch
-  vdb-lock
-  vdb-unlock

# meta-repair

## Options

| Option | Description |
|---|---|
| -m\|--mode &lt;check\|fix\|check-and-fix&gt; | Run mode: required. |
| -i\|--input &lt;acc\|dir\|file&gt; | Input: required. Directory will be updated. |
| -F\|--fix-file &lt;path&gt; | File with fix data. |
| -o\|--output-file &lt;file&gt; | Create output as kar arhive. |
| -O\|--output-directory &lt;directory&gt; | Create output as unkared directory. |
| -f\|--force | Forces an existing target to be overwritten. |
| -u\|--update | Confirm update of input directory. |
| -t\|--temp &lt;path&gt; | Where to put temporary files. Default is<br>current directory. |
| -C\|--verify | Run sra-stat on output targets. |
| -I\|--info | Print report for all fields examined for<br>mismatch even if the old value is correct. |
| -P\|--print-output | Print output of executed commands. |
| --dryrun | Dry run the application: don't execute<br>commands. |
| -z\|--xml-log &lt;logfile&gt; | Produce XML-formatted log file. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

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

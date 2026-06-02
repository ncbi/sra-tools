# vdb-config

## Summary

Manage VDB configuration

## Usage

```text
vdb-config [options] [<query> ...]
```

## Options

| Option | Description |
|---|---|
| -a\|--all | Print all information [default]. |
| -p\|--cfg | Print current configuration. |
| -f\|--files | Print loaded files. |
| -d\|--load-path | Print load path. |
| -e\|--env | Print shell variables. |
| -m\|--modules | Print external modules. |
| -s\|--set &lt;name=value&gt; | Set configuration node value. |
| -i\|--interactive | Create/update configuration. |
| --interactive-mode &lt;mode&gt; | Interactive mode: 'textual' or 'graphical'<br>(default). |
| --restore-defaults | Create default or update existing user<br>configuration. |
| --ignore-protected-repositories | Stop printing warning message when<br>protected repository is found. |
| -o\|--output &lt;x \| n&gt; | Output type: one of (x n), where 'x' is xml<br>(default), 'n' is native. |
| -Q\|--simplified-quality-scores &lt;yes \| no&gt; | yes: Prefer SRA Lite files with<br>simplified base quality scores if<br>available. no: Prefer SRA Normalized Format<br>files with full base quality scores if<br>available. Default: no. |
| -C\|--cloud-info | Display cloud-releated information. |
| --report-cloud-identity &lt;yes \| no&gt; | Give permission to report cloud instance<br>identity. |
| --accept-aws-charges &lt;yes \| no&gt; | Agree to accept charges for AWS usage. |
| --set-aws-credentials &lt;path&gt; | Select file with AWS credentials. |
| --set-aws-profile &lt;profile&gt; | Set AWS profile. |
| --accept-gcp-charges &lt;yes \| no&gt; | Agree to accept charges for GCP usage. |
| --set-gcp-credentials &lt;path&gt; | Select file with GCP credentials. |
| --prefetch-to-cwd | Prefetch downloads to current directory<br>when public user repository is set<br>(default: false). |
| --prefetch-to-user-repo | Prefetch downloads to public user<br>repository when it is set (default). |
| --proxy &lt;uri[:port]&gt; | Set HTTP proxy server configuration. |
| --proxy-disable &lt;yes \| no&gt; | Enable/disable using HTTP proxy. |
| --cfg-dir &lt;path&gt; | Set directory to load configuration. |
| --root | Enforce configuration update while being<br>run by superuser. |
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

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
| `-a`\|`--all` | Print all information [default]. |
| `-p`\|`--cfg` | Print current configuration. |
| `-f`\|`--files` | Print loaded files. |
| `-d`\|`--load-path` | Print load path. |
| `-e`\|`--env` | Print shell variables. |
| `-m`\|`--modules` | Print external modules. |
| `-s`\|`--set <name=value>` | Set configuration node value. |
| `-i`\|`--interactive` | Create/update configuration. |
| `--interactive-mode <mode>` | Interactive mode: 'textual' or 'graphical' (default). |
| `--restore-defaults` | Create default or update existing user configuration. |
| `--ignore-protected-repositories` | Stop printing warning message when protected repository is found. |
| `-o`\|`--output <x `\|` n>` | Output type: one of (x n), where 'x' is xml (default), 'n' is native. |
| `-Q`\|`--simplified-quality-scores <yes `\|` no>` | yes: Prefer SRA Lite files with simplified base quality scores if available. no: Prefer SRA Normalized Format files with full base quality scores if available. Default: no. |
| `-C`\|`--cloud-info` | Display cloud-releated information. |
| `--report-cloud-identity <yes `\|` no>` | Give permission to report cloud instance identity. |
| `--accept-aws-charges <yes `\|` no>` | Agree to accept charges for AWS usage. |
| `--set-aws-credentials <path>` | Select file with AWS credentials. |
| `--set-aws-profile <profile>` | Set AWS profile. |
| `--accept-gcp-charges <yes `\|` no>` | Agree to accept charges for GCP usage. |
| `--set-gcp-credentials <path>` | Select file with GCP credentials. |
| `--prefetch-to-cwd` | Prefetch downloads to current directory when public user repository is set (default: false). |
| `--prefetch-to-user-repo` | Prefetch downloads to public user repository when it is set (default). |
| `--proxy <uri[:port]>` | Set HTTP proxy server configuration. |
| `--proxy-disable <yes `\|` no>` | Enable/disable using HTTP proxy. |
| `--cfg-dir <path>` | Set directory to load configuration. |
| `--root` | Enforce configuration update while being run by superuser. |
| `-h`\|`--help` | Output brief explanation for the program. |
| `-V`\|`--version` | Display the version of the program then quit. |
| `-L`\|`--log-level <level>` | Logging level as number or enum string. One of (fatal\|sys\|int\|err\|warn\|info\|debug) or (0-6) Current/default is warn. |
| `-v`\|`--verbose` | Increase the verbosity of the program status messages. Use multiple times for more verbosity. Negates quiet. |
| `-q`\|`--quiet` | Turn off all status messages for the program. Negated by verbose. |
| `--option-file <file>` | Read more options and parameters from the file. |

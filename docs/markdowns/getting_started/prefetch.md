# prefetch

!!! note
    - `prefetch` does NOT download fastq files directly. It downloads the `.sra` files which can be then converted to fastq using `fasterq-dump`. 
    - It is highly recommended that you run `prefetch <acc>` before `fasterq-dump <acc>`. It prevents any truncated downloads and the overall workflow will be faster.

<!-- termynal -->
``` sh
$ prefetch SRR000123
2026-05-19T03:30:28 prefetch.3.4.1: 1) Resolving 'SRR000123'...
2026-05-19T03:30:28 prefetch.3.4.1: Current preference is set to retrieve SRA Normalized Format files with full base quality scores
2026-05-19T03:30:28 prefetch.3.4.1: 1) Downloading 'SRR000123.lite'...
2026-05-19T03:30:28 prefetch.3.4.1:  SRA Lite file is being retrieved due to current file availability
2026-05-19T03:30:28 prefetch.3.4.1:  Downloading via HTTPS...
2026-05-19T03:30:28 prefetch.3.4.1:  HTTPS download succeed
2026-05-19T03:30:28 prefetch.3.4.1:  'SRR000123.lite' is valid: 2248862 bytes were streamed from 2247980
2026-05-19T03:30:28 prefetch.3.4.1: 1) 'SRR000123.lite' was downloaded successfully
```


## Internet requirement

The `prefetch` tool downloads all necessary files to your computer. The `prefetch` - tool can be invoked multiple times if the download did not succeed. It will not start from the beginning every time; instead, it will pick up from where the last invocation failed.

After the successful download, there is no need for network-connectivity. You can move the folder created by `prefetch` to a different location to perform the conversion to the fastq-format somewhere else (for instance to a compute-cluster without internet access).

## Location of the downloaded sra files

The `prefetch`-tool downloads to a directory named by accession. E.g. `prefetch SRR000001` will create a directory named `SRR000001` in the current directory. Make sure that if you move the `SRR000001` directory, you don't rename it as the conversion-tool will need to find the original directory.

The location of the downloaded sra files depend on the configuration of the toolkit. There are 3 options:

1. In the current working directory:

``` sh
vdb-config --prefetch-to-cwd
```

2. In the user-repository

``` sh
vdb-config --prefetch-to-user-repo
```

3. user-defined location

``` sh
prefetch SRR000001 -O /path/to/be/used
```

## Download size limit
The `prefetch`-tool has a default maximum download-size of `20G`. If the requested accession is bigger than `20G`, you will need to increase that limit. You can specify an extremely high limit no matter how large the requested accession is. You can also query the accession-size using the `vdb-dump`-tool and the `--info` option. For instance, `vdb-dump SRR000001 --info` tells you how large this accession is ( among other information ). The accession `SRR000001` has `932,308,473` bytes, which is below the default limit, so no further action is necessary. The accession `SRR1951777` has `410,112,373,995` bytes. To download this accession you have to lift the limit above that size:
* `$prefetch SRR1951777 --max-size 420000000000`

You can specify the limit in:
* kilobytes (default): --max-size 10 == --max-size 10k : 10 kilobytes,
* megabytes: --max-size 10m : 10 megabytes,
* gigabytes: --max-size 10g : 10 gigabytes,
* terabytes: --max-size 10t : 10 terabytes,
* unlimited: --max-size u.



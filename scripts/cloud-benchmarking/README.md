# Cloud benchmarking

## Building the test docker image.

For example:
```
docker build --target aws -t benchmarking:aws .
```

## Running the tests.

For best results:
- Mount two directories into the container.
- These directories should be on different devices.
- One directory will be used as the current working directory inside the container. This is where files will be downloaded or generated.
- The other directory will be used for `tmpfs`.
- Use 8 CPUs. (More may not be better.)

```
docker run --cpus 8 -v ${BIG_DEVICE}:/output:rw --tmpfs ${FAST_DEVICE} -w /output --rm -it benchmarking:aws
```

This will run the benchmarking using the two default accessions and the AWS tool.

## Troubleshooting:

#### `fasterq-dump` complains about disk space.

Give it a bigger disk.

#### The script complains about not being able to make a cloud native URL.

The data you are requesting is not available from your location. For example:
- You are running the `gcp` version but the data is on S3.
- You are running the `aws` version but the data is at NCBI.

Change your location or change which version you are running.

## Purpose:

To compare the toolkit against other means of getting the _same_ result:
- Cloud native tools vs `prefetch` for data retrieval.
- `fasterq-dump` vs downloading fastq.
- `fasterq-dump` vs `prefetch`+`fasterq-dump`.

## Methodology:

Gather timing information for:

1. Download using cloud native tool from cloud native bucket.
   - `aws s3 cp` for data on S3.
   - `gsutil cp` for data on GCP.
2. Query EBI and download fastq.
3. Generate fastq with the toolkit from an SRA file over the network.
4. Download using the toolkit and generate fastq from the downloaded SRA file.

Each step is run in a temporary directory which is deleted after the step is complete.

## Components:

### `config.aws.pl`:

- Contains `aws` specific configuration information.
- Gets copied into the Docker image as `config.pl` by the `aws` target.

### `config.gsutil.pl`:

- Contains `gsutil` specific configuration information.
- Gets copied into the Docker image as `config.pl` by the `gcp` target.

### `Dockerfile`:
- Based on sra-tools 3.0.0 Docker image.
- Can build AWS and GCP targets.
    - `--target aws`
    - `--target gcp`

### `README.md`:

This file.

### `run-test`:

A PERL script that runs the tests.
- Gets copied into the Docker image.
- _Includes_ the configuration file `config.pl`, which controls what _cloud native_ means to the script.
    - What a cloud URL should look like?
    - What tool to use?
    - How to call that tool?

### `test.sh`:

A simple shell script with an example invocation of the result.

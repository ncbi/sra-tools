/* Tool arguments definitions
 * auto-generated
 */

#define TOOL_NAME_VDB_DUMP "vdb-dump" /* from argv[0] */
#define TOOL_ARGS_VDB_DUMP TOOL_ARGS ( \
    TOOL_ARG("row_id_on", "I", false, TOOL_HELP("print row id", 0)), \
    TOOL_ARG("line_feed", "l", true, TOOL_HELP("line-feed's inbetween rows", 0)), \
    TOOL_ARG("colname_off", "N", false, TOOL_HELP("do not print column-names", 0)), \
    TOOL_ARG("in_hex", "X", false, TOOL_HELP("print numbers in hex", 0)), \
    TOOL_ARG("table", "T", true, TOOL_HELP("table-name", 0)), \
    TOOL_ARG("rows", "R", true, TOOL_HELP("rows (default = all)", 0)), \
    TOOL_ARG("columns", "C", true, TOOL_HELP("columns (default = all)", 0)), \
    TOOL_ARG("schema", "S", true, TOOL_HELP("schema-name", 0)), \
    TOOL_ARG("schema_dump", "A", false, TOOL_HELP("dumps the schema", 0)), \
    TOOL_ARG("table_enum", "E", false, TOOL_HELP("enumerates tables", 0)), \
    TOOL_ARG("column_enum", "O", false, TOOL_HELP("enumerates columns in extended form", 0)), \
    TOOL_ARG("column_enum_short", "o", false, TOOL_HELP("enumerates columns in short form", 0)), \
    TOOL_ARG("dna_bases", "D", false, TOOL_HELP("force dna-bases if column fits pattern", 0)), \
    TOOL_ARG("max_length", "M", true, TOOL_HELP("limits line length", 0)), \
    TOOL_ARG("indent_width", "i", true, TOOL_HELP("indents the line", 0)), \
    TOOL_ARG("filter", "F", true, TOOL_HELP("filters lines", 0)), \
    TOOL_ARG("format", "f", true, TOOL_HELP("output format:", 0)), \
    TOOL_ARG("id_range", "r", false, TOOL_HELP("prints id-range", 0)), \
    TOOL_ARG("without_sra", "n", false, TOOL_HELP("without sra-type-translation", 0)), \
    TOOL_ARG("exclude", "x", true, TOOL_HELP("exclude these columns", 0)), \
    TOOL_ARG("boolean", "b", true, TOOL_HELP("defines how boolean's are printed (1,T)", 0)), \
    TOOL_ARG("numelem", "u", false, TOOL_HELP("print only element-count", 0)), \
    TOOL_ARG("numelemsum", "U", false, TOOL_HELP("sum element-count", 0)), \
    TOOL_ARG("phys-blobs", "", false, TOOL_HELP("show physical blobs", 0)), \
    TOOL_ARG("vdb-blobs", "", false, TOOL_HELP("show VDB-blobs", 0)), \
    TOOL_ARG("phys", "", false, TOOL_HELP("enumerate physical columns", 0)), \
    TOOL_ARG("readable", "", false, TOOL_HELP("enumerate readable columns", 0)), \
    TOOL_ARG("static", "", false, TOOL_HELP("enumerate static columns", 0)), \
    TOOL_ARG("obj_version", "j", false, TOOL_HELP("request vdb-version", 0)), \
    TOOL_ARG("obj_timestamp", "", false, TOOL_HELP("request object modification date", 0)), \
    TOOL_ARG("obj_type", "y", false, TOOL_HELP("report type of object", 0)), \
    TOOL_ARG("idx-report", "", false, TOOL_HELP("enumerate all available index", 0)), \
    TOOL_ARG("idx-range", "", true, TOOL_HELP("enumerate values and row-ranges of one index", 0)), \
    TOOL_ARG("cur-cache", "", true, TOOL_HELP("size of cursor cache", 0)), \
    TOOL_ARG("output-file", "", true, TOOL_HELP("write output to this file", 0)), \
    TOOL_ARG("output-path", "", true, TOOL_HELP("write output to this directory", 0)), \
    TOOL_ARG("gzip", "", false, TOOL_HELP("compress output using gzip", 0)), \
    TOOL_ARG("bzip2", "", false, TOOL_HELP("compress output using bzip2", 0)), \
    TOOL_ARG("output-buffer-size", "", true, TOOL_HELP("size of output-buffer, 0...none", 0)), \
    TOOL_ARG("disable-multithreading", "", false, TOOL_HELP("disable multithreading", 0)), \
    TOOL_ARG("info", "", false, TOOL_HELP("print info about run", 0)), \
    TOOL_ARG("spotgroups", "", false, TOOL_HELP("show spotgroups", 0)), \
    TOOL_ARG("merge-ranges", "", false, TOOL_HELP("merge and sort row-ranges", 0)), \
    TOOL_ARG("spread", "", false, TOOL_HELP("show spread of integer values", 0)), \
    TOOL_ARG("append", "a", false, TOOL_HELP("append to output-file, if output-file used", 0)), \
    TOOL_ARG("len-spread", "", false, TOOL_HELP("show spread of READ/REF_LEN values", 0)), \
    TOOL_ARG("slice", "", true, TOOL_HELP("find a slice of given depth", 0)), \
    TOOL_ARG("cell-debug", "", false, TOOL_HELP(0)), \
    TOOL_ARG("cell-v1", "", false, TOOL_HELP(0)), \
    TOOL_ARG("ngc", "", true, TOOL_HELP("path to ngc file", 0)), \
    TOOL_ARG("view", "", true, TOOL_HELP("view-name", 0)), \
    TOOL_ARG("inspect", "", false, TOOL_HELP("inspect data usage inside object", 0)), \
    TOOL_ARG(0, 0, 0, TOOL_HELP(0)))

#define TOOL_NAME_FASTERQ_DUMP "fasterq-dump" /* from argv[0] */
#define TOOL_ARGS_FASTERQ_DUMP TOOL_ARGS ( \
    TOOL_ARG("format", "F", true, TOOL_HELP("format (special, fastq, default=fastq)", 0)), \
    TOOL_ARG("outfile", "o", true, TOOL_HELP("output-file", 0)), \
    TOOL_ARG("outdir", "O", true, TOOL_HELP("output-dir", 0)), \
    TOOL_ARG("bufsize", "b", true, TOOL_HELP("size of file-buffer dflt=1MB", 0)), \
    TOOL_ARG("curcache", "c", true, TOOL_HELP("size of cursor-cache dflt=10MB", 0)), \
    TOOL_ARG("mem", "m", true, TOOL_HELP("memory limit for sorting dflt=100MB", 0)), \
    TOOL_ARG("temp", "t", true, TOOL_HELP("where to put temp. files dflt=curr dir", 0)), \
    TOOL_ARG("threads", "e", true, TOOL_HELP("how many thread dflt=6", 0)), \
    TOOL_ARG("progress", "p", false, TOOL_HELP("show progress", 0)), \
    TOOL_ARG("details", "x", false, TOOL_HELP("print details", 0)), \
    TOOL_ARG("split-spot", "s", false, TOOL_HELP("split spots into reads", 0)), \
    TOOL_ARG("split-files", "S", false, TOOL_HELP("write reads into different files", 0)), \
    TOOL_ARG("split-3", "3", false, TOOL_HELP("writes single reads in special file", 0)), \
    TOOL_ARG("concatenate-reads", "", false, TOOL_HELP("writes whole spots into one file", 0)), \
    TOOL_ARG("stdout", "Z", false, TOOL_HELP("print output to stdout", 0)), \
    TOOL_ARG("force", "f", false, TOOL_HELP("force to overwrite existing file(s)", 0)), \
    TOOL_ARG("skip-technical", "", false, TOOL_HELP("skip technical reads", 0)), \
    TOOL_ARG("include-technical", "", false, TOOL_HELP("include technical reads", 0)), \
    TOOL_ARG("min-read-len", "M", true, TOOL_HELP("filter by sequence-len", 0)), \
    TOOL_ARG("table", "", true, TOOL_HELP("which seq-table to use in case of pacbio", 0)), \
    TOOL_ARG("bases", "B", true, TOOL_HELP("filter by bases", 0)), \
    TOOL_ARG("append", "A", false, TOOL_HELP("append to output-file", 0)), \
    TOOL_ARG("fasta", "", false, TOOL_HELP("produce FASTA output", 0)), \
    TOOL_ARG("fasta-unsorted", "", false, TOOL_HELP("produce FASTA output, unsorted", 0)), \
    TOOL_ARG("fasta-ref-tbl", "", false, TOOL_HELP("produce FASTA output from REFERENCE tbl", 0)), \
    TOOL_ARG("fasta-concat-all", "", false, TOOL_HELP("concatenate all rows and produce FASTA", 0)), \
    TOOL_ARG("internal-ref", "", false, TOOL_HELP("extract only internal REFERENCEs", 0)), \
    TOOL_ARG("external-ref", "", false, TOOL_HELP("extract only external REFERENCEs", 0)), \
    TOOL_ARG("ref-name", "", true, TOOL_HELP("extract only these REFERENCEs", 0)), \
    TOOL_ARG("ref-report", "", false, TOOL_HELP("enumerate references", 0)), \
    TOOL_ARG("use-name", "", false, TOOL_HELP("print name instead of seq-id", 0)), \
    TOOL_ARG("seq-defline", "", true, TOOL_HELP("custom defline for sequence: ", "$ac=accession, $sn=spot-name, ", "$sg=spot-group, $si=spot-id, ", "$ri=read-id, $rl=read-length", 0)), \
    TOOL_ARG("qual-defline", "", true, TOOL_HELP("custom defline for qualities: ", "same as seq-defline", 0)), \
    TOOL_ARG("only-unaligned", "U", false, TOOL_HELP("process only unaligned reads", 0)), \
    TOOL_ARG("only-aligned", "a", false, TOOL_HELP("process only aligned reads", 0)), \
    TOOL_ARG("disk-limit", "", true, TOOL_HELP("explicitly set disk-limit", 0)), \
    TOOL_ARG("disk-limit-tmp", "", true, TOOL_HELP("explicitly set disk-limit for temp. files", 0)), \
    TOOL_ARG("size-check", "", true, TOOL_HELP("switch to control:", "on=perform size-check (default), ", "off=do not perform size-check, ", "only=perform size-check only", 0)), \
    TOOL_ARG("ngc", "", true, TOOL_HELP("PATH to ngc file", 0)), \
    TOOL_ARG("keep", "", false, TOOL_HELP("keep temp. files", 0)), \
    TOOL_ARG("step", "", true, TOOL_HELP("stop after step", 0)), \
    TOOL_ARG("row-limit", "l", true, TOOL_HELP("limit rowcount per thread", 0)), \
    TOOL_ARG(0, 0, 0, TOOL_HELP(0)))

#define TOOL_NAME_SAM_DUMP "sam-dump" /* from argv[0] */
#define TOOL_ARGS_SAM_DUMP TOOL_ARGS ( \
    TOOL_ARG("unaligned", "u", false, TOOL_HELP("Output unaligned reads along with aligned reads", 0)), \
    TOOL_ARG("primary", "1", false, TOOL_HELP("Output only primary alignments", 0)), \
    TOOL_ARG("cigar-long", "c", false, TOOL_HELP("Output long version of CIGAR", 0)), \
    TOOL_ARG("cigar-CG", "", false, TOOL_HELP("Output CG version of CIGAR", 0)), \
    TOOL_ARG("header", "r", false, TOOL_HELP("Always reconstruct header", 0)), \
    TOOL_ARG("header-file", "", true, TOOL_HELP("take all headers from this file", 0)), \
    TOOL_ARG("no-header", "n", false, TOOL_HELP("Do not output headers", 0)), \
    TOOL_ARG("header-comment", "", true, TOOL_HELP("Add comment to header. Use multiple times for several lines. Use quotes", 0)), \
    TOOL_ARG("aligned-region", "", true, TOOL_HELP("Filter by position on genome.", "Name can either be file specific name (ex: \"chr1\" or \"1\").", "\"from\" and \"to\" (inclusive) are 1-based coordinates", 0)), \
    TOOL_ARG("matepair-distance", "", true, TOOL_HELP("Filter by distance between matepairs.", "Use \"unknown\" to find matepairs split between the references.", "Use from-to (inclusive) to limit matepair distance on the same reference", 0)), \
    TOOL_ARG("seqid", "s", false, TOOL_HELP("Print reference SEQ_ID in RNAME instead of NAME", 0)), \
    TOOL_ARG("hide-identical", "=", false, TOOL_HELP("Output '=' if base is identical to reference", 0)), \
    TOOL_ARG("gzip", "", false, TOOL_HELP("Compress output using gzip", 0)), \
    TOOL_ARG("bzip2", "", false, TOOL_HELP("Compress output using bzip2", 0)), \
    TOOL_ARG("spot-group", "g", false, TOOL_HELP("Add .SPOT_GROUP to QNAME", 0)), \
    TOOL_ARG("fastq", "", false, TOOL_HELP("Produce FastQ formatted output", 0)), \
    TOOL_ARG("fasta", "", false, TOOL_HELP("Produce Fasta formatted output", 0)), \
    TOOL_ARG("prefix", "p", true, TOOL_HELP("Prefix QNAME: prefix.QNAME", 0)), \
    TOOL_ARG("reverse", "", false, TOOL_HELP("Reverse unaligned reads according to read type", 0)), \
    TOOL_ARG("mate-cache-row-gap", "", true, TOOL_HELP(0)), \
    TOOL_ARG("cigar-CG-merge", "", false, TOOL_HELP("Apply CG fixups to CIGAR/SEQ/QUAL and outputs CG-specific columns", 0)), \
    TOOL_ARG("XI", "", false, TOOL_HELP("Output cSRA alignment id in XI column", 0)), \
    TOOL_ARG("qual-quant", "Q", true, TOOL_HELP("Quality scores quantization level", "a string like '1:10,10:20,20:30,30:-'", 0)), \
    TOOL_ARG("CG-evidence", "", false, TOOL_HELP("Output CG evidence aligned to reference", 0)), \
    TOOL_ARG("CG-ev-dnb", "", false, TOOL_HELP("Output CG evidence DNB's aligned to evidence", 0)), \
    TOOL_ARG("CG-mappings", "", false, TOOL_HELP("Output CG sequences aligned to reference ", 0)), \
    TOOL_ARG("CG-SAM", "", false, TOOL_HELP("Output CG evidence DNB's aligned to reference ", 0)), \
    TOOL_ARG("report", "", false, TOOL_HELP("report options instead of executing", 0)), \
    TOOL_ARG("output-file", "", true, TOOL_HELP("print output into this file (instead of STDOUT)", 0)), \
    TOOL_ARG("output-buffer-size", "", true, TOOL_HELP("size of output-buffer(dflt:32k, 0...off)", 0)), \
    TOOL_ARG("cachereport", "", false, TOOL_HELP("print report about mate-pair-cache", 0)), \
    TOOL_ARG("unaligned-spots-only", "", false, TOOL_HELP("output reads for spots with no aligned reads", 0)), \
    TOOL_ARG("CG-names", "", false, TOOL_HELP("prints cg-style spotgroup.spotid formed names", 0)), \
    TOOL_ARG("cursor-cache", "", true, TOOL_HELP("open cached cursor with this size", 0)), \
    TOOL_ARG("min-mapq", "", true, TOOL_HELP("min. mapq an alignment has to have, to be printed", 0)), \
    TOOL_ARG("no-mate-cache", "", false, TOOL_HELP("do not use a mate-cache, slower but less memory usage", 0)), \
    TOOL_ARG("rna-splicing", "", false, TOOL_HELP("modify cigar-string (replace .D. with .N.) and add output flags (XS:A:+/-) ", "when rna-splicing is detected by match to spliceosome recognition sites", 0)), \
    TOOL_ARG("rna-splice-level", "", true, TOOL_HELP("level of rna-splicing detection (0,1,2)", "when testing for spliceosome recognition sites ", "0=perfect match, 1=one mismatch, 2=two mismatches ", "one on each site", 0)), \
    TOOL_ARG("rna-splice-log", "", true, TOOL_HELP("file, into which rna-splice events are written", 0)), \
    TOOL_ARG("disable-multithreading", "", false, TOOL_HELP("disable multithreading", 0)), \
    TOOL_ARG("omit-quality", "o", false, TOOL_HELP("omit qualities", 0)), \
    TOOL_ARG("with-md-flag", "", false, TOOL_HELP("print MD-flag", 0)), \
    TOOL_ARG("dump-mode", "", true, TOOL_HELP(0)), \
    TOOL_ARG("cigar-test", "", true, TOOL_HELP(0)), \
    TOOL_ARG("legacy", "", false, TOOL_HELP(0)), \
    TOOL_ARG("new", "", false, TOOL_HELP(0)), \
    TOOL_ARG("ngc", "", true, TOOL_HELP("PATH to ngc file", 0)), \
    TOOL_ARG("timing", "", true, TOOL_HELP(0)), \
    TOOL_ARG(0, 0, 0, TOOL_HELP(0)))

#define TOOL_NAME_SRA_PILEUP "sra-pileup" /* from argv[0] */
#define TOOL_ARGS_SRA_PILEUP TOOL_ARGS ( \
    TOOL_ARG("minmapq", "q", true, TOOL_HELP("Minimum mapq-value, ", "alignments with lower mapq", "will be ignored (default=0)", 0)), \
    TOOL_ARG("duplicates", "d", true, TOOL_HELP("process duplicates 0..off/1..on", 0)), \
    TOOL_ARG("noskip", "s", false, TOOL_HELP("Does not skip reference-regions without alignments", 0)), \
    TOOL_ARG("showid", "i", false, TOOL_HELP("Shows alignment-id for every base", 0)), \
    TOOL_ARG("spotgroups", "p", false, TOOL_HELP("divide by spotgroups", 0)), \
    TOOL_ARG("depth-per-spotgroup", "", false, TOOL_HELP("print depth per spotgroup", 0)), \
    TOOL_ARG("seqname", "e", false, TOOL_HELP("use original seq-name", 0)), \
    TOOL_ARG("minmismatch", "", true, TOOL_HELP("min percent of mismatches used in function mismatch, default is 5%", 0)), \
    TOOL_ARG("merge-dist", "", true, TOOL_HELP("If adjacent slices are closer than this, ", "they are merged and a skiplist is created. ", "a value of zero disables the feature, default is 10000", 0)), \
    TOOL_ARG("function", "", true, TOOL_HELP("alternative functionality", 0)), \
    TOOL_ARG("ngc", "", true, TOOL_HELP("path to ngc file", 0)), \
    TOOL_ARG("aligned-region", "r", true, TOOL_HELP("Filter by position on genome.", "Name can either be file specific name", "(ex: \"chr1\" or \"1\").", "\"from\" and \"to\" are 1-based coordinates", 0)), \
    TOOL_ARG("noqual", "n", false, TOOL_HELP("Omit qualities in output", 0)), \
    TOOL_ARG("outfile", "o", true, TOOL_HELP("Output will be written to this file", "instead of std-out", 0)), \
    TOOL_ARG("table", "t", true, TOOL_HELP("Which alignment table(s) to use (p|s|e):", "p - primary, s - secondary, e - evidence-interval", "(default = p)", 0)), \
    TOOL_ARG("gzip", "", false, TOOL_HELP("Compress output using gzip", 0)), \
    TOOL_ARG("bzip2", "", false, TOOL_HELP("Compress output using bzip2", 0)), \
    TOOL_ARG("infile", "f", true, TOOL_HELP("File with all input-parameters / options", 0)), \
    TOOL_ARG("schema", "S", true, TOOL_HELP("optional schema-file to be used", 0)), \
    TOOL_ARG("disable-multithreading", "", false, TOOL_HELP("disable multithreading", 0)), \
    TOOL_ARG("timing", "", true, TOOL_HELP("write timing log-file", 0)), \
    TOOL_ARG(0, 0, 0, TOOL_HELP(0)))

#define TOOL_NAME_PREFETCH "prefetch" /* from argv[0] */
#define TOOL_ARGS_PREFETCH TOOL_ARGS ( \
    TOOL_ARG("type", "T", true, TOOL_HELP("Specify file type to download.", "Default: sra", 0)), \
    TOOL_ARG("transport", "t", true, TOOL_HELP("Transport: one of: fasp; http; both [default].", "(fasp only; http only; first try fasp (ascp), use http if cannot download using fasp).", 0)), \
    TOOL_ARG("location", "", true, TOOL_HELP("Location of data.", 0)), \
    TOOL_ARG("min-size", "N", true, TOOL_HELP("Minimum file size to download in KB (inclusive).", 0)), \
    TOOL_ARG("max-size", "X", true, TOOL_HELP("Maximum file size to download in KB (exclusive).", "Default: 20G", 0)), \
    TOOL_ARG("force", "f", true, TOOL_HELP("Force object download: one of: no, yes, all, ALL.", "no [default]: skip download if the object if found and complete;", "yes: download it even if it is found and is complete;", "all: ignore lock files (stale locks or it is being downloaded by another process - use at your own risk!);", "ALL: ignore lock files, restart download from beginning.", 0)), \
    TOOL_ARG("resume", "r", true, TOOL_HELP("Resume partial downloads: one of: no, yes [default].", 0)), \
    TOOL_ARG("verify", "C", true, TOOL_HELP("Verify after download: one of: no, yes [default].", 0)), \
    TOOL_ARG("progress", "p", false, TOOL_HELP("Show progress.", 0)), \
    TOOL_ARG("heartbeat", "H", true, TOOL_HELP("Time period in minutes to display download progress.", "(0: no progress), default: 1", 0)), \
    TOOL_ARG("eliminate-quals", "", false, TOOL_HELP("Download SRA Lite files with simplified base quality scores, or fail if not available.", 0)), \
    TOOL_ARG("check-all", "c", false, TOOL_HELP("Double-check all refseqs.", 0)), \
    TOOL_ARG("check-rs", "S", true, TOOL_HELP("Check for refseqs in downloaded files: one of: no, yes, smart [default]. Smart: skip check for large encrypted non-sra files.", 0)), \
    TOOL_ARG("list", "l", false, TOOL_HELP("List the content of a kart file.", 0)), \
    TOOL_ARG("numbered-list", "n", false, TOOL_HELP("List the content of a kart file with kart row numbers.", 0)), \
    TOOL_ARG("list-sizes", "s", false, TOOL_HELP("List the content of a kart file with target file sizes.", 0)), \
    TOOL_ARG("order", "o", true, TOOL_HELP("Kart prefetch order when downloading a kart: one of: kart, size.", "(in kart order, by file size: smallest first), default: size.", 0)), \
    TOOL_ARG("rows", "R", true, TOOL_HELP("Kart rows to download (default all).", "Row list should be ordered.", 0)), \
    TOOL_ARG("perm", "", true, TOOL_HELP("PATH to jwt cart file.", 0)), \
    TOOL_ARG("ngc", "", true, TOOL_HELP("PATH to ngc file.", 0)), \
    TOOL_ARG("cart", "", true, TOOL_HELP("To read a kart file.", 0)), \
    TOOL_ARG("text-kart", "", true, TOOL_HELP("To read a textual format kart file (DEBUG ONLY).", 0)), \
    TOOL_ARG("ascp-path", "a", true, TOOL_HELP("Path to ascp program and private key file (aspera_tokenauth_id_rsa)", 0)), \
    TOOL_ARG("ascp-options", "", true, TOOL_HELP("Arbitrary options to pass to ascp command line.", 0)), \
    TOOL_ARG("FAIL-ASCP", "F", false, TOOL_HELP("Force ascp download fail to test ascp->http download combination.", 0)), \
    TOOL_ARG("output-file", "", true, TOOL_HELP("Write file to FILE when downloading a single file.", 0)), \
    TOOL_ARG("output-directory", "O", true, TOOL_HELP("Save files to DIRECTORY/", 0)), \
    TOOL_ARG("dryrun", "", false, TOOL_HELP("Dry run the application: don't download, only check resolving.", 0)), \
    TOOL_ARG(0, 0, 0, TOOL_HELP(0)))


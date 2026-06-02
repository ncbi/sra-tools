# test-sra

## Usage

```text
  quick check mode:
   test-sra -Q [ name... ]

  full test mode:
   test-sra [+acdDfFgnoOprRsStuw] [-acdDfFgnoOprRsStuw] [-R] [-N] [-C]
            [-X <type>] [-L <path>] [options] name [ name... ]

Test [SRA] object, resolve it, print dependencies, configuration

[+tests] - add tests
[-tests] - remove tests
```

## Options

| Option | Description |
|---|---|
| -h\|--help | Output brief explanation for the program. |
| -V\|--version | Display the version of the program then<br>quit. |
| -L\|--log-level &lt;level&gt; | Logging level as number or enum string. One<br>of (fatal\|sys\|int\|err\|warn\|info\|debug) or<br>(0-6) Current/default is warn. |
| -v\|--verbose | Increase the verbosity of the program<br>status messages. Use multiple times for more<br>verbosity. Negates quiet. |
| -q\|--quiet | Turn off all status messages for the<br>program. Negated by verbose. |
| --option-file &lt;file&gt; | Read more options and parameters from the<br>file. |

## Tests

s - print SRA software information
S - print SRA software information and latest SRA toolkit version
u - print operation system information
c - print configuration
n - print NCBI error report
f - print ascp information
F - print verbose ascp information
t - print object types
g - print NGS information
R - print repositories information
p - print content of resolved remote HTTP file
w - run network test
r - call VResolver
d - call ListDependencies(missing)
D - call ListDependencies(all)
o - call VDBManagerOpenTableRead(object)
O - call VDBManagerOpenDBRead(object)
A - run Analysys and print recommendations how to fix found problems
a - all tests except VDBManagerOpen...Read and verbose ascp

In quick check mode - the base checks are run;
in full test mode (default) all the tests are available.

In full mode, if no tests were specified then all tests will be run.

-X &lt; xml | text &gt; - whether to generate well-formed XML
-R - check objects recursively
-N - do not call VDBManagerPathType
-C - do not disable caching (default: from configuration)
-b --bytes=K - print the first K bytes of resolved remote HTTP file)<br>(default: 256)
-l --library=&lt;path to library&gt; - print version of dynamic library

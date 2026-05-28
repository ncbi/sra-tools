# CLI Help

## Usage

```sh
quick check mode:
test-sra -Q [ name... ]
full test mode:
test-sra [+acdDfFgnoOprRsStuw] [-acdDfFgnoOprRsStuw] [-R] [-N] [-C]
[-X <type>] [-L <path>] [options] name [ name... ]
Test [SRA] object, resolve it, print dependencies, configuration
[+tests] - add tests
[-tests] - remove tests
```

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

-X < xml | text > - whether to generate well-formed XML
-R - check objects recursively
-N - do not call VDBManagerPathType
-C - do not disable caching (default: from configuration)
-b --bytes=K - print the first K bytes of resolved remote HTTP file)

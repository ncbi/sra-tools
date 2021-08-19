ngs
===

# NGS Language Bindings

NGS is a new, domain-specific API for accessing reads, alignments and pileups produced from Next Generation Sequencing. The API itself is independent from any particular back-end implementation, and supports use of multiple back-ends simultaneously. It also provides a library for building new back-end "engines". The engine for accessing SRA data is contained within the sister repository [ncbi-vdb](https://github.com/ncbi/ncbi-vdb).

The API is currently expressed in C++, Java and Python languages. The design makes it possible to maintain a high degree of similarity between the code in one language and code in another - especially between C++ and Java.

# Documentation

Further documentation is available on the [wiki](https://github.com/ncbi/ngs/wiki).

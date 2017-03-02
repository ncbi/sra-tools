#!/bin/sh
cc -g -c test-expandCIGAR.c && c++ -g expandCIGAR.cpp cigar2events.cpp fasta-file.cpp test-expandCIGAR.o -o test-expandCIGAR

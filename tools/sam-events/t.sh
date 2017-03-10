#-----------------------------------------------------------------------
#            dump just one relatively small chromosome
#-----------------------------------------------------------------------
#sam-dump SRR834507 --seqid --aligned-region CM000459.1 > 1.sam



#-----------------------------------------------------------------------
#                       full-accession example
#-----------------------------------------------------------------------

ACC=SRR834507
#-----------------------------------------------------------------------
#   dump the whole accession as SAM ( takes long, creates huge file )
#-----------------------------------------------------------------------
#time sam-dump $ACC --seqid > $ACC.sam

#-----------------------------------------------------------------------
#make a fasta-file of the references used ( fast, output relatively small )
#-----------------------------------------------------------------------
#time vdb-dump $ACC -T REFERENCE -f fasta2 > $ACC.fasta

#-----------------------------------------------------------------------
#now let the sam-events tool extract the events
#-----------------------------------------------------------------------
#time sam-events $ACC.sam --reference $ACC.fasta --min-count 2 --reduce > $ACC.events



#-----------------------------------------------------------------------
#                      test for memory leaks
#-----------------------------------------------------------------------
#valgrind --ncbi --leak-check=full --log-file=vg.log sam-events 1.sam --reference CM000459_1.fasta --limit 10000 --reduce > 1.txt

#valgrind --orig --tool=callgrind --log-file=vg1.log sam-events 1.sam --reference CM000459_1.fasta --limit 10000 --reduce > 1.txt

#totalview sam-events -a 1.sam --reference CM000459_1.fasta --limit 10000 --reduce > 1.txt

#sam-events 1.sam --reference CM000459_1.fasta --limit 100000 --purge 2048 --reduce > 1.txt
#sam-events 1.sam --reference CM000459_1.fasta --reduce > 1.txt

#cat 1.sam | sam-events --reference CM000459_1.fasta --limit 100000 --purge 2048 --reduce > 1.txt
#cat 1.sam | sam-events --reference CM000459_1.fasta --reduce > 2.txt

#-----------------------------------------------------------------------
#                      compare fasts vs validated
#-----------------------------------------------------------------------
echo "validated SAM:"
time sam-events 1.sam --reference CM000459_1.fasta --reduce > 1.txt
echo "not validated SAM:"
time sam-events 1.sam --reference CM000459_1.fasta --reduce --fast > 2.txt
diff -q 1.txt 2.txt

#cc -g -c test-expandCIGAR.c && c++ -g -o test-expandCIGAR expandCIGAR.cpp cigar2events.cpp fasta-file.cpp test-expandCIGAR.o
#g++ expandCIGAR.cpp fasta-file.cpp cigar2events.cpp test-expandCIGAR.c

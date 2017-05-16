#!/bin/csh
echo "$1 $2" | awk -F'[: ]' '{printf("https://www.ncbi.nlm.nih.gov/projects/sviewer/?id=%s&tracks=[key:alignment_track,dbname:SRA,annots:%s,sort_by:strand,LinkMatePairAligns]&v=%d..%d&mk=%d:%d|%s\n",$2,$1,$3+1,$3+$4,$3+1,$3+$4,$5)}' 

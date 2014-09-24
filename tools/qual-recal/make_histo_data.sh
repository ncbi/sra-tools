#!/bin/bash

echo qual-recalib-stat -p -o qstat.${$}.txt -m file -g 8 "${@}" || exit ${?};
qual-recalib-stat -p -o qstat.${$}.txt -m file -g 8 "${@}" || exit ${?};
tail -n +2 qstat.${$}.txt > qstat.${$}.headless.txt
(echo -n "SPOTGROUPS: " ; cut -f 1 qstat.${$}.headless.txt | sort    | uniq | wc -l)  >qstat.${$}.hdr
(echo -n "MAX_POS: "    ; cut -f 2 qstat.${$}.headless.txt | sort -n | tail -n 1   ) >>qstat.${$}.hdr
(echo -n "MAX_READ: "   ; cut -f 3 qstat.${$}.headless.txt | sort -n | tail -n 1   ) >>qstat.${$}.hdr
(cat qstat.${$}.hdr; echo ""; cat qstat.${$}.headless.txt) >qstat.${$}.hdr.text
rm qstat.${$}.headless.txt qstat.${$}.hdr
echo qual-recal -s qstat.${$}.hdr.text "${1}"
qual-recal -s qstat.${$}.hdr.text "${1}"

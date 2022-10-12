#!/bin/bash

progname=`basename $0`
dir=$1
[ ! -d "$dir" ] && { echo "${progname}: Input directory '${dir}' does not exist (error:1)"; exit 1; }
cd  $dir || exit 1
bam_param=`cat bam_param`
bam_param="${bam_param%\\n}" # chomp
bam_param="${bam_param} --cache-size 3809280"

is_cram=0

bam_list=(`ls -S *.bam 2>/dev/null`)
if [ ${#bam_list[@]} -eq 0 ]; then
  cram_list=(`ls -S *.cram 2>/dev/null`)
  if [ ${#cram_list[@]} -eq 0 ]; then
      echo "No Input files"
      exit 1
  else
    cram=${cram_list[0]}
    is_cram=1   
  fi
else  
  for bam in ${bam_list[*]}
  do
    bams="${bams} ${bam}"
  done
fi

output="output.old"
mkdir ${output} || exit 1
if [ $is_cram -eq 1 ]; then
  /usr/bin/time -f '%E, %M' --output ${output}/time.log samtools view -h ${cram} | ../bam-load.2.9.1 ${bam_param} -o ${output}/vdb.out /dev/stdin 1> ${output}/stdout.log 2> ${output}/stderr.log
else
  /usr/bin/time -f '%E, %M' --output ${output}/time.log ../bam-load.2.9.1 ${bam_param} -o ${output}/vdb.out ${bams} 1> ${output}/stdout.log 2> ${output}/stderr.log
fi
cd ..

exit 0

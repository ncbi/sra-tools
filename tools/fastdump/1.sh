scratch="-t /dev/shm/raetzw_tmp"
numthreads="-e 6"
mem_limit="-m 50M"
cur_cache="-c 5M"
show_progress="-p"
force_overwrite="-f"
show_details="-x"
split_spot="-s"
split_file="-S"
rowid_as_name="-N"

# $1...file1, $2...md5
compare_md5()
{
    sum1=`md5sum $1 | awk '{print $1}'`
    if [ "$sum1" == "$2" ]
    then
        echo "$1 ... OK"
    else
        echo "$1 ... ERR ( $sum1 != $2 )"
    fi
}

fastdump_split_file_row_id_as_name()
{
    echo "***** fastdump database split into files with rowid as name *****"
    out="/dev/shm/raetzw_out/$1.fastq"

    rm -rf $out.1 $out.2

    options="$show_progress $force_overwrite $show_details $split_spot $split_file $rowid_as_name"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out.1 $2
    compare_md5 $out.2 $3
}

fastdump_split_file()
{
    echo "***** fastdump database split into files *****"
    out="/dev/shm/raetzw_out/$1.fastq"
    
    rm -rf $out.1 $out.2

    options="$show_progress $force_overwrite $show_details $split_spot $split_file"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out.1 $2
    compare_md5 $out.2 $3
}

fastdump_split_row_id_as_name()
{
    echo "***** fastdump database split with rowid as name *****"
    out="/dev/shm/raetzw_out/$1.fastq"
    
    rm -rf $out

    options="$show_progress $force_overwrite $show_details $split_spot $rowid_as_name"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out $2
}

fastdump_split()
{
    echo "***** fastdump database split *****"
    out="/dev/shm/raetzw_out/$1.fastq"
   
    rm -rf $out

    options="$show_progress $force_overwrite $show_details $split_spot"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    #totalview fastdump -a $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out $2
}

fastdump_not_split_row_id_as_name()
{
    echo "***** fastdump database not split with rowid as name *****"
    out="/dev/shm/raetzw_out/$1.fastq"
   
    rm -rf $out

    options="$show_progress $force_overwrite $show_details $rowid_as_name"    
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out $2
}

fastdump_not_split()
{
    echo "***** fastdump database not split *****"
    out="/dev/shm/raetzw_out/$1.fastq"
   
    rm -rf $out

    options="$show_progress $force_overwrite $show_details"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out $2
}

test_csra()
{
    #a CSRA-database with SEQUENCE, PRIMARY_ALIGNMENT and REFERENCE
    csra="SRR341578"
    #none-splitted
    md5_csra_n="b6832cde19b4083ea0a5a246bad1a670"
    #splitted
    md5_csra_s="66b97c245a2c605a8854967b454d789e"
    md5_csra_1="087c5292b808e2d30dfd3d9e08ba3c56"
    md5_csra_2="38c926a3c56ee3833ddd2092cacd0129"

    #fastdump_not_split $csra $md5_csra_n
    #fastdump_not_split_row_id_as_name $csra $md5_csra_n
    #fastdump_split $csra $md5_csra_s
    #fastdump_split_row_id_as_name $csra $md5_csra_s
    fastdump_split_file_row_id_as_name $csra $md5_csra_1 $md5_csra_2
    #fastdump_split_file $csra $md5_csra_1 $md5_csra_2
}

test_sra_flat()
{
    #a flat SRA-table
    sra_flat="SRR942391"
    #none-splitted
    md5_sra_flat_n="590366f579aa503bbedec1dab66df2ad"
    #splitted
    md5_sra_flat_s="a668be91b3e57b11fc56b977af1c1426"
    md5_sra_flat_1="fff51585bf9963419e4ce505c0f57637"
    md5_sra_flat_2="20784e715bd2498aca0e82ec61a195b9"

    fastdump_not_split $sra_flat $md5_sra_flat_n
    fastdump_split $sra_flat $md5_sra_flat_s
    fastdump_split_file $sra_flat $md5_sra_flat_1 $md5_sra_flat_2
}

test_sra_db()
{
    #a flat SRA-table as the only table in a database
    sra_db="SRR6173369"

    #none-splitted
    md5_sra_db_n="38250674922d516912b06498d8b4d3fb"
    #splitted
    md5_sra_db_s="07106db1a11bdd1cc8c382a4e2482b5f"
    md5_sra_db_1="c0be39a6d0566bfe72621e0a69cb2fe4"
    md5_sra_db_2="0e560c82f092bac8b2d6e92c1262ed95"
    
    #fastdump_not_split $sra_db $md5_sra_db_n
    #fastdump_split $sra_db $md5_sra_db_s
    fastdump_split_file $sra_db $md5_sra_db_1 $md5_sra_db_2    
}

test_csra
#test_sra_flat
#test_sra_db

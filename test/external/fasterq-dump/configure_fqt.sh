
#clear out the database
#rm -rf tests.db

#in case the db does not exist...
python fqt.py --fn init

#--- some cSRA - tests
ACC="ERR1681325"

#for FASTQ
python fqt.py --fn add --name csra_fq1 --acc $ACC
python fqt.py --fn add --name csra_fq2 --acc $ACC --opt \'--split-files\'
python fqt.py --fn add --name csra_fq3 --acc $ACC --opt \'--split-spot\'
python fqt.py --fn add --name csra_fq4 --acc $ACC --opt \'--concatenate-reads\'

#for FASTA
python fqt.py --fn add --name csra_fasta1 --acc $ACC --opt \'--fasta\'
python fqt.py --fn add --name csra_fasta2 --acc $ACC --opt \'--fasta\' \'--split-files\'
python fqt.py --fn add --name csra_fasta3 --acc $ACC --opt \'--fasta\' \'--split-spot\'
python fqt.py --fn add --name csra_fasta4 --acc $ACC --opt \'--fasta\' \'--concatenate-reads\'

#extract refs as FASTA
python fqt.py --fn add --name csra_fasta5 --acc $ACC --opt \'--fasta-ref-tbl\'

#-------------------------------------------------------------------------------------

#--- flat table - tests
ACC="SRR1155823"

#for FASTQ
python fqt.py --fn add --name flat_fq1 --acc $ACC
python fqt.py --fn add --name flat_fq2 --acc $ACC --opt \'--split-files\'
python fqt.py --fn add --name flat_fq3 --acc $ACC --opt \'--split-spot\'
python fqt.py --fn add --name flat_fq4 --acc $ACC --opt \'--concatenate-reads\'

#for FASTA
python fqt.py --fn add --name flat_fasta1 --acc $ACC --opt \'--fasta\'
python fqt.py --fn add --name flat_fasta2 --acc $ACC --opt \'--fasta\' \'--split-files\'
python fqt.py --fn add --name flat_fasta3 --acc $ACC --opt \'--fasta\' \'--split-spot\'
python fqt.py --fn add --name flat_fasta4 --acc $ACC --opt \'--fasta\' \'--concatenate-reads\'

#-------------------------------------------------------------------------------------

#--- db with just SEQ-table - tests
ACC="SRR5057153"

#for FASTQ
python fqt.py --fn add --name db_fq1 --acc $ACC
python fqt.py --fn add --name db_fq2 --acc $ACC --opt \'--split-files\'
python fqt.py --fn add --name db_fq3 --acc $ACC --opt \'--split-spot\'
python fqt.py --fn add --name db_fq4 --acc $ACC --opt \'--concatenate-reads\'

#for FASTA
python fqt.py --fn add --name db_fasta1 --acc $ACC --opt \'--fasta\'
python fqt.py --fn add --name db_fasta2 --acc $ACC --opt \'--fasta\' \'--split-files\'
python fqt.py --fn add --name db_fasta3 --acc $ACC --opt \'--fasta\' \'--split-spot\'
python fqt.py --fn add --name db_fasta4 --acc $ACC --opt \'--fasta\' \'--concatenate-reads\'

#-------------------------------------------------------------------------------------

#--- some ref - tests
ACC="NC_011752.1"
python fqt.py --fn add --name refseq1 --acc $ACC --opt \'--fasta-concat-all\'

ACC="AAAA01"
python fqt.py --fn add --name refseq2 --acc $ACC --opt \'--fasta-concat-all\'

#-------------------------------------------------------------------------------------
#now write expected md5-values
python fqt.py --fn exp

# precondition: binaries to test should be in the PATH

ACC=SRR053325
UNAME=`uname`
echo "uname='$UNAME'"
if [ "$UNAME" == 'Linux' ] ; then
    OS=linux
else
    OS=mac
fi
BAD=/net/traces01/trace_software/vdb/vdb-versions/2.6.1/$OS/release/x86_64/bin
if [ "$TEMPDIR" == "" ] ; then
    echo '$TEMPDIR should be set by sra-tools/test/prefetch/Makefile.'
    echo "It was not so assuming $0 is not run by Makefile."
    echo 'Creating temporary directory here...'
    DIR=`pwd`/XXXXXX
    TEMPDIR=`mktemp -d $DIR`                                           || exit 1
    CREATEDTEMPDIR=$TEMPDIR
    echo "Created temporary directory '$CREATEDTEMPDIR'"
fi
export NCBI_HOME=$TEMPDIR
KFGFILE=$NCBI_HOME/sra-tools.test.prefetch.test-prefetch.sh.kfg
echo repository/user/main/public/root=\"$NCBI_HOME/public\" > $KFGFILE || exit 2
CMD='vdb-config --restore-defaults'
echo
echo "Running $CMD..."
$CMD                                                                   || exit 3
vdb-config -s/repository/remote/disabled=true                          || exit 4
vdb-config -s/repository/site/disabled=true                            || exit 5

echo
echo === $ACC SHOULD NOT BE FOUND WHEN REMOTE AND SITE REPOSITORIES ARE DISABLED
srapath $ACC                                                           && exit 6

echo
vdb-config -s/repository/remote/disabled=false                         || exit 7
echo === $ACC SHOULD BE FOUND REMOTELY WHEN REMOTE REPOSITORY IS ENABLED =======
srapath $ACC | grep ^http://sra-download.ncbi.nlm.nih.gov/srapub/$ACC\$|| exit 8

echo
echo == BUGGY OLD PREFETCH SHOULD STILL DOWNLOAD $ACC WITH DEFAULT CONFIGURATION
$BAD/prefetch $ACC -th                                                || exit  9
echo
echo === $ACC SHOULD BE FOUND IN CACHE AFTER DOWNLOAD ==========================
srapath $ACC | grep ^$NCBI_HOME/public/sra/$ACC.sra\$                 || exit 10

echo
echo Cleaning the cache...
cache-mgr -c                                                          || exit 11
echo
echo === $ACC SHOULD BE FOUND REMOTELY AFTER THE CACHE WAS CLEANED =============
srapath $ACC | grep ^http://sra-download.ncbi.nlm.nih.gov/srapub/$ACC\$||exit 12

vdb-config -s/repository/user/cache-disabled=true                     || exit 13
# This value is set
# when 'Enable Local File Caching' is unchecked by vdb-config --interactive

echo
echo == BUGGY OLD PREFETCH SHOULD FAIL TO DOWNLOAD $ACC WHEN CACHING IS DISABLED
$BAD/prefetch $ACC -th                                                && exit 14

echo
echo === $ACC SHOULD BE STILL FOUND REMOTELY AFTER PREFETCH FAILURE ============
srapath $ACC | grep ^http://sra-download.ncbi.nlm.nih.gov/srapub/$ACC\$||exit 15

echo
echo ====== A FIXED PREFETCH SHOULD DOWNLOAD $ACC WHEN CACHING IS DISABLED =====
CMD=`which prefetch`
CMD="$CMD $ACC -th"
$CMD                                                                  || exit 16

echo
echo === $ACC SHOULD BE FOUND IN CACHE AFTER DOWNLOAD ==========================
srapath $ACC | grep ^$NCBI_HOME/public/sra/$ACC.sra\$                 || exit 17

echo
if [ "$CREATEDTEMPDIR" != "" ] ; then
    rm -rv $CREATEDTEMPDIR                                             ||exit 18
fi

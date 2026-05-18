
#change current directory to the location of this script ( in case called from elsewhere )
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd $SCRIPT_DIR

rnd2sra root.ini -D $SCRIPT_DIR

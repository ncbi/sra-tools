OUTPUT1="sam-analyze"
OUTPUT2="sam-analyze-fast"
LIBS="-pthread -lm -ldl"

rm -rf $OUTPUT1 $OUTPUT2

if [ ! -f "sqlite3.o" ]; then
    gcc -c sqlite3.c
fi

if [ ! -f "sqlite3-fast.o" ]; then
    gcc -c sqlite3.c -O3 -o sqlite3-fast.o
fi

g++ sqlite3.o main.cpp $LIBS -o $OUTPUT1
g++ sqlite3-fast.o main.cpp $LIBS -O3 -o $OUTPUT2

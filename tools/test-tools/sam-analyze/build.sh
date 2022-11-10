OUTPUT="sam-analyze"
LIBS="-pthread -lm -ldl"

rm -rf $OUTPUT main.o

if [ ! -f "sqlite3.o" ]; then
    gcc -c sqlite3.c
fi

g++ sqlite3.o main.cpp $LIBS -o $OUTPUT

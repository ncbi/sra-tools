all: sra2ir

sra2ir: sra2ir.cpp ../shared/include/vdb.hpp
	c++ -std=c++11 -g -o $@ -L ../lib -l ncbi-wvdb -I ../shared/include -I ../include sra2ir.cpp

.PHONY: clean
clean:
	@rm -rf sra2ir *.o

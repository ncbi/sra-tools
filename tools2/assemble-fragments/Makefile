all: \
	text2ir \
	sra2ir \
	sam2ir \
	makeIRIndex \
	sortIndex \
	reorder-ir \
    summarize-pairs \
	assemble-fragments

.PHONY: clean slowtest

NCBI_VDB_OPTIONS = \
	-L $(NCBI_VDB_LIBS) \
	-l ncbi-vdb \
	-I $(NCBI_VDB_INCLUDE)

text2ir: text2ir.cpp ../shared/include/writer.hpp
	c++ -o $@ text2ir.cpp -std=c++11 -lpthread -lm $(CFLAGS) -I ../shared/include

sam2ir: sam2ir.cpp ../shared/include/writer.hpp
	c++ -o $@ sam2ir.cpp -std=c++11 -lpthread -lm $(CFLAGS) -I ../shared/include

sra2ir: sra2ir.cpp ../shared/include/vdb.hpp ../shared/include/writer.hpp
	c++ -o $@ sra2ir.cpp -std=c++11 -lpthread -lm $(CFLAGS) $(NCBI_VDB_OPTIONS) -I ../shared/include

makeIRIndex: makeIRIndex.cpp IRIndex.h ../shared/include/vdb.hpp
	c++ -o $@ makeIRIndex.cpp -std=c++11 -lpthread -lm $(CFLAGS) $(NCBI_VDB_OPTIONS) -I ../shared/include

sortIndex: sortIndex.cpp IRIndex.h ../shared/include/vdb.hpp
	c++ -o $@ sortIndex.cpp -std=c++11 -lpthread -lm $(CFLAGS) $(NCBI_VDB_OPTIONS) -I ../shared/include

reorder-ir: reorder-ir.cpp ../shared/include/vdb.hpp ../shared/include/writer.hpp
	c++ -o $@ reorder-ir.cpp -std=c++11 -lpthread -lm $(CFLAGS) $(NCBI_VDB_OPTIONS) -I ../shared/include

filter-ir: filter-ir.cpp ../shared/include/vdb.hpp ../shared/include/writer.hpp fragment.hpp
	c++ -o $@ filter-ir.cpp -std=c++11 -lpthread -lm $(CFLAGS) $(NCBI_VDB_OPTIONS) -I ../shared/include

summarize-pairs: summarize-pairs.cpp ../shared/include/vdb.hpp ../shared/include/writer.hpp fragment.hpp
	c++ -o $@ summarize-pairs.cpp -std=c++11 -lpthread -lm $(CFLAGS) $(NCBI_VDB_OPTIONS) -I ../shared/include

assemble-fragments: assemble-fragments.cpp ../shared/include/vdb.hpp ../shared/include/writer.hpp fragment.hpp
	c++ -o $@ assemble-fragments.cpp -std=c++11 -lpthread -lm $(CFLAGS) $(NCBI_VDB_OPTIONS) -I ../shared/include

clean:
	@rm -rf sra2ir text2ir sam2ir makeIRIndex reorder-ir sortIndex summarize-pairs assemble-fragments *.dSYM *.o

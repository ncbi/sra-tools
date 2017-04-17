all: text2ir sra2ir sam2ir

.PHONY: clean slowtest

text2ir: text2ir.cpp ../shared/include/writer.hpp
	c++ -std=c++11 $(CFLAGS) -o $@ -I ../shared/include text2ir.cpp

sam2ir: sam2ir.cpp ../shared/include/writer.hpp
	c++ -std=c++11 $(CFLAGS) -o $@ -I ../shared/include sam2ir.cpp

sra2ir: sra2ir.cpp ../shared/include/vdb.hpp ../shared/include/writer.hpp
	c++ -std=c++11 $(CFLAGS) -o $@ -L ../lib -l ncbi-wvdb -I ../shared/include -I ../include sra2ir.cpp

makeIRIndex: makeIRIndex.cpp IRIndex.h ../shared/include/vdb.hpp
	c++ -std=c++11 $(CFLAGS) -o $@ -L ../lib -l ncbi-wvdb -I ../shared/include -I ../include makeIRIndex.cpp

sortIndex: sortIndex.cpp IRIndex.h ../shared/include/vdb.hpp
	c++ -std=c++11 $(CFLAGS) -o $@ -L ../lib -l ncbi-wvdb -I ../shared/include -I ../include sortIndex.cpp

slowtest: sra2ir ../shared/schema/aligned-ir.schema.text
	./sra2ir SRR5054227 | ~/ncbi/ncbi-vdb/bin/general-loader.2.8.2 -I ../../../ncbi-vdb/interfaces -S ../shared/schema/aligned-ir.schema.text -T ~/tmp/IR.vdb

clean:
	@rm -rf sra2ir text2ir sam2ir *.dSYM *.o ~/tmp/IR.vdb

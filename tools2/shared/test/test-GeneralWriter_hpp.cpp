#include "../include/writer.hpp"
#include <cstdio>
#include <iostream>

void test(FILE *const output) {
    auto writer = Writer2(output);

    writer.destination("dummy.file.vdb");
    writer.schema("dummy.schema.text", "dummy:db");
    writer.info("test-GeneralWriter_hpp", "1.0.0");

    writer.addTable("DUMMY", {
        { "NAME", sizeof(char) },
        { "READ", sizeof(char) },
        { "READ_START", sizeof(int32_t) },
        { "READ_LENGTH", sizeof(int32_t), "READ_LEN" },
        { "READ_TYPE", sizeof(int8_t) },
    });

    writer.beginWriting();
    writer.flush();

    auto const &table = writer.table("DUMMY");
    auto const &name = table.column("NAME");
    auto const &sequence = table.column("READ");
    auto const &readStart = table.column("READ_START");
    auto const &readLength = table.column("READ_LENGTH");
    auto const &readType = table.column("READ_TYPE");
    static int8_t const defaultReadType[] = {0, 0};

    writer.logMessage("Starting");

    table.setMetadata("TABLE-NODE", "VALUE");
    sequence.setMetadata("COLUMN-NODE", "VALUE");

    writer.logMessage("Setting defaults ...");
    name.setDefault<std::string>("<unnamed spot>");
    readType.setDefault(2, defaultReadType);
    writer.logMessage("Done setting defaults");
    writer.logMessage("Writing table data ...");
    writer.progressMessage(0);

    for (auto i = 1; i <= 10; ++i) {
        int32_t const rs[] = {0, 25};
        int32_t const rl[] = {25, 25};

        name.setValue(std::string("SPOT_") + std::to_string(i));
        sequence.setValue(std::string(50, 'N'));
        readStart.setValue(2, rs);
        readLength.setValue(2, rl);
        table.closeRow();
        writer.progressMessage(i * 10);
    }

    writer.logMessage("Done");
    writer.endWriting();
}

int main(int argc, char *argv[]) {
    FILE *output = stdout;

    if (argc == 2) {
        if ((output = fopen(argv[1], "a")) == NULL) {
            std::cerr << "can't open " << argv[1] << std::endl;
            exit(1);
        }
    }
    test(output);
    return 0;
}

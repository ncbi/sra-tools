#include "var-expand.vers.h"

#include <kapp/main.h>
#include <klib/rc.h>

#include <iostream>


#include "helper.h"

extern "C"
{
    const char UsageDefaultName[] = "var-expand";
    ver_t CC KAppVersion ()
    {
        return VAR_EXPAND_VERS;
    }

    rc_t CC UsageSummary (const char * progname)
    {
        OUTMSG ((
        "TODO: Add summary\n"
        "\n", progname));
        return 0;
    }

    rc_t CC Usage ( struct Args const * args )
    {
        rc_t rc = 0;
        const char* progname = UsageDefaultName;
        const char* fullpath = UsageDefaultName;

        if (args == NULL)
            rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
        else
            rc = ArgsProgram(args, &fullpath, &progname);

        UsageSummary (progname);


        OUTMSG (("\nInput: the stream of lines in the format: <key> <tab> <input variation>\n\n"));
        OUTMSG (("\nOptions: no options\n"));

        XMLLogger_Usage();

        HelpOptionsStandard ();

        HelpVersion (fullpath, KAppVersion());

        return rc;
    }

    rc_t CC KMain ( int argc, char *argv [] )
    {
        std::cout << "Hello world" << std::endl;
        return 0;
    }
}

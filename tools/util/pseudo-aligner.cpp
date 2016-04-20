/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 */

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <stdexcept>
#include <vector>
#include <utility>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

using namespace std;

static unsigned stringtoui(string const &str, int base = 0)
{
    if (str.length() == 0)
        throw length_error("empty");
    
    char const *const s = str.c_str();
    char *endp = NULL;
    unsigned long const value = strtoul(s, &endp, base);
    
    if (*endp != '\0')
        throw invalid_argument("'" + str + "' is not convertible to an integer");
    
    if (value > unsigned(-1L))
        throw range_error("'" + str + "' is too big");

    return (unsigned)value;
}

struct Settings {
    ostream *refSeqStream;
    ostream *fastqStream;
    
    string refName;

    int coverageTarget;
    int lengthTarget;
    int readLength;
    int templateLength;
    
    Settings(map<string, string> &args)
    : templateLength(600)
    , readLength(150)
    , coverageTarget(stringtoui(args["coverage"]))
    , lengthTarget(stringtoui(args["length"]))
    {
        refName = args["refname"];
        refSeqStream = new ofstream((refName + ".fasta").c_str());
        fastqStream = new ofstream(args["fastq"].c_str());
    }
    ~Settings() {
        delete refSeqStream;
        delete fastqStream;
    }
    
    static vector<string> const &paramNames() {
        static char const *values[] = {
            "refname", "fastq", "coverage", "length"
        };
        static vector<string> const value(values, values + 4);

        return value;
    }
    static vector<string> const &paramTypes() {
        static char const *values[] = {
            "name", "file", "integer", "integer"
        };
        static vector<string> const value(values, values + 4);
        
        return value;
    }
};

static Settings const *settings;

static double frand()
{
    return double(random()) / double(1 << 30) - 1.0;
}

static pair<double, double> normalRandom()
{
    for ( ; ; ) {
        double const x = frand();
        double const y = frand();
        double const r = x * x + y * y;
        if (0.0 < r && r <= 1.0) {
            double const scale = sqrt(-2.0 * log(r) / r);
            return make_pair(x * scale, y * scale);
        }
    }
}

struct refbase {
    uint8_t base, count;
};
typedef vector<refbase> ref_t;

static void printReference(ref_t const &ref)
{
    ostream &out = *settings->refSeqStream;
    
    out << '>' << settings->refName << " A.randomus random sequence" << endl;
    int j = 0;
    for (ref_t::const_iterator i = ref.begin(); i != ref.end(); ++i) {
        uint8_t const base = i->base;
        out << "NACGT"[base == 0 ? (random() % 4 + 1) : base];
        if (++j == 70) {
            j = 0;
            out << endl;
        }
    }
    if (j)
        out << endl;
}

static uint8_t complement(uint8_t const base)
{
    return (1 > base || base > 4) ? 0 : (5 - base);
}

static bool isOverCovered(ref_t const &ref, int pos)
{
    for (int i = 0; i < settings->readLength; ++i) {
        int const coverage = ref[pos + i].count;
        if (coverage > 2 * settings->coverageTarget)
            return true;
    }
    return false;
}

static void covered(ref_t const &ref, int pos, int &left, int &right)
{
    left = 0;
    right = settings->readLength;
    while (ref[pos + left].count != 0) {
        ++left;
    }
    while (ref[pos + right - 1].count != 0) {
        --right;
    }
}

static void fillReference(ref_t &ref, int pos)
{
    for (int i = 0; i < settings->readLength; ++i) {
        int const check = ref[pos + i].count;
        
        if (check == 0) {
            bool const GC = frand() > 0.2;
            bool const AC = frand() > 0.0;
            ref[pos + i].base = (GC ? AC ? 2 : 3 : AC ? 1 : 4);
        }
        int const count = int(check) + 1;
        ref[pos + i].count = count < 0xFF ? count : 0xFF;
    }
}

static void copyReference(ref_t &ref, int pos, vector<uint8_t> const &SEQ, bool reverse)
{
    for (int i = 0; i < settings->readLength; ++i) {
        int const check = ref[pos + i].count;
        
        if (check == 0) {
            bool const mutate = -10.0 * log10(frand()) > 15.0;
            int const j = reverse ? (settings->readLength - 1 - i) : i;
            uint8_t const value = mutate ? uint8_t(random() % 4 + 1) : SEQ[j];
            uint8_t const base = reverse ? complement(value) : value;
            ref[pos + i].base = base;
        }
        int const count = int(check) + 1;
        ref[pos + i].count = count < 0xFF ? count : 0xFF;
    }
}

static ostream &printSequence(ostream &out, vector<uint8_t> const &SEQ, bool reverse)
{
    for (int i = 0; i < settings->readLength; ++i) {
        int const j = reverse ? (settings->readLength - 1 - i) : i;
        uint8_t const value = SEQ[j];
        char const base = "NACGT"[reverse ? complement(value) : value];
        
        out << base;
    }
    return out;
}

static ostream &printQuality(ostream &out, vector<uint8_t> const &QUAL, bool reverse)
{
    for (int i = 0; i < settings->readLength; ++i) {
        int const j = reverse ? (settings->readLength - 1 - i) : i;
        char const value = QUAL[j] + 33;
        
        out << value;
    }
    return out;
}

static void print1FastQ(uint64_t serialNo, int readNo, vector<uint8_t> const &SEQ, vector<uint8_t> const &QUAL)
{
    ostream &out = *settings->fastqStream;
    
    out << '@' << serialNo << '/' << readNo << endl;
    printSequence(out, SEQ, false) << endl << '+' << endl;
    printQuality(out, QUAL, false) << endl;
}

static void printFastQ(uint64_t serialNo, vector<uint8_t> const &SEQ1, vector<uint8_t> const &SEQ2, vector<uint8_t> const &QUAL1, vector<uint8_t> const &QUAL2)
{
    print1FastQ(serialNo, 1, SEQ1, QUAL1);
    print1FastQ(serialNo, 2, SEQ2, QUAL2);
}

static int templateLength(int pos, int mpos, bool reversed)
{
    int const aleft = reversed ? pos + settings->readLength - 1 : pos;
    int const aright = reversed ? pos - 1 : pos + settings->readLength;
    int const bleft = !reversed ? mpos + settings->readLength - 1 : mpos;
    int const bright = !reversed ? mpos - 1 : mpos + settings->readLength;
    int const left = min(aleft, bleft);
    int const right = max(aright, bright);
    int const value = right - left;
    if (aright == right)
        return -value;
    return value;
}

static void printSAM(ostream &out, uint64_t name, int readNo, bool reversed, bool secondary, int pos, int lclip, int rclip, bool hardClip, int mpos, vector<uint8_t> const &SEQ, vector<uint8_t> const &QUAL)
{
    int const FLAG = 0x1 | 0x2 | (reversed ? 0x10 : 0) | (reversed ? 0 : 0x20) | (readNo == 1 ? 0x40 : 0) | (readNo == 2 ? 0x80 : 0) | (secondary ? 0x100 : 0);
    out << name << '\t' << FLAG << '\t' << settings->refName << '\t' << pos + 1 + lclip << '\t' << (secondary ? 3 : 30) << '\t';
    if (lclip)
        out << lclip << (hardClip ? 'H' : 'S');
    out << settings->readLength - lclip - rclip << 'M';
    if (rclip)
        out << rclip << (hardClip ? 'H' : 'S');
    out << "\t=\t" << mpos + 1 << '\t' << templateLength(pos, mpos, reversed) << '\t';
    printSequence(out, SEQ, reversed) << '\t';
    printQuality(out, QUAL, reversed) << endl;
}

static vector<uint8_t> randomQuality()
{
    vector<uint8_t> rslt(settings->readLength, 38);
    
    for (vector<uint8_t>::iterator i = rslt.begin(); i != rslt.end(); ++i) {
        *i = max(10, min(40, int(*i + normalRandom().first * 5)));
    }
    return rslt;
}

static vector<uint8_t> readReference(ref_t const &ref, int pos, bool reverse)
{
    vector<uint8_t> rslt(settings->readLength);
    for (int i = 0; i < settings->readLength; ++i) {
        bool const mutate = -10.0 * log10(frand()) > 30.0;
        uint8_t const base = mutate ? (random() % 4 + 1) : ref[pos + i].base;
        if (reverse) {
            rslt[settings->readLength - 1 - i] = complement(base);
        }
        else {
            rslt[i] = base;
        }
    }
    return rslt;
}

static uint64_t scramble(uint64_t const serialNo)
{
    static uint8_t scramble[256];
    static uint8_t const *mixer = NULL;
    
    if (mixer == NULL) {
        for (int i = 0; i < 256; ++i)
            scramble[i] = i;
        for (int k = 0; k < 256; ++k) {
            for (int i = 0; i < 256; ++i) {
                int const j = random() % 256;
                int const xi = scramble[i];
                int const xj = scramble[j];
                
                scramble[i] = xj;
                scramble[j] = xi;
            }
        }
        mixer = scramble;
    }
    uint64_t rslt = 0;
    uint8_t carry = 0x55;
    for (int i = 0; i < 8; ++i) {
        uint8_t const j = serialNo >> (8 * i);
        uint8_t const k = mixer[j ^ carry];
        rslt = (uint64_t(k) << 56) | (rslt >> 8);
        carry = k;
    }
    return rslt;
}

static pair<unsigned, unsigned> make_ipd_pair(void)
{
    for ( ; ; ) {
        pair<double, double> const r2 = normalRandom();
        int const ipd1 = int(r2.first * settings->templateLength / 10.0) + settings->templateLength;
        int const ipd2 = int(r2.second * settings->templateLength / 100.0) + ipd1;
        if (ipd1 > settings->readLength && ipd2 > settings->readLength)
            return make_pair((unsigned)ipd1, (unsigned)ipd2);
    }
}

static pair<unsigned, unsigned> make_read_pair(unsigned const ipd)
{
/*
 *            |<-------ipd------->|                             |rrrrrrrrr|
 *                                |fffffffff|-------------------|
 *            |-------------------|fffffffff|
 *  |rrrrrrrrr|
 *  ^second                       ^first                        ^second
 */
    for ( ; ; ) {
        double const r1 = frand();
        bool const reversed = r1 < 0;
        unsigned const pos1 = unsigned((reversed ? -r1 : r1) * settings->lengthTarget);
        if (reversed && pos1 > settings->readLength + ipd) {
            unsigned const pos2 = pos1 - settings->readLength - ipd;
            return make_pair(pos1, pos2);
        }
        else {
            unsigned const pos2 = pos1 + settings->readLength + ipd;
            if (pos2 + settings->readLength <= settings->lengthTarget)
                return make_pair(pos1, pos2);
        }
    }
}

static int run(void)
{
    ref_t ref(settings->lengthTarget);
    uint64_t coverage = 0;
    uint64_t serialNo = 0;
    
    for ( ; ; ) {
        pair<unsigned, unsigned> const ipd = make_ipd_pair();
        pair<unsigned, unsigned> const pos = make_read_pair(ipd.first);
        bool const rev = pos.first > pos.second;

        if (isOverCovered(ref, pos.first) || isOverCovered(ref, pos.second))
            continue;
        
        fillReference(ref, pos.first);
        fillReference(ref, pos.second);

        vector<uint8_t> const SEQ1 = readReference(ref, pos.first, rev);
        vector<uint8_t> const SEQ2 = readReference(ref, pos.second, !rev);
        vector<uint8_t> const QUAL1 = randomQuality();
        vector<uint8_t> const QUAL2 = randomQuality();
        uint64_t const name = scramble(++serialNo);
        
        printFastQ(name, SEQ1, SEQ2, QUAL1, QUAL2);
        printSAM(cout, name, 1, rev, false, pos.first, 0, 0, false, pos.second, SEQ1, QUAL1);
        printSAM(cout, name, 2, !rev, false, pos.second, 0, 0, false, pos.first, SEQ2, QUAL2);
        
        coverage += 2 * settings->readLength;

        for ( ; ; ) {
            pair<unsigned, unsigned> const pos = make_read_pair(ipd.second);
            bool const rev = pos.first > pos.second;

            if (isOverCovered(ref, pos.first) || isOverCovered(ref, pos.second))
                break;
            
            int left1, right1;
            covered(ref, pos.first, left1, right1);
            if (right1 <= left1 || (right1 - left1) * 2 < settings->readLength)
                break;

            int left2, right2;
            covered(ref, pos.second, left2, right2);
            if (right2 <= left2 || (right2 - left2) * 2 < settings->readLength)
                break;
            
            copyReference(ref, pos.first, SEQ1, rev);
            copyReference(ref, pos.second, SEQ2, !rev);

            printSAM(cout, name, 1, rev, true, pos.first, left1, settings->readLength - right1, false, pos.second, SEQ1, QUAL1);
            printSAM(cout, name, 2, !rev, true, pos.second, left2, settings->readLength - right2, false, pos.first, SEQ2, QUAL2);

            coverage += 2 * settings->readLength;
            break;
        }

        if (double(coverage)/double(ref.size()) > settings->coverageTarget)
            break;
    }

    printReference(ref);
    return 0;
}

static string getProgName(string const &argv0)
{
    string::size_type const sep = argv0.find_last_of('/');
    if (sep == string::npos)
        return argv0;
    return argv0.substr(sep + 1);
}

static string command_line(string const &progname, int argc, char *argv[])
{
    ostringstream oss;
    
    oss << progname;
    while (++argv, --argc) {
        oss << ' ' << *argv;
    }
    return oss.str();
}

static void usage(string const &progname, bool error)
{
    cerr << "Usage: " << progname;
    for (int i = 0; i < Settings::paramNames().size(); ++i) {
        cout << " [" << Settings::paramNames()[i] << "=<" << Settings::paramTypes()[i] << ">]";
    }
    cout << endl;
    exit(error ? EXIT_FAILURE : EXIT_SUCCESS);
}

static map<string, string> loadArgs(int argc, char *argv[], string const &progname)
{
    map<string, string> args;

    while (++argv, --argc) {
        string const arg(*argv);
        bool found = false;
        
        if (arg == "--help" || arg == "-h" || arg == "-?")
            usage(progname, false);
        
        for (vector<string>::const_iterator i = Settings::paramNames().begin(); i != Settings::paramNames().end(); ++i) {
            if (arg.substr(0, i->length() + 1) == *i + "=") {
                args[*i] = arg.substr(i->length() + 1);
                found = true;
                break;
            }
        }
        if (!found) {
            cerr << "Error: unknown parameter: '" << arg << "'" << endl;
            usage(progname, true);
        }
    }
    return args;
}

int main(int argc, char *argv[])
{
    string const progname = getProgName(argv[0]);
    string const CL = command_line(progname, argc, argv);
    map<string, string> args = loadArgs(argc, argv, progname);
    
    // set defaults
    (void)args.insert(make_pair("refname", "R"));
    (void)args.insert(make_pair("fastq", progname + ".fastq"));
    (void)args.insert(make_pair("coverage", "30"));
    (void)args.insert(make_pair("length", "500000"));
    
    try {
        settings = new Settings(args);
    }
    catch (logic_error const &e) {
        cerr << "Error: " << e.what() << endl;
        usage(progname, true);
    }

    cout << "@HD\tVN:1.0\tSO:queryname" << endl;
    cout << "@SQ\tSN:" << args["refname"] << "\tLN:" << args["length"] << "\tUR:" << args["refname"] << ".fasta" << endl;
    cout << "@PG\tID:1\tPN:" << progname << "\tCL:" << CL << endl;
    cout << "@CO\targuments: { ";
    for (map<string, string>::const_iterator i = args.begin(); i != args.end(); ++i) {
        cout << i->first << "=" << i->second << "; ";
    }
    cout << "}" << endl;
    
    srandom((unsigned long)(time(0)));
    return run();
}

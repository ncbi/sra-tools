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
    
    char *endp = NULL;
    auto const value = strtoul(str.c_str(), &endp, base);
    
    if (*endp != '\0')
        throw invalid_argument("'" + str + "' is not convertible to an integer");
    
    if (value > unsigned(-1L))
        throw range_error("'" + str + "' is too big");

    return unsigned(value);
}

struct Settings {
    ostream *refSeqStream;
    ostream *fastqStream;
    
    int const templateLength;
    int const readLength;
    int const coverageTarget;
    int const lengthTarget;
    string const &refName;
    
    Settings(map<string, string> &args)
    : templateLength(600)
    , readLength(150)
    , coverageTarget(stringtoui(args["coverage"]))
    , lengthTarget(stringtoui(args["length"]))
    , refName(args["refname"])
    {
        refSeqStream = new ofstream((refName + ".fasta").c_str());
        fastqStream = new ofstream(args["fastq"].c_str());
    }
    ~Settings() {
        delete refSeqStream;
        delete fastqStream;
    }
    
    static vector<string> const &paramNames() {
        static auto const value = vector<string>({
            "refname", "fastq", "coverage", "length"
        });
        return value;
    }
    static vector<string> const &paramTypes() {
        static auto const value = vector<string>({
            "name", "file", "integer", "integer"
        });
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
        auto const x = frand();
        auto const y = frand();
        auto const r = x * x + y * y;
        if (0.0 < r && r <= 1.0) {
            auto const scale = sqrt(-2.0 * log(r) / r);
            return make_pair(x * scale, y * scale);
        }
    }
}

class Quality {
    struct Phred {
        uint8_t value;

        Phred() : value(2) {}
        explicit Phred(uint8_t const other) : value(other) {}
        char Char() const {
            return value + 33;
        }
        static Phred random() {
            for (;;) {
                auto const value = int(33 + normalRandom().first * 5);
                if (3 <= value && value <= 40)
                    return Phred(value);
            }
        }
    };
    typedef vector<Phred> vector_t;
    
    vector_t vec;
    explicit Quality(vector_t const &other) : vec(other) {}
public:
    ostream &print(ostream &stream, bool rev) const {
        if (rev) {
            for (auto i = vec.rbegin(); i != vec.rend(); ++i) {
                stream << i->Char();
            }
        }
        else {
            for (auto i : vec) {
                stream << i.Char();
            }
        }
        return stream;
    }
    static Quality random() {
        auto rslt = vector_t(settings->readLength);
        for (auto i = 0; i < settings->readLength; ++i) {
            rslt[i] = Phred::random();
        }
        return Quality(rslt);
    }
};

struct Base {
    uint8_t value;

    Base() : value(0) {}
    explicit Base(uint8_t newValue) : value(newValue) {}
    char DNA() const {
        static char const tr[] = "NACGT";
        return tr[value];
    }
    Base complement() const {
        return Base(value == 0 ? 0 : (5 - value));
    }
    static Base random() {
        auto const GC = frand() > 0.2; // GC or AT; is biased, target GC content is 40%
        auto const AC = frand() > 0.0; // A or C
        return Base(GC ? (AC ? 2 : 3) : (AC ? 1 : 4));
    }
};

class Sequence {
    friend class RefSequence;

    typedef vector<Base> vector_t;
    
    vector_t vec;
    
    explicit Sequence(vector_t const &other) : vec(other) {}
public:
    Base operator[](int i) const { return vec[i]; }
    ostream &print(ostream &stream, bool rev) const {
        if (rev) {
            for (auto i = vec.rbegin(); i != vec.rend(); ++i) {
                stream << i->complement().DNA();
            }
        }
        else {
            for (auto i : vec) {
                stream << i.DNA();
            }
        }
        return stream;
    }
};

class RefSequence {
    struct RefBase {
        Base base;
        uint8_t count;
        
        RefBase() : base(0), count(0) {}
        void increaseCoverageCount() {
            if (count < 0xFF)
                ++count;
        }
    };
    typedef vector<RefBase> vector_t;
    
    vector_t vec;
public:
    RefSequence()
    : vec(vector_t(settings->lengthTarget))
    {}
    
    void fill(int const pos) {
        auto const N = settings->readLength;
        for (auto i = 0; i < N; ++i) {
            if (vec[pos + i].count == 0)
                vec[pos + i].base = Base::random();
            vec[pos + i].increaseCoverageCount();
        }
    }
    
    void copy(int const pos, Sequence const &from, bool rev) {
        auto const N = settings->readLength;
        for (auto i = 0; i < N; ++i) {
            if (vec[pos + i].count == 0) {
                auto const j = rev ? (N - i - 1) : i;
                auto const value = from[j];
                auto const mutate = log10(frand()) < -1.5;
                vec[pos + i].base = mutate ? Base::random() : rev ? value.complement() : value;
            }
            vec[pos + i].increaseCoverageCount();
        }
    }
    
    size_t size() const {
        return vec.size();
    }
    void print() const {
        auto &out = *settings->refSeqStream;
        auto j = 0;
        
        out << '>' << settings->refName << endl;
        for (auto i : vec) {
            out << i.base.DNA();
            if (++j == 70) {
                j = 0;
                out << endl;
            }
        }
        if (j != 0)
            out << endl;
    }
    
    bool isOverCovered(int const pos) const {
        auto const N = settings->readLength;
        auto const max = 2 * settings->coverageTarget;
        for (auto i = 0; i < N; ++i) {
            if (vec[pos + i].count > max)
                return true;
        }
        return false;
    }
    
    int nextNotCovered(int const pos) const {
        auto const N = settings->readLength;
        for (auto i = 0; i < N; ++i) {
            if (vec[pos + i].count == 0)
                return i;
        }
        return N;
    }
    
    int prevNotCovered(int const pos) const {
        auto const N = settings->readLength;
        for (auto i = N; i > 0; --i) {
            if (vec[pos + i - 1].count == 0)
                return i;
        }
        return 0;
    }
    
    Sequence read(int const pos, bool rev)
    {
        auto rslt = vector<Base>(settings->readLength);
        auto const N = settings->readLength;
        for (auto i = 0; i < N; ++i) {
            auto const mutate = log10(frand()) < -3.0;
            auto const base = mutate ? Base::random() : vec[pos + i].base;
            if (rev) {
                rslt[N - 1 - i] = base.complement();
            }
            else {
                rslt[i] = base;
            }
        }
        return Sequence(rslt);
    }

};

static void print1FastQ(uint64_t serialNo, int readNo, Sequence const &SEQ, Quality const &QUAL)
{
    auto &out = *settings->fastqStream;
    
    out << '@' << serialNo << '/' << readNo << endl;
    SEQ.print(out, false) << endl << '+' << endl;
    QUAL.print(out, false) << endl;
}

static void printFastQ(uint64_t serialNo, Sequence const &SEQ1, Sequence const &SEQ2, Quality const &QUAL1, Quality const &QUAL2)
{
    print1FastQ(serialNo, 1, SEQ1, QUAL1);
    print1FastQ(serialNo, 2, SEQ2, QUAL2);
}

static int templateLength(int pos, int mpos, bool reversed)
{
    auto const aleft = reversed ? pos + settings->readLength - 1 : pos;
    auto const aright = reversed ? pos - 1 : pos + settings->readLength;
    auto const bleft = !reversed ? mpos + settings->readLength - 1 : mpos;
    auto const bright = !reversed ? mpos - 1 : mpos + settings->readLength;
    auto const left = min(aleft, bleft);
    auto const right = max(aright, bright);
    auto const value = right - left;
    if (aright == right)
        return -value;
    return value;
}

static void printSAM(ostream &out, uint64_t name, int readNo, bool reversed, bool secondary, int pos, int lclip, int rclip, bool hardClip, int mpos, Sequence const &SEQ, Quality const &QUAL)
{
    auto const FLAG = 0x1 | 0x2 | (reversed ? 0x10 : 0) | (reversed ? 0 : 0x20) | (readNo == 1 ? 0x40 : 0) | (readNo == 2 ? 0x80 : 0) | (secondary ? 0x100 : 0);
    out << name << '\t' << FLAG << '\t' << settings->refName << '\t' << pos + 1 + lclip << '\t' << (secondary ? 3 : 30) << '\t';
    if (lclip)
        out << lclip << (hardClip ? 'H' : 'S');
    out << settings->readLength - lclip - rclip << 'M';
    if (rclip)
        out << rclip << (hardClip ? 'H' : 'S');
    out << "\t=\t" << mpos + 1 << '\t' << templateLength(pos, mpos, reversed) << '\t';
    SEQ.print(out, reversed) << '\t';
    QUAL.print(out, reversed) << endl;
}

static uint64_t scramble(uint64_t const serialNo)
{
    static uint8_t scramble[256];
    static uint8_t const *mixer = NULL;
    
    if (mixer == NULL) {
        for (int i = 0; i < 256; ++i) {
            auto const j = random() % (i + 1);
            scramble[i] = scramble[j];
            scramble[j] = i;
        }
        mixer = scramble;
    }
    auto rslt = serialNo;
    auto carry = 0x55;
    for (auto i = 0; i < 8; ++i) {
        auto const k = mixer[uint8_t(rslt ^ carry)];
        rslt = (uint64_t(k) << 56) | (rslt >> 8);
        carry = k;
    }
    return rslt;
}

static pair<unsigned, unsigned> make_ipd_pair(void)
{
    for ( ; ; ) {
        auto const r2 = normalRandom();
        auto const ipd1 = r2.first * settings->templateLength / 10.0 + settings->templateLength;
        auto const ipd2 = r2.second * settings->templateLength / 100.0 + ipd1;
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
        auto const r1 = frand();
        auto const reversed = r1 < 0;
        auto const pos1 = (reversed ? -r1 : r1) * settings->lengthTarget;
        if (reversed && pos1 > settings->readLength + ipd) {
            auto const pos2 = pos1 - settings->readLength - ipd;
            return make_pair(pos1, pos2);
        }
        else {
            auto const pos2 = pos1 + settings->readLength + ipd;
            if (pos2 + settings->readLength <= settings->lengthTarget)
                return make_pair(pos1, pos2);
        }
    }
}

static void run(void)
{
    auto ref = RefSequence();
    uint64_t serialNo = 0;
    
    for (double coverage = 0.0; coverage/double(ref.size()) < settings->coverageTarget; ) {
        auto const ipd = make_ipd_pair();
        auto const pos = make_read_pair(ipd.first);
        auto const rev = pos.first > pos.second;

        if (ref.isOverCovered(pos.first) || ref.isOverCovered(pos.second))
            continue;
        
        ref.fill(pos.first);
        ref.fill(pos.second);

        auto const SEQ1 = ref.read(pos.first, rev);
        auto const SEQ2 = ref.read(pos.second, !rev);
        auto const QUAL1 = Quality::random();
        auto const QUAL2 = Quality::random();
        auto const name = scramble(++serialNo);
        
        printFastQ(name, SEQ1, SEQ2, QUAL1, QUAL2);
        printSAM(cout, name, 1, rev, false, pos.first, 0, 0, false, pos.second, SEQ1, QUAL1);
        printSAM(cout, name, 2, !rev, false, pos.second, 0, 0, false, pos.first, SEQ2, QUAL2);
        
        coverage += 2 * settings->readLength;

        for ( ; ; ) {
            auto const pos = make_read_pair(ipd.second);
            auto const rev = pos.first > pos.second;

            if (ref.isOverCovered(pos.first) || ref.isOverCovered(pos.second))
                break;
            
            auto const left1 = ref.nextNotCovered(pos.first);
            auto const right1 = ref.prevNotCovered(pos.first);
            if (right1 <= left1 || (right1 - left1) * 2 < settings->readLength)
                break;

            auto const left2 = ref.nextNotCovered(pos.second);
            auto const right2 = ref.prevNotCovered(pos.second);
            if (right2 <= left2 || (right2 - left2) * 2 < settings->readLength)
                break;
            
            ref.copy(pos.first, SEQ1, rev);
            ref.copy(pos.second, SEQ2, !rev);

            printSAM(cout, name, 1, rev, true, pos.first, left1, settings->readLength - right1, false, pos.second, SEQ1, QUAL1);
            printSAM(cout, name, 2, !rev, true, pos.second, left2, settings->readLength - right2, false, pos.first, SEQ2, QUAL2);

            coverage += 2 * settings->readLength;
            break;
        }
    }
    ref.print();
}

static string getProgName(string const &argv0)
{
    auto const sep = argv0.find_last_of('/');
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
        auto const arg = string(*argv);
        bool found = false;
        
        if (arg == "--help" || arg == "-h" || arg == "-?")
            usage(progname, false);
        
        for (auto i : Settings::paramNames()) {
            if (arg.substr(0, i.length() + 1) == i + "=") {
                args[i] = arg.substr(i.length() + 1);
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
    auto const progname = getProgName(argv[0]);
    auto const CL = command_line(progname, argc, argv);
    auto args = loadArgs(argc, argv, progname);
    
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
    for (auto i : args) {
        cout << i.first << "=" << i.second << "; ";
    }
    cout << "}" << endl;
    
    srandom((unsigned)(time(0)));
    run();
    delete settings;
    return 0;
}

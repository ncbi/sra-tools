#include "quality-quantizer.hpp"
#include <cctype>

static void setLookupTable(int tbl[256], int const value, unsigned const start = 0, unsigned const end = 256)
{
    for (unsigned i = start; i < end; ++i) {
        tbl[i] = value;
    }
}

static void clearLookupTable(int tbl[256])
{
    setLookupTable(tbl, -1);
}

static bool initLookupTable(int tbl[256], char const quant[])
{
    unsigned i = 0;
    unsigned limit = 0;
    int value = -1;
    int ws = 1;
    int st = 0;
    
    clearLookupTable(tbl);
    for (unsigned cur = 0; quant[cur] != 0; ++cur) {
        int const ch = quant[cur];
        
        if (ws) {
            if (isspace(ch))
                continue;
            ws = false;
        }
        switch (st) {
        case 0:
            if (isdigit(ch)) {
                value = (value * 10) + ch - '0';
                break;
            }
            else if (isspace(ch)) {
                ++st;
                ws = true;
                break;
            }
            ++st;
            /* no break */
        case 1:
            if (ch != ':')
                return false;
            ws = true;
            ++st;
            break;
        case 2:
            if (isdigit(ch)) {
                limit  = (limit * 10) + ch - '0';
                break;
            }
            else if (isspace(ch)) {
                ++st;
                ws = true;
                break;
            }
            else if (ch == '-' && limit == 0) {
                setLookupTable(tbl, value, i);
                return true;
            }
            ++st;
            /* no break */
        case 3:
            if (ch != ',')
                return false;
            ws = true;
            st = 0;
            if (i > limit)
                return false;
            setLookupTable(tbl, value, i, limit);
            i = limit;
            limit = value = 0;
            break;
        }
    }
    if (st == 0) {
        switch (value) {
        case 0:
            for (unsigned i = 0; i < 256; ++i) {
                tbl[i] = i;
            }
            return true;
        case 1:
            setLookupTable(tbl,  1,  0, 10);
            setLookupTable(tbl, 10, 10, 20);
            setLookupTable(tbl, 20, 20, 30);
            setLookupTable(tbl, 30, 30);
            return true;
        case 2:
            setLookupTable(tbl,  1,  0, 30);
            setLookupTable(tbl, 30, 30);
            return true;
        }
    }
    return false;
}

QualityQuantizer::QualityQuantizer(char const spec[])
{
    if (!initLookupTable(lookup, spec))
        clearLookupTable(lookup);
}

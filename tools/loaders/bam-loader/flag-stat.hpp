#include <array>
#include <utility>
#include <cstdint>

class FlagStat {
    struct Counter {
        using Array = std::array<uint64_t, 16>;
        Array counter;
        
        Counter() {
            for (auto && c : counter)
                c = 0;
        }
        void add(uint16_t bits) {
            Array x;
            for (auto && i : x) {
                i = bits & 1;
                bits >>= 1;
            }
            for (auto i = 0; i < 16; ++i)
                counter[i] += x[i];
        }
        void copy(uint64_t counts[16]) const {
            std::copy(counter.cbegin(), counter.cend(), counts); 
        }
    };
    Counter raw, canonical;
public:
    void add(uint16_t bits) {
        uint16_t unmask = 0xF000;        
        if ((bits & 0x001) == 0) {
            unmask |= 0x002 | 0x008 | 0x020 | 0x040 | 0x080; 
        }
        if ((bits & 0x004) != 0) {
            unmask |= 0x800 | 0x100 | 0x002;
        }
        uint16_t const mask = ~unmask;

        raw.add(bits);
        canonical.add(bits & mask);
    }
    void rawCounts(uint64_t counts[16]) const {
        raw.copy(counts);
    }
    void canonicalCounts(uint64_t counts[16]) const {
        canonical.copy(counts);
    }
};

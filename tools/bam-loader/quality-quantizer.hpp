#include <cstdint>

class QualityQuantizer {
    int lookup[256];
public:
    QualityQuantizer(char const spec[]);
    int quantize(int const value) const {
        return (0 <= value && value < 256) ? lookup[value] : -1;
    }
};

/* utf8-like-int-codec.h */

#ifndef def_utf8_like_int_codec
#define def_utf8_like_int_codec

#include <stdint.h>

#ifndef _WIN32
#include <stdlib.h> /* size_t */
#endif


enum
{
    CODEC_INSUFFICIENT_BUFFER = 0,
    CODEC_INVALID_FORMAT = -1,
    CODEC_UNKNOWN_ERROR = -2
};

/*
all encode_uintXX return:
    value <= 0: error:   one of CODEC_* above
    value > 0:  success: number of bytes written to buf_start

all decode_uintXX return:
    value <= 0: error:   one of CODEC_* above
    value > 0:  success: number of bytes read from buf_start
*/

int64_t encode_uint16 ( uint16_t value_to_encode, uint8_t* buf_start, uint8_t* buf_xend );
int64_t decode_uint16 ( uint8_t const* buf_start, uint8_t const* buf_xend, uint16_t* ret_decoded );

int64_t encode_uint32 ( uint32_t value_to_encode, uint8_t* buf_start, uint8_t* buf_xend );
int64_t decode_uint32 ( uint8_t const* buf_start, uint8_t const* buf_xend, uint32_t* ret_decoded );

int64_t encode_uint64 ( uint64_t value_to_encode, uint8_t* buf_start, uint8_t* buf_xend );
int64_t decode_uint64 ( uint8_t const* buf_start, uint8_t const* buf_xend, uint64_t* ret_decoded );


#endif /* def_utf8_like_int_codec */


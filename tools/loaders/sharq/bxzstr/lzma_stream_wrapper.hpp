/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * This file is a part of bxzstr (https://github.com/tmaklin/bxzstr)
 * Written by Tommi Mäklin (tommi@maklin.fi) */

#if defined(BXZSTR_LZMA_SUPPORT) && (BXZSTR_LZMA_SUPPORT) == 1

#ifndef BXZSTR_LZMA_STREAM_WRAPPER_HPP
#define BXZSTR_LZMA_STREAM_WRAPPER_HPP

#include <lzma.h>

#include <string>
#include <sstream>
#include <exception>

#include "stream_wrapper.hpp"

namespace bxz {
/// Exception class thrown by failed liblzma operations.
class lzmaException : public std::exception {
  public:
    lzmaException(const lzma_ret ret) : _msg("liblzma: ") {
        switch (ret) {
            case LZMA_MEM_ERROR:
		_msg += "LZMA_MEM_ERROR: ";
		break;
            case LZMA_OPTIONS_ERROR:
		_msg += "LZMA_OPTIONS_ERROR: ";
		break;
            case LZMA_UNSUPPORTED_CHECK:
		_msg += "LZMA_UNSUPPORTED_CHECK: ";
		break;
            case LZMA_PROG_ERROR:
		_msg += "LZMA_PROG_ERROR: ";
		break;
            case LZMA_BUF_ERROR:
		_msg += "LZMA_BUF_ERROR: ";
		break;
            case LZMA_DATA_ERROR:
		_msg += "LZMA_DATA_ERROR: ";
		break;
            case LZMA_FORMAT_ERROR:
		_msg += "LZMA_FORMAT_ERROR: ";
		break;
            case LZMA_NO_CHECK:
		_msg += "LZMA_NO_CHECK: ";
		break;
            case LZMA_MEMLIMIT_ERROR:
		_msg += "LZMA_MEMLIMIT_ERROR: ";
		break;
            default:
		std::ostringstream oss;
		oss << ret;
		_msg += "[" + oss.str() + "]: ";
		break;
        }
        _msg += ret;
    }
    lzmaException(const std::string msg) : _msg(msg) {}

    const char * what() const noexcept { return _msg.c_str(); }

  private:
    std::string _msg;
}; // class lzmaException

namespace detail {
class lzma_stream_wrapper : public lzma_stream, public stream_wrapper {
  public:
    lzma_stream_wrapper(const bool _is_input = true, const int _level = 2, const int _flags = 0)
	    : lzma_stream(LZMA_STREAM_INIT), is_input(_is_input) {
	lzma_ret ret;
	if (is_input) {
	    lzma_stream::avail_in = 0;
	    lzma_stream::next_in = NULL;
	    ret = lzma_auto_decoder(this, UINT64_MAX, _flags);
	} else {
	    ret = lzma_easy_encoder(this, _level, LZMA_CHECK_CRC64);
	}
	if (ret != LZMA_OK) throw lzmaException(ret);
    }
    ~lzma_stream_wrapper() { lzma_end(this); }

    int decompress(const int) override {
	ret = lzma_code(this, LZMA_RUN);
	if (ret != LZMA_OK && ret != LZMA_STREAM_END && ret) throw lzmaException(ret);
	return (int)ret;
    }
    int compress(const int _flags = LZMA_RUN) override {
	ret = lzma_code(this, (lzma_action)_flags);
	if (ret != LZMA_OK && ret != LZMA_STREAM_END && ret != LZMA_BUF_ERROR)
	    throw lzmaException(ret);
	return (int)ret;
    }
    bool stream_end() const override { return this->ret == LZMA_STREAM_END; }
    bool done() const override { return (this->ret == LZMA_BUF_ERROR || this->stream_end()); }

    const uint8_t* next_in() const override { return lzma_stream::next_in; }
    long avail_in() const override { return lzma_stream::avail_in; }
    uint8_t* next_out() const override { return lzma_stream::next_out; }
    long avail_out() const override { return lzma_stream::avail_out; }

    void set_next_in(const unsigned char* in) override { lzma_stream::next_in = in; }
    void set_avail_in(long in) override { lzma_stream::avail_in = in; }
    void set_next_out(const uint8_t* in) override { lzma_stream::next_out = const_cast<uint8_t*>(in); }
    void set_avail_out(long in) override { lzma_stream::avail_out = in; }

  private:
    bool is_input;
    lzma_ret ret;
}; // class lzma_stream_wrapper
} // namespace detail
} // namespace bxz
#endif
#endif

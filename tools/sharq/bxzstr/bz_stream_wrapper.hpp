/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * This file is a part of bxzstr (https://github.com/tmaklin/bxzstr)
 * Written by Tommi Mäklin (tommi@maklin.fi) */

#if defined(BXZSTR_BZ2_SUPPORT) && (BXZSTR_BZ2_SUPPORT) == 1

#ifndef BXZSTR_BZ_STREAM_WRAPPER_HPP
#define BXZSTR_BZ_STREAM_WRAPPER_HPP

#include <bzlib.h>

#include <string>
#include <sstream>
#include <exception>

#include "stream_wrapper.hpp"

namespace bxz {
/// Exception class thrown by failed bzlib operations.
class bzException : public std::exception {
  public:
    bzException(const int ret) : _msg("bzlib: ") {
	switch (ret) {
            case BZ_CONFIG_ERROR:
                _msg += "BZ_CONFIG_ERROR: ";
	        break;
	    case BZ_SEQUENCE_ERROR:
		_msg += "BZ_SEQUENCE_ERROR: ";
		break;
            case BZ_PARAM_ERROR:
		_msg += "BZ_PARAM_ERROR: ";
		break;
            case BZ_MEM_ERROR:
		_msg += "BZ_MEM_ERROR: ";
		break;
            case BZ_DATA_ERROR:
		_msg += "BZ_DATA_ERROR: ";
		break;
            case BZ_DATA_ERROR_MAGIC:
		_msg += "BZ_DATA_ERROR_MAGIC: ";
		break;
            case BZ_IO_ERROR:
		_msg += "BZ_IO_ERROR: ";
		break;
            case BZ_UNEXPECTED_EOF:
		_msg += "BZ_UNEXPECTED_EOF: ";
		break;
            case BZ_OUTBUFF_FULL:
		_msg += "BZ_OUTBUFF_FULL: ";
		break;
            default:
		std::ostringstream oss;
		oss << ret;
		_msg += "[" + oss.str() + "]: ";
		break;
        }
        _msg += ret;
    }
    bzException(const std::string msg) : _msg(msg) {}

    const char * what() const noexcept { return _msg.c_str(); }

  private:
    std::string _msg;
}; // class bzException

namespace detail {
class bz_stream_wrapper : public bz_stream, public stream_wrapper {
  public:
    bz_stream_wrapper(const bool _is_input = true, const int _level = 9, const int _wf = 30)
            : is_input(_is_input) {
	bz_stream::next_in = new char();
	bz_stream::next_out = new char();
	this->bzalloc = NULL;
	this->bzfree = NULL;
	this->opaque = NULL;
	if (is_input) {
	    bz_stream::avail_in = 0;
	    bz_stream::next_in = NULL;
	    ret = BZ2_bzDecompressInit(this, 0, 0);
	} else {
	    ret = BZ2_bzCompressInit(this, _level, 0, _wf);
	}
	if (ret != BZ_OK) throw bzException(ret);
    }
    ~bz_stream_wrapper() {
	if (is_input) {
	    BZ2_bzDecompressEnd(this);
	} else {
	    BZ2_bzCompressEnd(this);
	}
    }

    int decompress(const int) override {
	ret = BZ2_bzDecompress(this);
	if (ret != BZ_OK && ret != BZ_STREAM_END) throw bzException(ret);
	return ret;
    }
    int compress(const int _flags = BZ_RUN) override {
	ret = BZ2_bzCompress(this, _flags);
	if (!ret) throw bzException(ret);
	return ret;
    }
    bool stream_end() const override { return this->ret == BZ_STREAM_END; }
    bool done() const override { return this->stream_end(); }

    const uint8_t* next_in() const override { return (uint8_t*)bz_stream::next_in; }
    long avail_in() const override { return bz_stream::avail_in; }
    uint8_t* next_out() const override { return (uint8_t*)bz_stream::next_out; }
    long avail_out() const override { return bz_stream::avail_out; }

    void set_next_in(const unsigned char* in) override { bz_stream::next_in = (char*)in; }
    void set_avail_in(const long in) override { bz_stream::avail_in = in; }
    void set_next_out(const uint8_t* in) override { bz_stream::next_out = (char*)in; }
    void set_avail_out(const long in) override { bz_stream::avail_out = in; }

  private:
    bool is_input;
    int ret;
}; // class bz_stream_wrapper
} // namespace detail
} // namespace bxz

#endif
#endif

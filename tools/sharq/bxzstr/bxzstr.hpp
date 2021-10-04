/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * This file is a part of bxzstr (https://github.com/tmaklin/bxzstr)
 * Written by Tommi Mäklin (tommi@maklin.fi) based on the zstr.hpp
 * file from the zstr project (https://github.com/mateidavid/zstr)
 * written by Matei David (https://github.com/mateidavid). */

#include "config.hpp"

#ifndef BXZSTR_BXZSTR_HPP
#define BXZSTR_BXZSTR_HPP

#include <fstream>
#include <memory>

#include "stream_wrapper.hpp"
#include "strict_fstream.hpp"
#include "compression_types.hpp"

namespace bxz {
class istreambuf : public std::streambuf {
  public:
    istreambuf(std::streambuf * _sbuf_p, std::size_t _buff_size = default_buff_size,
	       bool _auto_detect = true)
            : sbuf_p(_sbuf_p),
	      strm_p(nullptr),
	      buff_size(_buff_size),
	      auto_detect(_auto_detect),
	      auto_detect_run(false) {
        assert(sbuf_p);
        in_buff = new char [buff_size];
        in_buff_start = in_buff;
        in_buff_end = in_buff;
        out_buff = new char [buff_size];
        setg(out_buff, out_buff, out_buff);
    }
    istreambuf(const istreambuf &) = delete;
    istreambuf(istreambuf &&) = default;
    istreambuf & operator = (const istreambuf &) = delete;
    istreambuf & operator = (istreambuf &&) = default;
    virtual ~istreambuf() {
        delete [] in_buff;
        delete [] out_buff;
    }

    virtual std::streambuf::int_type underflow() {
        if (this->gptr() == this->egptr()) {
            // pointers for free region in output buffer
            char * out_buff_free_start = out_buff;
            do {
                // read more input if none available
                if (in_buff_start == in_buff_end) {
                    // empty input buffer: refill from the start
                    in_buff_start = in_buff;
                    std::streamsize sz = sbuf_p->sgetn(in_buff, buff_size);
                    in_buff_end = in_buff + sz;
                    if (in_buff_end == in_buff_start) break; // end of input
                }
                // auto detect if the stream contains text or deflate data
                if (auto_detect && ! auto_detect_run) {
		    this->type = detect_type(in_buff_start, in_buff_end);
		    this->auto_detect_run = true;
		}
                if (this->type == plaintext) {
                    // simply swap in_buff and out_buff, and adjust pointers
                    assert(in_buff_start == in_buff);
                    std::swap(in_buff, out_buff);
                    out_buff_free_start = in_buff_end;
                    in_buff_start = in_buff;
                    in_buff_end = in_buff;
                } else {
                    // run inflate() on input
		    if (! strm_p) init_stream(this->type, true, &strm_p);
		    strm_p->set_next_in(reinterpret_cast< decltype(strm_p->next_in()) >(in_buff_start));
		    strm_p->set_avail_in(in_buff_end - in_buff_start);
		    strm_p->set_next_out(reinterpret_cast< decltype(strm_p->next_out()) >(out_buff_free_start));
		    strm_p->set_avail_out((out_buff + buff_size) - out_buff_free_start);
		    strm_p->decompress();
                    // update in&out pointers following inflate()
		    auto tmp = const_cast< unsigned char* >(strm_p->next_in()); // cast away const qualifiers
                    in_buff_start = reinterpret_cast< decltype(in_buff_start) >(tmp);
                    in_buff_end = in_buff_start + strm_p->avail_in();
                    out_buff_free_start = reinterpret_cast< decltype(out_buff_free_start) >(strm_p->next_out());
                    assert(out_buff_free_start + strm_p->avail_out() == out_buff + buff_size);
                    // if stream ended, deallocate inflator
                    if (strm_p->stream_end()) strm_p.reset();
                }
            } while (out_buff_free_start == out_buff);
            // 2 exit conditions:
            // - end of input: there might or might not be output available
            // - out_buff_free_start != out_buff: output available
            this->setg(out_buff, out_buff, out_buff_free_start);
        }
        return this->gptr() == this->egptr()
	    ? traits_type::eof() : traits_type::to_int_type(*this->gptr());
    }
  private:
    std::streambuf* sbuf_p;
    char* in_buff;
    char* in_buff_start;
    char* in_buff_end;
    char* out_buff;
    std::unique_ptr<detail::stream_wrapper> strm_p;
    std::size_t buff_size;
    bool auto_detect;
    bool auto_detect_run;
    Compression type;

    static const std::size_t default_buff_size = (std::size_t)1 << 20;
}; // class istreambuf

class ostreambuf : public std::streambuf {
  public:
    ostreambuf(std::streambuf * _sbuf_p, Compression type, int _level = 6,
               std::size_t _buff_size = default_buff_size)
            : sbuf_p(_sbuf_p),
              buff_size(_buff_size),
              type(type),
              level(_level) {
        assert(sbuf_p);
        in_buff = new char [buff_size];
        out_buff = new char [buff_size];
        setp(in_buff, in_buff + buff_size);
	init_stream(this->type, false, this->level, &strm_p);
    }
    ostreambuf(const ostreambuf &) = delete;
    ostreambuf(ostreambuf &&) = default;
    ostreambuf & operator = (const ostreambuf &) = delete;
    ostreambuf & operator = (ostreambuf &&) = default;

    int deflate_loop(const int action) {
        while (true) {
            strm_p->set_next_out(reinterpret_cast< decltype(strm_p->next_out()) >(out_buff));
            strm_p->set_avail_out(buff_size);
	    strm_p->compress(action);

            std::streamsize sz = sbuf_p->sputn(out_buff, reinterpret_cast< decltype(out_buff) >(strm_p->next_out()) - out_buff);
            if (sz != reinterpret_cast< decltype(out_buff) >(strm_p->next_out()) - out_buff) {
                // there was an error in the sink stream
                return -1;
            }
	    if (strm_p->done() || sz == 0) break;
        }
        return 0;
    }

    virtual ~ostreambuf() {
        // flush the lzma stream
        //
        // NOTE: Errors here (sync() return value not 0) are ignored, because we
        // cannot throw in a destructor. This mirrors the behaviour of
        // std::basic_filebuf::~basic_filebuf(). To see an exception on error,
        // close the ofstream with an explicit call to close(), and do not rely
        // on the implicit call in the destructor.
        //
        sync();
        delete [] in_buff;
        delete [] out_buff;
    }
    virtual std::streambuf::int_type overflow(std::streambuf::int_type c = traits_type::eof()) {
        strm_p->set_next_in(reinterpret_cast< decltype(strm_p->next_in()) >(pbase()));
        strm_p->set_avail_in(pptr() - pbase());
        while (strm_p->avail_in() > 0) {
            int r = deflate_loop(bxz_run(this->type));
            if (r != 0) {
                setp(nullptr, nullptr);
                return traits_type::eof();
            }
        }
        setp(in_buff, in_buff + buff_size);
        return traits_type::eq_int_type(c, traits_type::eof()) ? traits_type::eof() : sputc(c);
    }
    virtual int sync() {
        // first, call overflow to clear in_buff
        overflow();
        if (! pptr()) return -1;
        // then, call deflate asking to finish the zlib stream
        strm_p->set_next_in(nullptr);
        strm_p->set_avail_in(0);
        if (deflate_loop(bxz_finish(this->type)) != 0) return -1;
	init_stream(this->type, false, this->level, &strm_p);
        return 0;
    }

  private:
    std::streambuf* sbuf_p;
    char* in_buff;
    char* out_buff;
    std::unique_ptr<detail::stream_wrapper> strm_p;
    std::size_t buff_size;
    Compression type;
    int level;

    static const std::size_t default_buff_size = (std::size_t)1 << 20;
}; // class ostreambuf

class istream : public std::istream {
  public:
    istream(std::istream & is) : std::istream(new istreambuf(is.rdbuf())) {
        exceptions(std::ios_base::badbit);
    }
    explicit istream(std::streambuf * sbuf_p) : std::istream(new istreambuf(sbuf_p)) {
        exceptions(std::ios_base::badbit);
    }
    virtual ~istream() { delete rdbuf(); }
}; // class istream

class ostream : public std::ostream {
  public:
    ostream(std::ostream & os, Compression type = plaintext, int level = 6)
	    : std::ostream(new ostreambuf(os.rdbuf(), type, level)) {
	exceptions(std::ios_base::badbit);
    }
    explicit ostream(std::streambuf * sbuf_p, Compression type = z, int level = 6)
	    : std::ostream(new ostreambuf(sbuf_p, type, level)) {
	exceptions(std::ios_base::badbit);
    }
    virtual ~ostream() {
        delete rdbuf();
    }
}; // class ostream

namespace detail {
template < typename FStream_Type >
class strict_fstream_holder {
  public:
    strict_fstream_holder() {};
    strict_fstream_holder(const std::string& filename,
			  std::ios_base::openmode mode = std::ios_base::in)
            : _fs(filename, mode) {}

  protected:
    FStream_Type _fs;
}; // class strict_fstream_holder

} // namespace detail

class ifstream : public detail::strict_fstream_holder< strict_fstream::ifstream >,
		 public std::istream {
  public:
    ifstream() : std::istream(new istreambuf(_fs.rdbuf())) {}
    explicit ifstream(const std::string& filename,
		      std::ios_base::openmode mode = std::ios_base::in)
            : detail::strict_fstream_holder< strict_fstream::ifstream >(filename, mode),
            std::istream(new istreambuf(_fs.rdbuf())),
	    filename(filename),
	    mode(mode) {
        this->setstate(_fs.rdstate());
        exceptions(std::ios_base::badbit);
    }
    ifstream(const ifstream& other) : ifstream(other.filename, other.mode) {}
    virtual ~ifstream() { if (rdbuf()) delete rdbuf(); }


    void open(const std::string &filename,
	      std::ios_base::openmode mode = std::ios_base::in) {
	this->~ifstream();
	new (this) ifstream(filename, mode);
    }
    void open(const char* filename,
	      std::ios_base::openmode mode = std::ios_base::in) {
	this->~ifstream();
	new (this) ifstream(filename, mode);
    }
    bool is_open() const { return _fs.is_open(); }
    void close() { _fs.close(); }

  private:
    std::string filename;
    std::ios_base::openmode mode;
}; // class ifstream

class ofstream : public detail::strict_fstream_holder< strict_fstream::ofstream >,
		 public std::ostream {
  public:
    explicit ofstream(const std::string& filename,
		      std::ios_base::openmode mode = std::ios_base::out,
		      Compression type = z, int level = 6)
            : detail::strict_fstream_holder< strict_fstream::ofstream >(filename, mode | std::ios_base::binary),
            std::ostream(new ostreambuf(_fs.rdbuf(), type, level)),
            filename(filename),
            mode(mode),
            level(level) {
        exceptions(std::ios_base::badbit);
    }
    explicit ofstream(const std::string& filename, Compression type, int level = 6)
              : ofstream(filename, std::ios_base::out, type, level) {}
    ofstream(const ofstream& other)
            : ofstream(other.filename,
	    other.mode,
            other.type,
	    other.level) {}
    virtual ~ofstream() { if (rdbuf()) delete rdbuf(); }
    void open(const std::string &filename,
	      std::ios_base::openmode mode = std::ios_base::in) {
	this->~ofstream();
	new (this) ofstream(filename, mode);
    }
    void open(const char* filename,
	      std::ios_base::openmode mode = std::ios_base::in) {
	this->~ofstream();
	new (this) ofstream(filename, mode);
    }
    bool is_open() const { return _fs.is_open(); }
    void close() { _fs.close(); }

  private:
    std::string filename;
    std::ios_base::openmode mode;
    Compression type;
    int level;
}; // class ofstream
} // namespace bxz

#endif

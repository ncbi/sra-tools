/*===========================================================================
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

#pragma once

/**
 * @file fastq_stream_factory.hpp
 * @brief Factory for creating FASTQ input streams from local or cloud storage
 * 
 * This module provides a drop-in replacement for the existing s_OpenStream()
 * function with extended cloud storage support.
 * 
 * Usage:
 * @code
 * #include "fastq_stream_factory.hpp"
 * 
 * // Using default configuration (reads credentials from environment)
 * auto stream = sharq::open_fastq_stream("s3://my-bucket/sample.fastq.gz");
 * 
 * // Or with explicit configuration
 * sharq::fs::CloudConfig config;
 * config.s3.region = "us-west-2";
 * auto stream = sharq::open_fastq_stream("s3://my-bucket/sample.fastq.gz", config);
 * @endcode
 */

#include <memory>
#include <string>
#include <istream>
#include <stdexcept>

#include "istreambuf_holder.hpp"
#include "cloud_filesystem.hpp"
#include <kfile_stream/kfile_stream.hpp>

namespace sharq {


/**
 * @brief Stream source adapter that wraps CloudInputStream for bxzstr compatibility
 * 
 * This adapter allows CloudInputStream to be used with the existing bxzstr
 * decompression layer, maintaining compatibility with the current codebase.
 */
class CloudStreamSource : public custom_istream::src_interface {
public:
    explicit CloudStreamSource(fs::CloudInputStreamPtr stream)
        : m_stream(std::move(stream))
    {}
    
    size_t read(char* dst, size_t requested) override {
        if (!m_stream || m_stream->has_error()) {
            return static_cast<size_t>(-1);
        }
        
        size_t bytes_read = m_stream->read(dst, requested);
        
        if (m_stream->is_eof() && bytes_read == 0) {
            return static_cast<size_t>(-1);  // Signal EOF to bxzstr
        }
        
        return bytes_read;
    }
    
    fs::CloudInputStream* underlying() const {
        return m_stream.get();
    }
    
private:
    fs::CloudInputStreamPtr m_stream;
};

/**
 * @brief RAII wrapper for CloudFileFactory to ensure proper lifecycle
 */
class CloudFileManager {
public:
    static CloudFileManager& instance() {
        static CloudFileManager mgr;
        return mgr;
    }
    
    fs::CloudFileFactory& factory() {
        return m_factory;
    }
    
    void configure(const fs::CloudConfig& config) {
        m_factory = fs::CloudFileFactory(config);
    }
    
private:
    CloudFileManager() : m_factory(fs::CloudConfig::from_environment()) {}
    fs::CloudFileFactory m_factory;
};

/**
 * @brief Extended istreambuf_holder that tracks cloud metadata
 * 
 * This class extends the existing istreambuf_holder to preserve information
 * about the underlying cloud storage backend for logging and error reporting.
 */

/*
class CloudIStreamHolder : public bxz::istream {
public:
    CloudIStreamHolder(std::streambuf* sb, fs::CloudInputStreamPtr cloud_stream)
        : bxz::istream(sb)
        , m_streambuf(sb)
        , m_cloud_stream(std::move(cloud_stream))
    {}
    
    ~CloudIStreamHolder() override {
        delete m_streambuf;
    }

    
    
    // Metadata accessors
    fs::StorageBackend storage_backend() const {
        return m_cloud_stream ? m_cloud_stream->backend() : fs::StorageBackend::Unknown;
    }
    
    std::string storage_path() const {
        return m_cloud_stream ? m_cloud_stream->path() : "";
    }
    
    bool is_cloud_storage() const {
        auto be = storage_backend();
        return be == fs::StorageBackend::S3 || 
               be == fs::StorageBackend::GCS;
    }
    
private:
    std::streambuf* m_streambuf;
    fs::CloudInputStreamPtr m_cloud_stream;
};
*/

class CloudIStreamHolder : public istreambuf_holder {
public:
    CloudIStreamHolder(std::streambuf* sb, fs::CloudInputStreamPtr cloud_stream)
        : istreambuf_holder(sb)
        , m_cloud_stream(std::move(cloud_stream))
    {}


    void get_md5(uint8_t digest[16]) const override
    {
        if (!m_cloud_stream) {
            spdlog::warn("No underlying cloud stream for MD5 retrieval");
            std::fill_n(digest, 16, 0);
            return;
        }

        std::string hex_string = m_cloud_stream->md5();
        if (hex_string.size() != 32) {
            spdlog::warn("Invalid MD5 string length: {}", hex_string.size());
            std::fill_n(digest, 16, 0);
            return;
        }

        for (int i = 0; i < 16; ++i) {
            const std::string byte_string = hex_string.substr(i * 2, 2);
            digest[i] = static_cast<uint8_t>(std::stoi(byte_string, nullptr, 16));
        }
    }

    
    // Metadata accessors
    fs::StorageBackend storage_backend() const {
        return m_cloud_stream ? m_cloud_stream->backend() : fs::StorageBackend::Unknown;
    }
    
    std::string storage_path() const {
        return m_cloud_stream ? m_cloud_stream->path() : "";
    }
    
    bool is_cloud_storage() const {
        auto be = storage_backend();
        return be == fs::StorageBackend::S3 || 
               be == fs::StorageBackend::GCS;
    }
    
private:
    fs::CloudInputStreamPtr m_cloud_stream;
};


/**
 * @brief Open a FASTQ file from local or cloud storage
 * 
 * This function is a drop-in replacement for the existing s_OpenStream() function.
 * It automatically detects the storage backend from the URI/path and creates
 * the appropriate stream, including decompression for .gz/.bz2 files.
 * 
 * Supported formats:
 * - Local files: /path/to/file.fastq, ./relative/path.fastq.gz
 * - S3: s3://bucket-name/path/to/file.fastq.gz
 * - GCS: gs://bucket-name/path/to/file.fastq.gz
 * - HTTP/HTTPS: https://example.com/file.fastq
 * - Stdin: "-"
 * 
 * @param filename File path or URI to open
 * @param buffer_size Read buffer size (default 4KB, recommend 1MB for cloud)
 * @return shared_ptr<istream> Input stream ready for reading
 * @throws runtime_error on failure to open file
 * @throws sharq::fs::CloudStorageError on cloud-specific errors
 * 
 * @example
 * // Read from S3
 * auto stream = sharq::open_fastq_stream("s3://my-bucket/sample.fastq.gz");
 * 
 * // Read from GCS with custom buffer
 * auto stream = sharq::open_fastq_stream("gs://my-bucket/sample.fastq.gz", 1024*1024);
 */
inline std::shared_ptr<std::istream> open_fastq_stream(
    const std::string& filename,
    size_t buffer_size = 4096)
{
    custom_istream::custom_istream * c_istream = nullptr;
    // Handle stdin
    if (filename == "-") {

        const vdb::KStream * kin = nullptr;
        if ( KStreamMakeStdIn ( &kin ) != 0 )
        {
            throw std::runtime_error("Failure to open stdin");
        }

        const vdb::KStreamMD5ReadObserver * observer = nullptr;
        if ( KStreamMakeMD5ReadObserver ( kin, & observer ) != 0 )
        {
            throw std::runtime_error("Failure to create observer for stdin");
        }

        c_istream = new custom_istream::custom_istream( custom_istream::custom_istream::make_from_kstream( kin, buffer_size ) );
        return std::shared_ptr<std::istream>( new istreambuf_holder( c_istream, observer ));
    }
    
    // Detect storage backend
    fs::StorageBackend backend = fs::detect_backend(filename);
    
    // For local files and HTTP, use existing VDB infrastructure
    // which is already optimized and battle-tested
    if (backend == fs::StorageBackend::Local) {
        // Use existing VDB KFile infrastructure
        const vdb::KFile * kfile = vdb::KFileFactory::make_from_path( filename );
        if ( kfile == nullptr )
        {
            throw std::runtime_error("Failure to open file '" + filename + "'");
        }

        const vdb::KFileMD5ReadObserver * observer = nullptr;
        if ( KFileMakeMD5ReadObserver ( kfile, & observer ) != 0 )
        {
            throw std::runtime_error("Failure to create observer for '" + filename + "'");
        }

        c_istream = new custom_istream::custom_istream( custom_istream::custom_istream::make_from_kfile( kfile, buffer_size ) );
        return std::shared_ptr<std::istream>( new istreambuf_holder( c_istream, observer ));
    }
    
    if (backend == fs::StorageBackend::HTTP) {

        const vdb::KStream * kstream = vdb::KStreamFactory::make_from_uri( filename );
        if ( kstream == nullptr )
        {
            throw std::runtime_error("Failure to open URL '" + filename + "'");
        }

        const vdb::KStreamMD5ReadObserver * observer = nullptr;
        if ( KStreamMakeMD5ReadObserver ( kstream, & observer ) != 0 )
        {
            throw std::runtime_error("Failure to create observer for '" + filename + "'");
        }

        c_istream = new custom_istream::custom_istream( custom_istream::custom_istream::make_from_kstream( kstream, buffer_size ));
        return std::shared_ptr<std::istream>( new istreambuf_holder( c_istream, observer ));
    }
    
    // For S3/GCS, use cloud filesystem
    if (backend == fs::StorageBackend::S3 || backend == fs::StorageBackend::GCS) {
        try {
            auto& factory = CloudFileManager::instance().factory();
            auto cloud_stream = factory.open(filename);
            
            if (!cloud_stream || cloud_stream->has_error()) {
                std::string error_msg = cloud_stream ? 
                    cloud_stream->last_error() : "Unknown error";
                throw std::runtime_error(
                    "Failure to open " + fs::backend_name(backend) + 
                    " file '" + filename + "': " + error_msg
                );
            }
            
            // Create adapter for bxzstr compatibility
            auto src = std::make_shared<CloudStreamSource>(cloud_stream);
            auto* c_istream = new custom_istream::custom_istream(src, buffer_size);
            
            // Wrap with CloudIStreamHolder for metadata preservation
            return std::shared_ptr<std::istream>(
                new CloudIStreamHolder(c_istream, cloud_stream)
            );
            
        } catch (const fs::FileNotFoundError& e) {
            throw std::runtime_error("File not found: " + filename);
        } catch (const fs::AccessDeniedError& e) {
            throw std::runtime_error("Access denied: " + filename + 
                ". Check your credentials.");
        } catch (const fs::CredentialsError& e) {
            throw std::runtime_error("Credentials error for " + 
                fs::backend_name(backend) + ": " + e.what());
        } catch (const fs::CloudStorageError& e) {
            throw std::runtime_error(e.what());
        }
    }
    
    throw std::runtime_error(
        "Unsupported storage backend for file '" + filename + "'"
    );
}

/**
 * @brief Open a FASTQ file with explicit cloud configuration
 * 
 * Use this overload when you need to specify custom credentials or
 * configuration options for cloud storage.
 * 
 * @param filename File path or URI to open
 * @param config Cloud configuration options
 * @param buffer_size Read buffer size
 * @return shared_ptr<istream> Input stream ready for reading
 */
inline std::shared_ptr<std::istream> open_fastq_stream(
    const std::string& filename,
    const fs::CloudConfig& config,
    size_t buffer_size = 4096)
{
    // Update the global factory configuration
    CloudFileManager::instance().configure(config);
    return open_fastq_stream(filename, buffer_size);
}

/**
 * @brief Check if a filename/URI represents cloud storage
 */
inline bool is_cloud_path(const std::string& path) {
    auto backend = fs::detect_backend(path);
    return backend == fs::StorageBackend::S3 || 
           backend == fs::StorageBackend::GCS;
}

/**
 * @brief Get the storage backend type for a path
 */
inline fs::StorageBackend get_storage_backend(const std::string& path) {
    return fs::detect_backend(path);
}

} // namespace sharq

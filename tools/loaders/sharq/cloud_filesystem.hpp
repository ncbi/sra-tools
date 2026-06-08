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
 * @file cloud_filesystem.hpp
 * @brief Unified filesystem abstraction for local, GCS, and S3 storage backends
 * 
 * This module provides a consistent interface for reading files from:
 * - Local filesystem
 * - Google Cloud Storage (gs://)
 * - Amazon S3 (s3://)
 * 
 * Implementation uses native AWS/GCP SDKs directly for lighter dependencies.
 * Enable with -DSHARQ_USE_NATIVE_CLOUD=ON
 */

#include <memory>
#include <string>
#include <string_view>
#include <stdexcept>
#include <regex>
#include <functional>
#include <cstdlib>
#include <fstream>
// Include the existing kfile_stream infrastructure
#include <kfile_stream/kfile_stream.hpp>
#include <spdlog/spdlog.h>

namespace sharq {
namespace fs {

//==============================================================================
// Storage Backend Types
//==============================================================================

enum class StorageBackend {
    Local,      // Local filesystem
    S3,         // Amazon S3 (s3://)
    GCS,        // Google Cloud Storage (gs://)
    HTTP,       // HTTP/HTTPS URLs
    Unknown
};

/**
 * @brief Detect storage backend from URI/path
 */
inline StorageBackend detect_backend(const std::string& path) {
    if (path.empty()) {
        return StorageBackend::Unknown;
    }
    
    // Check for S3
    if (path.rfind("s3://", 0) == 0 || path.rfind("S3://", 0) == 0) {
        return StorageBackend::S3;
    }
    
    // Check for GCS
    if (path.rfind("gs://", 0) == 0 || path.rfind("GS://", 0) == 0) {
        return StorageBackend::GCS;
    }
    
    // Check for HTTP/HTTPS
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) {
        return StorageBackend::HTTP;
    }
    
    // Default to local filesystem
    return StorageBackend::Local;
}

/**
 * @brief Get human-readable name for storage backend
 */
inline std::string backend_name(StorageBackend backend) {
    switch (backend) {
        case StorageBackend::Local: return "Local";
        case StorageBackend::S3:    return "S3";
        case StorageBackend::GCS:   return "GCS";
        case StorageBackend::HTTP:  return "HTTP";
        default:                    return "Unknown";
    }
}

//==============================================================================
// Cloud Credentials Configuration
//==============================================================================

/**
 * @brief S3 connection configuration
 */
struct S3Config {
    std::string access_key_id;          // AWS_ACCESS_KEY_ID
    std::string secret_access_key;      // AWS_SECRET_ACCESS_KEY
    std::string session_token;          // AWS_SESSION_TOKEN (optional)
    std::string region;                 // AWS_REGION (default: us-east-1)
    std::string endpoint_override;      // Custom endpoint (for MinIO, etc.)
    bool use_virtual_addressing = true; // Virtual-hosted style URLs
    size_t connect_timeout_ms = 30000;  // Connection timeout
    size_t request_timeout_ms = 300000; // Request timeout
    
    // Use default credentials chain (environment, config file, IAM role)
    bool use_default_credentials = true;
    
    S3Config() = default;
    
    // Factory for default configuration (uses environment/IAM)
    static S3Config from_environment() {
        S3Config config;
        config.use_default_credentials = true;
        
        // Check for explicit credentials in environment
        if (const char* key = std::getenv("AWS_ACCESS_KEY_ID")) {
            config.access_key_id = key;
            config.use_default_credentials = false;
        }
        if (const char* secret = std::getenv("AWS_SECRET_ACCESS_KEY")) {
            config.secret_access_key = secret;
        }
        if (const char* token = std::getenv("AWS_SESSION_TOKEN")) {
            config.session_token = token;
        }
        if (const char* region = std::getenv("AWS_REGION")) {
            config.region = region;
        } else if (const char* region = std::getenv("AWS_DEFAULT_REGION")) {
            config.region = region;
        } else {
            config.region = "us-east-1";
        }
        
        return config;
    }
};

/**
 * @brief GCS connection configuration
 */
struct GCSConfig {
    std::string credentials_file;        // Path to service account JSON
    std::string project_id;              // GCP project ID
    std::string endpoint_override;       // Custom endpoint (for testing)
    size_t connect_timeout_ms = 30000;   // Connection timeout
    size_t request_timeout_ms = 300000;  // Request timeout
    
    // Use Application Default Credentials
    bool use_default_credentials = true;
    
    GCSConfig() = default;
    
    // Factory for default configuration (uses ADC)
    static GCSConfig from_environment() {
        GCSConfig config;
        config.use_default_credentials = true;
        
        // Check for explicit credentials file
        if (const char* creds = std::getenv("GOOGLE_APPLICATION_CREDENTIALS")) {
            config.credentials_file = creds;
        }
        if (const char* project = std::getenv("GOOGLE_CLOUD_PROJECT")) {
            config.project_id = project;
        } else if (const char* project = std::getenv("GCLOUD_PROJECT")) {
            config.project_id = project;
        }
        
        return config;
    }
};

/**
 * @brief Combined configuration for all cloud backends
 */
struct CloudConfig {
    S3Config s3;
    GCSConfig gcs;
    size_t read_buffer_size = 1024 * 1024 * 250;  // 250MB default buffer
    int max_retries = 3;
    bool enable_caching = false;
    std::string cache_directory;
    
    CloudConfig() = default;
    
    static CloudConfig from_environment() {
        CloudConfig config;
        config.s3 = S3Config::from_environment();
        config.gcs = GCSConfig::from_environment();
        return config;
    }
};

//==============================================================================
// URI Parsing Utilities
//==============================================================================

/**
 * @brief Parsed cloud URI components
 */
struct CloudUri {
    StorageBackend backend;
    std::string bucket;
    std::string key;         // Object key/path within bucket
    std::string region;      // Optional region hint
    std::string original;    // Original URI string
    
    bool is_valid() const {
        return backend != StorageBackend::Unknown && !bucket.empty();
    }
};

/**
 * @brief Parse S3 or GCS URI into components
 * 
 * Supports:
 * - s3://bucket-name/path/to/object
 * - gs://bucket-name/path/to/object
 */
inline CloudUri parse_cloud_uri(const std::string& uri) {
    CloudUri result;
    result.original = uri;
    result.backend = detect_backend(uri);
    
    if (result.backend != StorageBackend::S3 && 
        result.backend != StorageBackend::GCS) {
        return result;
    }
    
    // Find the scheme separator
    size_t scheme_end = uri.find("://");
    if (scheme_end == std::string::npos) {
        result.backend = StorageBackend::Unknown;
        return result;
    }
    
    // Extract bucket and key
    size_t path_start = scheme_end + 3;
    size_t bucket_end = uri.find('/', path_start);
    
    if (bucket_end == std::string::npos) {
        // Just bucket, no key
        result.bucket = uri.substr(path_start);
        result.key = "";
    } else {
        result.bucket = uri.substr(path_start, bucket_end - path_start);
        result.key = uri.substr(bucket_end + 1);
    }
    
    return result;
}

//==============================================================================
// Error Types
//==============================================================================

class CloudStorageError : public std::runtime_error {
public:
    StorageBackend backend;
    std::string operation;
    std::string path;
    int error_code;
    
    CloudStorageError(StorageBackend be, const std::string& op, 
                      const std::string& p, const std::string& msg, int code = 0)
        : std::runtime_error(format_message(be, op, p, msg, code))
        , backend(be)
        , operation(op)
        , path(p)
        , error_code(code)
    {}
    
private:
    static std::string format_message(StorageBackend be, const std::string& op,
                                      const std::string& p, const std::string& msg, int code) {
        std::string result = backend_name(be) + " error during " + op;
        if (!p.empty()) {
            result += " [" + p + "]";
        }
        result += ": " + msg;
        if (code != 0) {
            result += " (code: " + std::to_string(code) + ")";
        }
        return result;
    }
};

class FileNotFoundError : public CloudStorageError {
public:
    FileNotFoundError(StorageBackend be, const std::string& path)
        : CloudStorageError(be, "open", path, "File not found", 404)
    {}
};

class AccessDeniedError : public CloudStorageError {
public:
    AccessDeniedError(StorageBackend be, const std::string& path)
        : CloudStorageError(be, "open", path, "Access denied", 403)
    {}
};

class CredentialsError : public CloudStorageError {
public:
    CredentialsError(StorageBackend be, const std::string& msg)
        : CloudStorageError(be, "authenticate", "", msg, 401)
    {}
};

//==============================================================================
// Abstract Input Stream Interface
//==============================================================================

/**
 * @brief Abstract interface for cloud-aware input streams
 * 
 * Extends custom_istream::src_interface to add metadata and error handling
 */
class CloudInputStream : public custom_istream::src_interface {
public:
    virtual ~CloudInputStream() = default;
    
    // Required: read data into buffer
    virtual size_t read(char* dst, size_t requested) override = 0;
    
    // Optional metadata
    virtual size_t size() const { return 0; }  // 0 means unknown
    virtual bool supports_seek() const { return false; }
    virtual bool seek(size_t position) { return false; }
    
    // Error handling
    virtual bool is_eof() const = 0;
    virtual bool has_error() const = 0;
    virtual std::string last_error() const { return ""; }
    
    // Storage info
    virtual StorageBackend backend() const = 0;
    virtual std::string path() const = 0;
    virtual std::string md5() const = 0;
};

using CloudInputStreamPtr = std::shared_ptr<CloudInputStream>;

//==============================================================================
// Local File Implementation
//==============================================================================

/**
 * @brief Local filesystem input stream using existing kfile infrastructure
 */


} // namespace fs
} // namespace sharq
//==============================================================================
// Native Cloud SDK Implementation
//==============================================================================

#ifdef SHARQ_USE_NATIVE_CLOUD

// AWS SDK includes
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/GetBucketLocationRequest.h>
#include <aws/s3/model/BucketLocationConstraint.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>

// GCP SDK includes
#include <google/cloud/storage/client.h>

namespace sharq {
namespace fs {
namespace native_impl {

/**
 * @brief AWS SDK global initialization RAII wrapper
 */
class AwsInitializer {
public:
    static AwsInitializer& instance() {
        static AwsInitializer inst;
        return inst;
    }
    
private:
    AwsInitializer() {
        Aws::InitAPI(m_options);
    }
    
    ~AwsInitializer() {
        Aws::ShutdownAPI(m_options);
    }
    
    Aws::SDKOptions m_options;
};

/**
 * @brief S3 input stream using AWS SDK directly
 */
class S3InputStream : public CloudInputStream {
public:
    S3InputStream(const CloudUri& uri, const S3Config& config)
        : m_uri(uri)
        , m_path(uri.original)
    {
        // Ensure AWS SDK is initialized
        AwsInitializer::instance();
        
        // Configure client
        Aws::Client::ClientConfiguration clientConfig;
        if (!config.region.empty()) {
            clientConfig.region = config.region;
        }
        if (!config.endpoint_override.empty()) {
            clientConfig.endpointOverride = config.endpoint_override;
        }
        clientConfig.connectTimeoutMs = static_cast<long>(config.connect_timeout_ms);
        clientConfig.requestTimeoutMs = static_cast<long>(config.request_timeout_ms);
        
        // Create credentials provider
        std::shared_ptr<Aws::Auth::AWSCredentialsProvider> credProvider;
        if (!config.use_default_credentials && 
            !config.access_key_id.empty()) {
            credProvider = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(
                config.access_key_id.c_str(),
                config.secret_access_key.c_str(),
                config.session_token.c_str()
            );
        } else {
            //credProvider = std::make_shared<Aws::Auth::DefaultAWSCredentialsProviderChain>();
            credProvider = std::static_pointer_cast<Aws::Auth::AWSCredentialsProvider>(
                std::make_shared<Aws::Auth::DefaultAWSCredentialsProviderChain>()
            );        
        }
        
        // Get the actual bucket region to handle PermanentRedirect errors
        Aws::S3::Model::GetBucketLocationRequest locRequest;
        locRequest.SetBucket(m_uri.bucket.c_str());
        auto tempClient = std::make_unique<Aws::S3::S3Client>(credProvider, clientConfig, 
            Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
            true,
            Aws::S3::US_EAST_1_REGIONAL_ENDPOINT_OPTION::NOT_SET);
        auto locOutcome = tempClient->GetBucketLocation(locRequest);
        if (locOutcome.IsSuccess()) {
            Aws::S3::Model::BucketLocationConstraint constraint = locOutcome.GetResult().GetLocationConstraint();
            Aws::String actualRegion = Aws::S3::Model::BucketLocationConstraintMapper::GetNameForBucketLocationConstraint(constraint);
            if (!actualRegion.empty() && actualRegion != clientConfig.region) {
                spdlog::info("Bucket '{}' is in region '{}', updating client region from '{}'", 
                             m_uri.bucket, actualRegion, clientConfig.region);
                clientConfig.region = actualRegion;
            }
        } else {
            spdlog::warn("Failed to get bucket location for '{}': {}", 
                         m_uri.bucket, locOutcome.GetError().GetMessage());
        }
        
        m_client = std::make_unique<Aws::S3::S3Client>(credProvider, clientConfig, 
            Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,  // or RequestDependent
            true,   // useVirtualAddressing
            Aws::S3::US_EAST_1_REGIONAL_ENDPOINT_OPTION::NOT_SET);
//            std::make_shared<Aws::Client::ClientConfiguration>(clientConfig));
        
        // Open the object stream
        open_stream();
    }
    
    size_t read(char* dst, size_t requested) override {
        if (m_error || !m_response_stream) {
            return static_cast<size_t>(-1);
        }
        
        m_response_stream->read(dst, requested);
        size_t bytes_read = static_cast<size_t>(m_response_stream->gcount());
        m_bytes_read += bytes_read;
        if (m_response_stream->eof()) {
            m_eof = true;
            spdlog::info("AWS EOF reached after reading {} bytes, expected {}", m_bytes_read, m_size);
            if (m_size > 0 && m_bytes_read != m_size) 
                throw CloudStorageError(StorageBackend::S3, "read", m_path,
                                        "Unexpected EOF: expected " + 
                                        std::to_string(m_size) + 
                                        " bytes, but read " + 
                                        std::to_string(m_bytes_read) + " bytes");

        }
        
        if (m_response_stream->fail() && !m_response_stream->eof()) {
            m_error = true;
            m_error_msg = "S3 stream read error";
            return static_cast<size_t>(-1);
        }
        
        return bytes_read;
    }
    
    bool is_eof() const override { return m_eof; }
    bool has_error() const override { return m_error; }
    std::string last_error() const override { return m_error_msg; }
    
    StorageBackend backend() const override { return StorageBackend::S3; }
    std::string path() const override { return m_path; }
    std::string md5() const override { return m_md5; }
    
    size_t size() const override { return m_size; }
    
private:
    void open_stream() {
        Aws::S3::Model::GetObjectRequest request;
        request.SetBucket(m_uri.bucket.c_str());
        request.SetKey(m_uri.key.c_str());
        
        auto outcome = m_client->GetObject(request);
        
        if (!outcome.IsSuccess()) {
            m_error = true;
            const auto& error = outcome.GetError();
            m_error_msg = error.GetMessage();
            
            auto errorType = error.GetErrorType();
            if (errorType == Aws::S3::S3Errors::NO_SUCH_KEY ||
                errorType == Aws::S3::S3Errors::NO_SUCH_BUCKET) {
                throw FileNotFoundError(StorageBackend::S3, m_path);
            } else if (errorType == Aws::S3::S3Errors::ACCESS_DENIED) {
                throw AccessDeniedError(StorageBackend::S3, m_path);
            }
            return;
        }
        
        m_get_result = std::make_unique<Aws::S3::Model::GetObjectResult>(
            std::move(outcome.GetResult())
        );
        m_response_stream = &m_get_result->GetBody();
        m_size = static_cast<size_t>(m_get_result->GetContentLength());
        m_md5 = m_get_result->GetETag();
        // Remove surrounding quotes from ETag if present
        if (m_md5.size() >= 2 && m_md5.front() == '"' && m_md5.back() == '"') {
            m_md5 = m_md5.substr(1, m_md5.size() - 2);
        }
    }
    
    CloudUri m_uri;
    std::string m_path;
    std::unique_ptr<Aws::S3::S3Client> m_client;
    std::unique_ptr<Aws::S3::Model::GetObjectResult> m_get_result;
    Aws::IOStream* m_response_stream = nullptr;
    size_t m_size = 0;
    size_t m_bytes_read = 0;
    bool m_eof = false;
    bool m_error = false;
    std::string m_error_msg;
    std::string m_md5;
};


std::string base64ToHex(const std::string& base64_string) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::vector<int> decode_table(256, -1);
    for (int i = 0; i < 64; i++) {
        decode_table[base64_chars[i]] = i;
    }
    
    std::vector<uint8_t> decoded;
    int val = 0, valb = -8;
    for (unsigned char c : base64_string) {
        if (decode_table[c] == -1) break;
        val = (val << 6) + decode_table[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    
    // Convert bytes to hex string
    std::stringstream hex_stream;
    for (uint8_t byte : decoded) {
        hex_stream << std::hex << std::setw(2) << std::setfill('0') 
                   << static_cast<int>(byte);
    }
    
    return hex_stream.str();
}


/**
 * @brief GCS input stream using Google Cloud SDK directly
 */
class GcsInputStream : public CloudInputStream {
public:
    GcsInputStream(const CloudUri& uri, const GCSConfig& config)
        : m_uri(uri)
        , m_path(uri.original)
        , m_client(get_or_create_client(config))
    {
        if (!m_client) {
            m_error = true;
            m_error_msg = "Failed to initialize GCS client";
            return;
        }
        
        try {
            // Open the object stream
            open_stream();
        } catch (const std::exception& ex) {
            m_error = true;
            m_error_msg = std::string("GCS stream initialization failed: ") + ex.what();
        }
    }

    size_t read(char* dst, size_t requested) override {
        if (m_error || !m_reader) {
            return static_cast<size_t>(-1);
        }
        m_reader.read(dst, requested);
        size_t bytes_read = static_cast<size_t>(m_reader.gcount());
        m_bytes_read += bytes_read;
        if (m_reader.eof()) {
            m_eof = true;
            spdlog::info("GCS EOF reached after reading {} bytes, expected {}", m_bytes_read, m_object_size);
            if (m_object_size > 0 && m_bytes_read != m_object_size) 
                throw CloudStorageError(StorageBackend::GCS, "read", m_path,
                                        "Unexpected EOF: expected " + 
                                        std::to_string(m_object_size) + 
                                        " bytes, but read " + 
                                        std::to_string(m_bytes_read) + " bytes");
        }
        
        if (m_reader.bad()) {
            m_error = true;
            m_error_msg = m_reader.status().message();
            return static_cast<size_t>(-1);
        }
        
        return bytes_read;
    }


private:
    static std::shared_ptr<google::cloud::storage::Client> get_or_create_client(const GCSConfig& config) {
        static std::mutex mutex;
        static std::shared_ptr<google::cloud::storage::Client> client;
        static std::string current_credentials_file;
        
        std::lock_guard<std::mutex> lock(mutex);
        
        // Check if we need to create a new client (first time or config changed)
        if (!client || current_credentials_file != config.credentials_file) {
            client = create_client(config);
            current_credentials_file = config.credentials_file;
        }
        
        return client;
    }
    
    static std::shared_ptr<google::cloud::storage::Client> create_client(const GCSConfig& config) {
        namespace gcs = google::cloud::storage;
        
        try {
            google::cloud::Options options;
            
            if (!config.credentials_file.empty()) {
                // Read the credentials file to detect type
                std::ifstream cred_file(config.credentials_file);
                if (!cred_file.is_open()) {
                    spdlog::error("Failed to open credentials file: {}", config.credentials_file);
                    return nullptr;
                }
                
                std::string cred_content((std::istreambuf_iterator<char>(cred_file)),
                                        std::istreambuf_iterator<char>());
                cred_file.close();
                
                // Detect credential type and set appropriate options
                if (cred_content.find("\"client_email\"") != std::string::npos) {
                    // Service account credentials
                    auto creds = gcs::oauth2::CreateServiceAccountCredentialsFromJsonFilePath(
                        config.credentials_file
                    );
                    if (!creds) {
                        spdlog::error("Failed to load GCS service account credentials: {}", 
                                     creds.status().message());
                        return nullptr;
                    }
                    options.set<gcs::Oauth2CredentialsOption>(*creds);
                    spdlog::info("Using GCS service account credentials from file: {}", 
                                config.credentials_file);
                } else if (cred_content.find("\"type\"") != std::string::npos && 
                           cred_content.find("\"authorized_user\"") != std::string::npos) {
                    // User credentials (from gcloud auth application-default login)
                    auto creds = gcs::oauth2::CreateAuthorizedUserCredentialsFromJsonFilePath(
                        config.credentials_file
                    );
                    if (!creds) {
                        spdlog::error("Failed to load GCS user credentials: {}", 
                                     creds.status().message());
                        return nullptr;
                    }
                    options.set<gcs::Oauth2CredentialsOption>(*creds);
                    spdlog::info("Created GCS client with user credentials from file: {}", 
                                config.credentials_file);
                } else {
                    // Unknown credential type - try default credentials as fallback
                    auto creds = google::cloud::MakeGoogleDefaultCredentials();
                    options.set<google::cloud::UnifiedCredentialsOption>(creds);
                    spdlog::info("Created GCS client with default credentials as fallback.");
                }
            } else {
                // Use Application Default Credentials
                auto creds = google::cloud::MakeGoogleDefaultCredentials();
                options.set<google::cloud::UnifiedCredentialsOption>(creds);
                spdlog::info("Created GCS client with Application Default Credentials.");
            }
            
            // Create client once with the configured options
            return std::make_shared<gcs::Client>(options);
            
        } catch (const std::exception& ex) {
            spdlog::error("GCS client creation failed: {}", ex.what());
            return nullptr;
        }
    }

    
    bool is_eof() const override { return m_eof; }
    bool has_error() const override { return m_error; }
    std::string last_error() const override { return m_error_msg; }
    
    StorageBackend backend() const override { return StorageBackend::GCS; }
    std::string path() const override { return m_path; }
    std::string md5() const override { return m_md5_hash; }
    
    void open_stream() {
        namespace gcs = google::cloud::storage;
        auto metadata = m_client->GetObjectMetadata(m_uri.bucket, m_uri.key);
        m_object_size = metadata->size();
        m_md5_hash = base64ToHex(metadata->md5_hash());

        m_reader = m_client->ReadObject(m_uri.bucket, m_uri.key);
        
        if (!m_reader) {
            m_error = true;
            m_error_msg = m_reader.status().message();
            
            if (m_reader.status().code() == google::cloud::StatusCode::kNotFound) {
                throw FileNotFoundError(StorageBackend::GCS, m_path);
            } else if (m_reader.status().code() == google::cloud::StatusCode::kPermissionDenied) {
                throw AccessDeniedError(StorageBackend::GCS, m_path);
            }
        }
    }
    
    CloudUri m_uri;
    std::string m_path;
    std::shared_ptr<google::cloud::storage::Client> m_client;
    google::cloud::storage::ObjectReadStream m_reader;
    bool m_eof = false;
    bool m_error = false;
    std::string m_error_msg;
    size_t m_bytes_read = 0;
    size_t m_object_size = 0;
    std::string m_md5_hash;
};

} // namespace native_impl
} // namespace fs
} // namespace sharq

#endif // SHARQ_USE_NATIVE_CLOUD

//==============================================================================
// Unified File Factory
//==============================================================================

namespace sharq {
namespace fs {

/**
 * @brief Factory for creating input streams from any supported backend
 * 
 * This is the main entry point for opening files from local or cloud storage.
 * It automatically detects the storage backend from the URI/path and creates
 * the appropriate stream implementation.
 */
class CloudFileFactory {
public:
    explicit CloudFileFactory(const CloudConfig& config = CloudConfig::from_environment())
        : m_config(config)
    {}
    
    ~CloudFileFactory() = default;
    
    /**
     * @brief Open a file for reading
     * 
     * @param path File path or URI (local path, s3://, gs://, http://, https://)
     * @return CloudInputStreamPtr Input stream for reading
     * @throws CloudStorageError on failure
     */
    CloudInputStreamPtr open(const std::string& path) {
        StorageBackend backend = detect_backend(path);
        
        switch (backend) {
            //case StorageBackend::Local:
            //    return open_local(path);
                
            case StorageBackend::S3:
                return open_s3(path);
                
            case StorageBackend::GCS:
                return open_gcs(path);
                
            //case StorageBackend::HTTP:
            //    return open_http(path);
                
            default:
                throw CloudStorageError(StorageBackend::Unknown, "open", path,
                                       "Unsupported storage backend");
        }
    }
    
    /**
     * @brief Open a file and wrap it in std::istream for compatibility
     * 
     * This provides compatibility with existing code that expects std::istream.
     */
    std::shared_ptr<std::istream> open_as_istream(const std::string& path) {
        auto stream = open(path);
        
        // Create a custom_istream wrapper
        auto src = std::dynamic_pointer_cast<custom_istream::src_interface>(stream);
        auto* custom = new custom_istream::custom_istream(src, m_config.read_buffer_size);
        return std::shared_ptr<std::istream>(custom);
    }
    
    /**
     * @brief Check if a file exists
     */
    bool exists(const std::string& path) {
        try {
            auto stream = open(path);
            return stream && !stream->has_error();
        } catch (const FileNotFoundError&) {
            return false;
        } catch (const CloudStorageError&) {
            return false;
        }
    }
    
    /**
     * @brief Get file size if available
     * @return File size in bytes, or 0 if unknown
     */
    size_t file_size(const std::string& path) {
        auto stream = open(path);
        return stream ? stream->size() : 0;
    }
    
private:
    //CloudInputStreamPtr open_local(const std::string& path) {
    //    return std::make_shared<LocalFileInputStream>(path);
    //}
    
    //CloudInputStreamPtr open_http(const std::string& url) {
    //    return std::make_shared<HttpInputStream>(url);
    //}
    
    CloudInputStreamPtr open_s3(const std::string& uri) {
        CloudUri parsed = parse_cloud_uri(uri);
        if (!parsed.is_valid()) {
            throw CloudStorageError(StorageBackend::S3, "parse", uri,
                                   "Invalid S3 URI format");
        }
        
#ifdef SHARQ_USE_NATIVE_CLOUD
        // Use native AWS SDK
        return std::make_shared<native_impl::S3InputStream>(parsed, m_config.s3);
#else
        // Cloud support not compiled
        throw CloudStorageError(StorageBackend::S3, "open", uri,
            "S3 support not compiled. Enable with -DSHARQ_USE_NATIVE_CLOUD=ON");
#endif
    }
    
    CloudInputStreamPtr open_gcs(const std::string& uri) {
        CloudUri parsed = parse_cloud_uri(uri);
        if (!parsed.is_valid()) {
            throw CloudStorageError(StorageBackend::GCS, "parse", uri,
                                   "Invalid GCS URI format");
        }
        
#ifdef SHARQ_USE_NATIVE_CLOUD
        // Use native GCP SDK
        return std::make_shared<native_impl::GcsInputStream>(parsed, m_config.gcs);
#else
        // Cloud support not compiled
        throw CloudStorageError(StorageBackend::GCS, "open", uri,
            "GCS support not compiled. Enable with -DSHARQ_USE_NATIVE_CLOUD=ON");
#endif
    }
    
    CloudConfig m_config;
};

} // namespace fs
} // namespace sharq

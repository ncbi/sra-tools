#pragma once

#include <cstdint>
#include <memory>
#include <bits/stdc++.h>

using namespace std;

namespace sra_convert {

/*
 * from ncbi-vdb/interfaces/insdc/sra.h
    SRA_PLATFORM_UNDEFINED         = 0,
    SRA_PLATFORM_454               = 1,
    SRA_PLATFORM_ILLUMINA          = 2,
    SRA_PLATFORM_ABSOLID           = 3,
    SRA_PLATFORM_COMPLETE_GENOMICS = 4,
    SRA_PLATFORM_HELICOS           = 5,
    SRA_PLATFORM_PACBIO_SMRT       = 6,
    SRA_PLATFORM_ION_TORRENT       = 7,
    SRA_PLATFORM_CAPILLARY         = 8,
    SRA_PLATFORM_OXFORD_NANOPORE   = 9,
    SRA_PLATFORM_ELEMENT_BIO       = 10,
    SRA_PLATFORM_TAPESTRI          = 11,
    SRA_PLATFORM_VELA_DIAG         = 12,
    SRA_PLATFORM_GENAPSYS          = 13,
    SRA_PLATFORM_ULTIMA            = 14,
    SRA_PLATFORM_GENEMIND          = 15,
    SRA_PLATFORM_BGISEQ            = 16,
    SRA_PLATFORM_DNBSEQ            = 17,
    SRA_PLATFORM_SINGULAR_GENOMICS = 18,
    SRA_PLATFORM_GENEUS_TECH       = 19,
    SRA_PLATFORM_SALUS             = 20

*/

class Platform;
typedef std::shared_ptr< Platform > Platform_ptr;
class Platform {
    private:
        inline static const string S_UNDEFINED = "UNDEFINED";
        inline static const string S_454 = "454";
        inline static const string S_ILLUMINA = "ILLUMINA";
        inline static const string S_ABSOLID = "ABSOLID";
        inline static const string S_COMPLETE_GENOMICS = "COMPLETE_GENOMICS";
        inline static const string S_HELICOS = "HELICOS";
        inline static const string S_PACBIO_SMRT = "PACBIO_SMRT";
        inline static const string S_ION_TORRENT = "ION_TORRENT";
        inline static const string S_CAPILLARY = "CAPILLARY";
        inline static const string S_OXFORD_NANOPORE = "OXFORD_NANOPORE";
        inline static const string S_ELEMENT_BIO = "ELEMENT_BIO";
        inline static const string S_TAPESTRI = "TAPESTRI";
        inline static const string S_VELA_DIAG = "VELA_DIAG";
        inline static const string S_GENAPSYS = "GENAPSYS";
        inline static const string S_ULTIMA = "ULTIMA";
        inline static const string S_GENEMIND = "GENEMIND";
        inline static const string S_BGISEQ = "BGISEQ";
        inline static const string S_DNBSEQ = "DNBSEQ";
        inline static const string S_SINGULAR_GENOMICS = "SINGULAR_GENOMICS";
        inline static const string S_GENEUS_TECH = "GENEUS_TECH";
        inline static const string S_SALUS = "SALUS";

        enum class Platform_E : uint8_t {
            UNDEFINED         = 0,
            PF454             = 1,
            ILLUMINA          = 2,
            ABSOLID           = 3,
            COMPLETE_GENOMICS = 4,
            HELICOS           = 5,
            PACBIO_SMRT       = 6,
            ION_TORRENT       = 7,
            CAPILLARY         = 8,
            OXFORD_NANOPORE   = 9,
            ELEMENT_BIO       = 10,
            TAPESTRI          = 11,
            VELA_DIAG         = 12,
            GENAPSYS          = 13,
            ULTIMA            = 14,
            GENEMIND          = 15,
            BGISEQ            = 16,
            DNBSEQ            = 17,
            SINGULAR_GENOMICS = 18,
            GENEUS_TECH       = 19,
            SALUS             = 20,
            INVALID };
        Platform_E f_type;

        static Platform_E from_string( const std::string& ss ) {
            std::string s = ss;
            std::transform( s . begin(), s . end(), s . begin(), ::toupper );
            if ( S_UNDEFINED == s ) { return Platform_E::UNDEFINED; }
            else if ( S_454 == s ) { return Platform_E::PF454; }
            else if ( S_ILLUMINA == s ) { return Platform_E::ILLUMINA; }
            else if ( S_ABSOLID == s ) { return Platform_E::ABSOLID; }
            else if ( S_COMPLETE_GENOMICS == s ) { return Platform_E::COMPLETE_GENOMICS; }
            else if ( "CG" == s ) { return Platform_E::COMPLETE_GENOMICS; }
            else if ( S_HELICOS == s ) { return Platform_E::HELICOS; }
            else if ( S_PACBIO_SMRT == s ) { return Platform_E::PACBIO_SMRT; }
            else if ( S_ION_TORRENT == s ) { return Platform_E::ION_TORRENT; }
            else if ( S_CAPILLARY == s ) { return Platform_E::CAPILLARY; }
            else if ( S_OXFORD_NANOPORE == s ) { return Platform_E::OXFORD_NANOPORE; }
            else if ( S_ELEMENT_BIO == s ) { return Platform_E::ELEMENT_BIO; }
            else if ( S_TAPESTRI == s ) { return Platform_E::TAPESTRI; }
            else if ( S_VELA_DIAG == s ) { return Platform_E::VELA_DIAG; }
            else if ( S_GENAPSYS == s ) { return Platform_E::GENAPSYS; }
            else if ( S_ULTIMA == s ) { return Platform_E::ULTIMA; }
            else if ( S_GENEMIND == s ) { return Platform_E::GENEMIND; }
            else if ( S_BGISEQ == s ) { return Platform_E::BGISEQ; }
            else if ( S_DNBSEQ == s ) { return Platform_E::DNBSEQ; }
            else if ( S_SINGULAR_GENOMICS == s ) { return Platform_E::SINGULAR_GENOMICS; }
            else if ( S_GENEUS_TECH == s ) { return Platform_E::GENEUS_TECH; }
            else if ( S_SALUS == s ) { return Platform_E::SALUS; }
            return Platform_E::INVALID;
        }

        Platform ( const std::string& s ) : f_type( from_string( s ) ) {}

    public:
        static Platform_ptr make( const std::string& s ) {
            return Platform_ptr( new Platform( s ) );
        }

        std::string to_string( void ) const {
            switch ( f_type ) {
                case Platform_E::UNDEFINED : return S_UNDEFINED; break;
                case Platform_E::PF454 : return S_454; break;
                case Platform_E::ILLUMINA : return S_ILLUMINA; break;
                case Platform_E::ABSOLID : return S_ABSOLID; break;
                case Platform_E::COMPLETE_GENOMICS : return S_COMPLETE_GENOMICS; break;
                case Platform_E::HELICOS : return S_HELICOS; break;
                case Platform_E::PACBIO_SMRT : return S_PACBIO_SMRT; break;
                case Platform_E::ION_TORRENT : return S_ION_TORRENT; break;
                case Platform_E::CAPILLARY : return S_CAPILLARY; break;
                case Platform_E::OXFORD_NANOPORE : return S_OXFORD_NANOPORE; break;
                case Platform_E::ELEMENT_BIO : return S_ELEMENT_BIO; break;
                case Platform_E::TAPESTRI : return S_TAPESTRI; break;
                case Platform_E::VELA_DIAG : return S_VELA_DIAG; break;
                case Platform_E::GENAPSYS : return S_GENAPSYS; break;
                case Platform_E::ULTIMA : return S_ULTIMA; break;
                case Platform_E::GENEMIND : return S_GENEMIND; break;
                case Platform_E::BGISEQ : return S_BGISEQ; break;
                case Platform_E::DNBSEQ : return S_DNBSEQ; break;
                case Platform_E::SINGULAR_GENOMICS : return S_SINGULAR_GENOMICS; break;
                case Platform_E::GENEUS_TECH : return S_GENEUS_TECH; break;
                case Platform_E::SALUS : return S_SALUS; break;
                case Platform_E::INVALID : return "INVALID"; break;
                default : return "UNKNOWN"; break;
            };
        }

        uint8_t to_u8( void ) const { return (uint8_t)f_type; }

        static std::string list_all( void ) {
            std::stringstream ss;
            ss << S_UNDEFINED << endl;
            ss << S_454 << endl;
            ss << S_ILLUMINA << endl;
            ss << S_ABSOLID << endl;
            ss << S_COMPLETE_GENOMICS << " | CG" << endl;
            ss << S_HELICOS << endl;
            ss << S_PACBIO_SMRT << endl;
            ss << S_ION_TORRENT << endl;
            ss << S_CAPILLARY << endl;
            ss << S_OXFORD_NANOPORE << endl;
            ss << S_ELEMENT_BIO << endl;
            ss << S_TAPESTRI << endl;
            ss << S_VELA_DIAG << endl;
            ss << S_GENAPSYS << endl;
            ss << S_ULTIMA << endl;
            ss << S_GENEMIND << endl;
            ss << S_BGISEQ << endl;
            ss << S_DNBSEQ << endl;
            ss << S_SINGULAR_GENOMICS << endl;
            ss << S_GENEUS_TECH << endl;
            ss << S_SALUS << endl;
            return ss . str();
        }
};

} // end of namespace sra_convert

#include <iostream>

#include <kapp/main.h>
#include <klib/rc.h>

#include <stdio.h>

#include <ngs/ncbi/NGS.hpp>
#include <ngs/ReferenceSequence.hpp>

#include "helper.h"
#include "common.h"

#define PARAM_ALG_SW "sw"
#define PARAM_ALG_RA "ra"

#ifdef _WIN32
#define PRSIZE_T "I"
#else
#define PRSIZE_T "z"
#endif

namespace VarExpand
{
    struct Params
    {
        ::RefVarAlg alg;
    } g_Params =
    {
        ::refvarAlgSW
    };

    char const OPTION_ALG[] = "algorithm";
    //char const ALIAS_ALG[]  = "a";
    char const* USAGE_ALG[] = { "the algorithm to use for searching. "
        "\"" PARAM_ALG_SW "\" means Smith-Waterman. "
        "\"" PARAM_ALG_RA "\" means Rolling bulldozer algorithm\n", NULL };

    ::OptDef Options[] =
    {
        { OPTION_ALG, NULL, NULL, USAGE_ALG, 1, true, false }
    };

    template <class TObject, typename TKey> class CNGSObject
    {
        TObject* m_self;
        TKey m_key;

    public:
        CNGSObject() : m_self(NULL) {}
        ~CNGSObject() { Release(); }
        CNGSObject ( CNGSObject const& x);
        CNGSObject& operator=(CNGSObject const& x);

        void Release()
        {
            delete m_self;
            m_self = NULL;
        }

        void Init(TObject const& obj, TKey const& key)
        {
            TObject* p = new TObject(obj); // may throw ?

            Release();
            m_self = p;
            m_key = key;
        }

        TKey const& GetKey() const { return m_key; }
        TObject const* GetSelfPtr() const { return m_self; }
    };

    bool check_ref_slice ( char const* ref, size_t ref_size )
    {
        bool ret = ref_size == 0;
        for ( size_t i = 0; i < ref_size; ++i )
        {
            if ( !(ref [i] == 'N' || ref [i] == 'n' || ref [i] == '.') )
            {
                ret = true; // at least one non-N base is OK
                break;
            }
        }
        return ret;
    }

    void get_ref_var_object (KSearch::CVRefVariation& obj, size_t ref_pos, size_t del_len,
        char const* allele, size_t allele_len, ngs::ReferenceSequence const& ref_seq,
        std::string & ref_allele)
    {
        size_t var_len = allele_len;

        size_t chunk_size = 5000; // TODO: add the method Reference[Sequence].getChunkSize() to the API
        size_t chunk_no = ref_pos / chunk_size;
        size_t ref_pos_in_slice = ref_pos % chunk_size;
        size_t bases_start = chunk_no * chunk_size;
        size_t chunk_no_last = ref_seq.getLength() / chunk_size;

        bool cont = false;
        size_t chunk_no_start = chunk_no, chunk_no_end = chunk_no;

        // optimization: first look into the current chunk only (using ngs::StringRef)
        {
            ngs::StringRef ref_chunk = ref_seq.getReferenceChunk ( bases_start );

            if ( ! check_ref_slice (ref_chunk.data() + ref_pos_in_slice, del_len) )
            {
                PLOGMSG ( klogWarn,
                    ( klogWarn,
                    "The selected reference region [$(REFSLICE)] does not contain valid bases, skipping...",
                    "REFSLICE=%.*s",
                    (int)del_len, ref_chunk.data() + ref_pos_in_slice ));
            }

            cont = Common::find_variation_core_step ( obj, g_Params.alg,
                ref_chunk.data(), ref_chunk.size(), ref_pos_in_slice,
                allele, var_len, del_len,
                chunk_size, chunk_no_last, bases_start, chunk_no_start, chunk_no_end );

            if ( !cont )
                ref_allele.assign (ref_chunk.data() + obj.GetAlleleStartRelative(), obj.GetAlleleLenOnRef());
        }

        // general case - expanding ref_slice to multiple chunks
        if ( cont )
        {
            ngs::String ref_slice;
            while ( cont )
            {
                ref_slice = ref_seq.getReferenceBases (
                    bases_start, (chunk_no_end - chunk_no_start + 1)*chunk_size );

                cont = Common::find_variation_core_step ( obj, g_Params.alg,
                    ref_slice.c_str(), ref_slice.size(), ref_pos_in_slice,
                    allele, var_len, del_len,
                    chunk_size, chunk_no_last, bases_start, chunk_no_start, chunk_no_end );
            }
            ref_allele.assign (ref_slice.c_str() + obj.GetAlleleStartRelative(), obj.GetAlleleLenOnRef());
        }
    }

    void expand_variation ( //ngs::ReferenceSequence const& ref_seq,
                    CNGSObject <ngs::ReferenceSequence, ncbi::String>& ref_obj,
                    char const* key, size_t key_len,
                    char const* ref_name, size_t ref_name_len,
                    size_t ref_pos, size_t del_len,
                    char const* allele, size_t allele_len )
    {
        ncbi::String sref_name ( ref_name, ref_name_len );
        if ( ref_obj.GetSelfPtr() == NULL || ref_obj.GetKey() != sref_name )
        {
            try
            {
                ref_obj.Init (ncbi::NGS::openReferenceSequence(sref_name), sref_name);
            }
            catch (ngs::ErrorMsg const& e)
            {
                if ( strstr ( e.what(), "failed to open table" ) == NULL )
                    throw;
                return;
            }
        }

        ngs::ReferenceSequence const& ref_seq = * ref_obj.GetSelfPtr();

        KSearch::CVRefVariation obj;
        std::string ref_allele;

        get_ref_var_object ( obj, ref_pos, del_len, allele, allele_len, ref_seq, ref_allele );

        if ( ref_allele.size() == 0 )
            ref_allele = "-";

        size_t new_allele_size = obj.GetAlleleSize();
        char const* new_allele = obj.GetAllele();

        if ( new_allele_size == 0)
        {
            new_allele = "-";
            new_allele_size = 1;
        }

        printf (
            "%.*s\t%.*s:%" PRSIZE_T "u:%" PRSIZE_T "u:%.*s\t%.*s:%" PRSIZE_T "u:%" PRSIZE_T "u:%.*s\t%.*s:%" PRSIZE_T "u:%" PRSIZE_T "u:%s\n",
            (int)key_len, key,

            (int)ref_name_len, ref_name,
            ref_pos, del_len,
            (int)allele_len, allele,

            (int)ref_name_len, ref_name,
            obj.GetAlleleStartAbsolute(), obj.GetAlleleLenOnRef(),
            (int)new_allele_size, new_allele,

            (int)ref_name_len, ref_name,
            obj.GetAlleleStartAbsolute(), obj.GetAlleleLenOnRef(),
            ref_allele.c_str()
            );
    }


    bool is_eol (char ch)
    {
        return ch == '\0' || ch == '\r' || ch == '\n';
    }

    bool is_sep (char ch, char const* sz_separators)
    {
        return strchr (sz_separators, ch) != NULL;
    }

    bool parse_number ( char const* num_str, size_t num_str_len, size_t* parsed_val )
    {
        * parsed_val = 0;
        for ( size_t i = 0; i < num_str_len; ++i )
        {
            char ch = num_str[i];
            if ( ch < '0' || ch > '9' )
                return false;

            size_t digit = ch - '0';
            *parsed_val = *parsed_val * 10 + digit;
        }
        return true;
    }

    bool parse_input_line ( char const* line, size_t line_size,
        char const** pkey, size_t* pkey_len,
        char const** pref_name, size_t* pref_name_len,
        char const** pallele, size_t* pallele_len,
        size_t* pref_pos, size_t* pdel_len )
    {
        size_t i = 0;
        char const* pstr;
        size_t str_len;

        char const SEP_SPACE[] = " \t\n\r";
        char const SEP_COLON[] = ":\n\r";

        // key
        pstr = & line[i];
        str_len = 0;
        for (; i < line_size && !is_sep(line[i], SEP_SPACE) && !is_eol(line[i]); ++i, ++str_len);
        if (i == line_size || str_len == 0 || is_eol(line[i]))
            return false;
        *pkey = pstr;
        *pkey_len = str_len;
        for (++i; i < line_size && is_sep(line[i], SEP_SPACE) && !is_eol(line[i]); ++i);

        // ref_name
        pstr = & line[i];
        str_len = 0;
        for (; i < line_size && !is_sep(line[i], SEP_COLON) && !is_eol(line[i]); ++i, ++str_len);
        if (i == line_size || str_len == 0 || is_eol(line[i]))
            return false;
        *pref_name = pstr;
        *pref_name_len = str_len;

        // ref_pos
        ++i;
        pstr = & line[i];
        str_len = 0;
        for (; i < line_size && !is_sep(line[i], SEP_COLON) && !is_eol(line[i]); ++i, ++str_len);
        if (i == line_size
            || str_len == 0
            || is_eol (line[i])
            || ! parse_number (pstr, str_len, pref_pos))
        {
            return false;
        }

        // del_len
        ++i;
        pstr = & line[i];
        str_len = 0;
        for (; i < line_size && !is_sep(line[i], SEP_COLON) && !is_eol(line[i]); ++i, ++str_len);
        if (i == line_size
            || str_len == 0
            || is_eol (line[i])
            || ! parse_number (pstr, str_len, pdel_len))
        {
            return false;
        }

        // allele
        ++i;
        pstr = & line[i];
        str_len = 0;
        for (; i < line_size && !is_sep(line[i], SEP_COLON) && !is_eol(line[i]); ++i, ++str_len);
        *pallele = pstr;
        *pallele_len = str_len;
        // treat "-" as ""
        if (*pallele_len == 1 && (*pallele)[0] == '-')
            *pallele_len = 0;

        return true;
    }

    void process_input_line (
        CNGSObject <ngs::ReferenceSequence, ncbi::String>& ref_obj,
        char const* line, size_t line_size )
    {
        char const* key, *ref_name, *allele;
        size_t key_len, ref_name_len, allele_len, ref_pos, del_len;

        if ( parse_input_line ( line, line_size,
            & key, & key_len,
            & ref_name, & ref_name_len,
            & allele, & allele_len,
            & ref_pos, & del_len ) )
        {
            expand_variation ( ref_obj, key, key_len,
                ref_name, ref_name_len,
                ref_pos, del_len,
                allele, allele_len );
        }
    }

    int expand_variations_impl ( )
    {
        std::string line;
        CNGSObject <ngs::ReferenceSequence, ncbi::String> ref_obj;
        while ( std::getline ( std::cin, line ) )
        {
            if (line.size() > 0)
                process_input_line ( ref_obj, line.c_str(), line.size() );
        }

        return 0;
    }

    int expand_variations (int argc, char** argv)
    {
        int ret;
        try
        {
            KApp::CArgs args;
            args.MakeAndHandle (argc, argv, Options, countof (Options));

            if (args.GetOptionCount (OPTION_ALG) == 1)
            {
                char const* alg = args.GetOptionValue ( OPTION_ALG, 0 );
                if (!strcmp(alg, PARAM_ALG_SW))
                    g_Params.alg = ::refvarAlgSW;
                else if (!strcmp(alg, PARAM_ALG_RA))
                    g_Params.alg = ::refvarAlgRA;
                else
                {
                    PLOGMSG ( klogErr, ( klogErr,
                        "Error: Unknown algorithm specified: \"$(ALG)\"", "ALG=%s", alg ));
                    return 3;
                }
            }

            ret = expand_variations_impl ();
        }
        catch ( ngs::ErrorMsg const& e )
        {
            PLOGMSG ( klogErr,
                ( klogErr, "ngs::ErrorMsg: $(WHAT)", "WHAT=%s", e.what()
                ));
            ret = 3;
        }
        catch (...)
        {
            Utils::HandleException ();
            ret = 3;
        }

        return ret;
    }
}

extern "C"
{
    const char UsageDefaultName[] = "var-expand";

    rc_t CC UsageSummary (const char * progname)
    {
        OUTMSG (("\nFor each pair (key, variation spec) in input produces the expanded variation spec\n\n"));
        return 0;
    }

    rc_t CC Usage ( struct Args const * args )
    {
        rc_t rc = 0;
        const char* progname = UsageDefaultName;
        const char* fullpath = UsageDefaultName;

        if (args == NULL)
            rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
        else
            rc = ArgsProgram(args, &fullpath, &progname);

        UsageSummary (progname);


        OUTMSG (("\nInput: the stream of lines in the format: <key> <tab> <input variation>\n\n"));
        OUTMSG (("\nOptions:\n"));

        HelpOptionLine (NULL, VarExpand::OPTION_ALG, "value", VarExpand::USAGE_ALG);

        XMLLogger_Usage();

        HelpOptionsStandard ();

        HelpVersion (fullpath, KAppVersion());

        return rc;
    }

    MAIN_DECL(argc, argv)
    {
        VDB::Application app( argc, argv );
        if (!app)
        {
            return VDB_INIT_FAILED;
        }

        SetUsage( Usage );
        SetUsageSummary( UsageSummary );
        return VarExpand::expand_variations (argc, app.getArgV());
    }
}

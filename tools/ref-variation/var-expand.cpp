#include "var-expand.vers.h"

#include <kapp/main.h>
#include <klib/rc.h>
#include <klib/printf.h>

#include <iostream>
#include <stdio.h>

#include <ngs/ncbi/NGS.hpp>
#include <ngs/ReferenceSequence.hpp>

#include "helper.h"
#include "common.h"

namespace VarExpand
{
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
        char const* allele, size_t allele_len, ngs::ReferenceSequence const& ref_seq)
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
            
            cont = Common::find_variation_core_step ( obj,
                ref_chunk.data(), ref_chunk.size(), ref_pos_in_slice,
                allele, var_len, del_len,
                chunk_size, chunk_no_last, bases_start, chunk_no_start, chunk_no_end );
        }

        // general case - expanding ref_slice to multiple chunks
        if ( cont )
        {
            ngs::String ref_slice;
            while ( cont )
            {
                ref_slice = ref_seq.getReferenceBases (
                    bases_start, (chunk_no_end - chunk_no_start + 1)*chunk_size );

                cont = Common::find_variation_core_step ( obj,
                    ref_slice.c_str(), ref_slice.size(), ref_pos_in_slice,
                    allele, var_len, del_len,
                    chunk_size, chunk_no_last, bases_start, chunk_no_start, chunk_no_end );
            }
        }
    }

    void expand_variation ( ngs::ReferenceSequence const& ref_seq,
                        char const* key, size_t key_len,
                        char const* ref_name, size_t ref_name_len,
                        size_t ref_pos, size_t del_len,
                        char const* allele, size_t allele_len )
    {
        KSearch::CVRefVariation obj;

        get_ref_var_object ( obj, ref_pos, del_len, allele, allele_len, ref_seq );

        char buf[512];
        size_t new_allele_size;
        char const* new_allele = obj.GetAllele ( new_allele_size );
        string_printf ( buf, countof(buf), NULL,
            "%.*s\t%.*s:%zu:%zu:%.*s\t%.*s:%zu:%zu:%.*s",
            (int)key_len, key,

            (int)ref_name_len, ref_name,
            ref_pos, del_len,
            (int)allele_len, allele,

            (int)ref_name_len, ref_name,
            obj.GetAlleleStartAbsolute(), obj.GetAlleleLenOnRef(),
            (new_allele_size), new_allele
            );
        buf [countof(buf) - 1] = '\0';
        printf ("%s\n", buf);
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
#if 0
    void process_input_line ( char const* line, size_t line_size )
    {
        char const* key, *ref_name, *allele;
        size_t key_len, ref_name_len, allele_len, ref_pos, del_len;

        if ( parse_input_line ( line, line_size,
            & key, & key_len,
            & ref_name, & ref_name_len,
            & allele, & allele_len,
            & ref_pos, & del_len ) )
        {
            expand_variation ( key, key_len,
                ref_name, ref_name_len,
                ref_pos, del_len,
                allele, allele_len );
        }
    }
#endif
    int expand_variations_impl ( )
    {
        std::string line;
        bool end_of_stream = false;

        ncbi::String sref_name, sref_name_prev;
        char const* key, *ref_name, *allele;
        size_t key_len, ref_name_len, allele_len, ref_pos, del_len;

        while ( ! end_of_stream )
        {
            end_of_stream = std::getline ( std::cin, line ).eof();
            if (line.size() > 0 && parse_input_line ( line.c_str(), line.size(),
                                    & key, & key_len,
                                    & ref_name, & ref_name_len,
                                    & allele, & allele_len,
                                    & ref_pos, & del_len ))
            {
                try // really only trying to open the first reference
                {
                    sref_name.assign ( ref_name, ref_name_len );
                    ngs::ReferenceSequence ref_seq = ncbi::NGS::openReferenceSequence(sref_name);
                    break;
                }
                catch (ngs::ErrorMsg const& e)
                {
                    if ( strstr ( e.what(), "failed to open table" ) == NULL )
                        throw;
                    else
                        continue;
                }
            }
        }

        if ( end_of_stream )
            return 0;

        // here we have the first good reference name in sref_name (shall be opened with no exceptions)
        ngs::ReferenceSequence ref_seq = ncbi::NGS::openReferenceSequence(sref_name);
        expand_variation ( ref_seq, key, key_len,
                ref_name, ref_name_len,
                ref_pos, del_len,
                allele, allele_len );
        sref_name_prev = sref_name;

        // process the next input lines
        while ( std::getline ( std::cin, line ) )
        {
            if (line.size() > 0 && parse_input_line ( line.c_str(), line.size(),
                                    & key, & key_len,
                                    & ref_name, & ref_name_len,
                                    & allele, & allele_len,
                                    & ref_pos, & del_len ))
            {
                sref_name.assign ( ref_name, ref_name_len );
                if (sref_name == sref_name_prev)
                {
                    expand_variation ( ref_seq, key, key_len,
                            ref_name, ref_name_len,
                            ref_pos, del_len,
                            allele, allele_len );
                }
                else
                {
                    try
                    {
                    
                        ref_seq = ncbi::NGS::openReferenceSequence(sref_name);
                        expand_variation ( ref_seq, key, key_len,
                                ref_name, ref_name_len,
                                ref_pos, del_len,
                                allele, allele_len );
                        sref_name_prev = sref_name;
                    }
                    catch (ngs::ErrorMsg const& e)
                    {
                        if ( strstr ( e.what(), "failed to open table" ) == NULL )
                            throw;
                        else
                            continue;
                    }
                }
            }
        }

        return 0;
    }

    int expand_variations (int argc, char** argv)
    {
        int ret;
        try
        {
            KApp::CArgs args;
            args.MakeAndHandle (argc, argv, NULL, 0);
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
    ver_t CC KAppVersion ()
    {
        return VAR_EXPAND_VERS;
    }

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
        OUTMSG (("\nOptions: no options\n"));

        XMLLogger_Usage();

        HelpOptionsStandard ();

        HelpVersion (fullpath, KAppVersion());

        return rc;
    }

    rc_t CC KMain ( int argc, char *argv [] )
    {
        return VarExpand::expand_variations (argc, argv);
    }
}

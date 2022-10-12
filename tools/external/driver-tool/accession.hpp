/* ===========================================================================
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
 * Project:
 *  sratools command line tool
 *
 * Purpose:
 *  NCBI SRA accession parsing
 *
 */

#pragma once
#include <string>
#include <cstring>
#include "util.hpp"

namespace sratools {

enum AccessionType {
    unknown = 0,
    submitter = 'A',
    project = 'P',
    run = 'R',
    study = 'S',
    experiment = 'X'
};

struct Accession {
    std::string value;
    std::string::size_type alphas, digits, vers, verslen, ext;
    bool valid;
    
    Accession(std::string const &value);
    std::string version() const {
        if (valid && digits + alphas < vers && 0 < verslen)
            return value.substr(vers, verslen);
        return value.substr(0, 0);
    }
    std::string extension() const {
        if (valid && digits + alphas <= ext)
            return value.substr(ext);
        return value.substr(0, 0);
    }
    std::string accession() const {
        return valid ? value.substr(0, digits + alphas) : value.substr(0, 0);
    }
    enum AccessionType type() const;
    
    static char const *extensions[];
    static char const *qualityTypes[];
    static char const *qualityTypeForFull;
    static char const *qualityTypeForLite;
    
    static char const *qualityTypeFor(char const *const extension) {
        if (extension == extensions[0] || extension == extensions[4])
            return qualityTypes[0];
        if (extension == extensions[1] || extension == extensions[2] || extension == extensions[3])
            return qualityTypes[1];
        if (strcmp(extension, extensions[0]) == 0)
            return qualityTypes[0];
        for (auto ext : make_sequence(&extensions[1], 3)) {
            if (strcmp(extension, ext) == 0)
                return qualityTypes[1];
        }
        return nullptr;
    }
    
    static decltype(make_sequence(&extensions[0], 4)) extensionsFor(bool wantFullQuality) {
        return make_sequence(&extensions[wantFullQuality ? 0 : 1], 4);
    }
    
    /// \returns [{index in extensions, index in value}]
    std::vector<std::pair<unsigned, unsigned>> sraExtensions() const;
    
    /// \returns [{index, length}] where value[index] == '.'; does not include the version.
    std::vector<std::pair<unsigned, unsigned>> allExtensions() const;
};

}

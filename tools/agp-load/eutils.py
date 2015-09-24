#!python
# =============================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# =============================================================================
"""
    Utility class for getting sequence and metadata for accessions via eutils.
"""

import httplib
import json

class EUtils:
    @classmethod
    def _response(cls, function, *args):
        """ Builds URL and gets it """
        url = "/entrez/eutils/{}.fcgi?".format(function)+"&".join(args)
        try:
            eutils = httplib.HTTPConnection("eutils.ncbi.nlm.nih.gov")
            eutils.request('GET', url)
            return eutils.getresponse()
        except:
            return None

    @classmethod
    def _giList(cls, acc):
        """ gets gi for accession """
        response = EUtils._response("esearch", "db=nuccore", "retmode=json", "term={}".format(acc))
        if response:
            return json.load(response)['esearchresult']['idlist']
        return None

    @classmethod
    def summary(cls, acc):
        """
            Retrieves the metadata for an accession
            Returns a dictionary
        """
        gi = EUtils._giList(acc)
        if gi:
            response = EUtils._response("esummary", "db=nuccore", "retmode=json", "id={}".format(gi[0]))
            if response:
                return json.load(response)["result"]["{}".format(gi[0])]
        return None

    @classmethod
    def fasta(cls, acc):
        """
            Retrieves the FASTA sequence for an accession
            Returns a generator on the lines of the FASTA sequence
        """
        gi = EUtils._giList(acc)
        if gi:
            response = EUtils._response("efetch", "db=nuccore", "retmode=text", "rettype=fasta", "id={}".format(gi[0]))
            def lines():
                buf = response.read(4096)
                at = 0
                while len(buf) > 0:
                    if at >= 4096:
                        buf = buf[4096:]
                        at -= 4096
                    next = buf.find("\n", at)
                    if next < 0:
                        more = response.read(4096)
                        if more:
                            buf += more
                            continue
                        line = buf[at:]
                        buf = ""
                        at = 0
                        yield line
                    else:
                        line = buf[at:next]
                        at = next + 1
                        yield line
            return lines()
        return None

if __name__ == "__main__":
    import sys

    if sys.argv[1] == "summary":
        for acc in sys.argv[2:]:
            summary = EUtils.summary(acc)
            if summary:
                for k in sorted(summary):
                    print("{}: {}".format(k,summary[k]))
            else:
                print("Nothing found for {}".format(acc))
    elif sys.argv[1] == "fasta":
        for acc in sys.argv[2:]:
            for line in EUtils.fasta(acc):
                print(line)

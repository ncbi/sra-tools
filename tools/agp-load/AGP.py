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


import tarfile
import gzip
import os
from eutils import EUtils

class _loc:
    def __init__(self, mol, start, last):
        self.mol = mol
        self.start = start
        self.last = last

    def dir(self):
        if self.last < self.start:
            return -1
        else:
            return 1

    def length(self):
        if self.last < self.start:
            return (self.start - self.last) + 1
        else:
            return (self.last - self.start) + 1

    def split(self, at):
        if self.last < self.start:
            left = _loc(self.mol, self.start, self.start - at + 1)
            right = _loc(self.mol, self.start - at, self.last)
        else:
            left = _loc(self.mol, self.start, self.start + at - 1)
            right = _loc(self.mol, self.start + at, self.last)
        return left, right

    def merge(self, other):
        if self.mol != other.mol:
            return None
        if self.last + self.dir() != other.start:
            return None
        return _loc(self.mol, self.start, other.last)

    def __str__(self):
        return "\t".join([self.mol, str(self.start), str(self.last)])

class map_pair:
    def __init__(self, first, second):
        if first.last >= first.start:
            self.first = first
            self.second = second
        else:
            self.first = _loc(first.mol, first.last, first.start)
            self.second = _loc(second.mol, second.last, second.start)

    def dir(self):
        return self.second.dir()

    def agp(self):
        r = [self.first.mol, self.first.start, self.first.last, 0, 'O', self.second.mol, self.second.start, self.second.last, 1]
        if r[7] < r[6]:
            r[6], r[7], r[8] = r[7], r[6], -1
        return r

    def merge(self, other):
        if self.dir() != other.dir(): return None
        first = self.first.merge(other.first)
        if first == None: return None
        second = self.second.merge(other.second)
        if second == None: return None
        return map_pair(first, second)

    @classmethod
    def cmp(cls, a, b):
        if a.first.mol < b.first.mol: return -1
        if a.first.mol > b.first.mol: return 1
        apos = a.first.start if a.first.start < a.first.last else a.first.last
        bpos = b.first.start if b.first.start < b.first.last else b.first.last
        if apos < bpos: return -1
        if apos > bpos: return 1
        return 0

class _loc_pair:
    def __init__(self, first):
        self.first = first
        self.second = []

    def add(self, second):
        self.second.append(second)

    def split(self, at):
        cut = at - self.first.start
        l1, r1 = self.first.split(cut)
        left = _loc_pair(l1)
        right = _loc_pair(r1)
        for s in self.second:
            l2, r2 = s.split(cut)
            assert(l1.length() == l2.length())
            assert(r1.length() == r2.length())
            left.add(l2)
            right.add(r2)
        return left, right

    def __str__(self):
        rslt = ''
        for s in self.second:
            rslt += "\t".join(map((lambda a: str(a)), [self.first, s]))+"\n"
        return rslt

    def agp(self, rev=False):
        rslt = []
        for s in self.second:
            start, last, dir = s.start, s.last, 1
            if last < start:
                start, last, dir = last, start, -1
            r = [s.mol, start, last, 0, "O", self.first.mol, self.first.start, self.first.last, dir] if rev else [self.first.mol, self.first.start, self.first.last, 0, "O", s.mol, start, last, dir]
            rslt.append(r)
        return rslt

    @classmethod
    def make_pair(cls, first, second):
        rslt = _loc_pair(first)
        rslt.add(second)
        return rslt

class _loc_set:
    def __init__(self):
        self.set = []

    def _overlap(self, first):
        f = 0
        e = len(self.set)
        while f < e:
            m = f + (e - f)/2
            fnd = self.set[m]
            if fnd.first.start > first.last:
                e = m
                continue
            if fnd.first.last < first.start:
                f = m + 1
                continue
            return m, None
        return None, f

    def _add(self, first, second):
        m, f = self._overlap(first)
        if m == None:
            self.set.insert(f, _loc_pair.make_pair(first, second))
            return None
        if first.start == self.set[m].first.start and first.last == self.set[m].first.last:
            self.set[m].add(second)
            return None
        return m

    def split(self, at):
        """ Search the list for the record that overlaps the split point, then
            split that record. No split occurs if the split point is not fully
            inside any record, in particular, the start is not 'inside'
        """
        f = 0
        e = len(self.set)
        while f < e:
            m = f + (e - f)/2
            fnd = self.set[m]
            if fnd.first.start > at:
                e = m
                continue
            if fnd.first.last < at:
                f = m + 1
                continue
            if fnd.first.start < at:
                l, r = fnd.split(at)
                self.set[m:m+1] = [l, r]
            break

    def add(self, first, second):
        if len(self.set) == 0:
            self.set.append(_loc_pair.make_pair(first, second))
            return
        else:
            assert(first.mol == self.set[0].first.mol)
        self.split(first.start)
        self.split(first.last + 1)
        m = self._add(first, second)
        if m == None: return
        if self.set[m].first.start > first.start:
            cut = self.set[m].first.start - first.start
            l1, r1 = first.split(cut)
            l2, r2 = second.split(cut)
            self._add(l1, l2)
            self.add(r1, r2)
            return
        if self.set[m].first.last < first.last:
            cut = (first.last) - (self.set[m].first.last)
            l1, r1 = first.split(first.length() - cut)
            l2, r2 = second.split(second.length() - cut)
            self._add(r1, r2)
            self.add(l1, l2)
            return

    def intersect(self, other):
        if len(self.set) == 0 or len(other.set) == 0: return
        assert(self.set[0].first.mol == other.set[0].first.mol)
        for r in self.set:
            other.split(r.first.start)
            other.split(r.first.last + 1)
        for r in other.set:
            self.split(r.first.start)
            self.split(r.first.last + 1)
        i, j = 0, 0
        while i < len(self.set) and j < len(other.set):
            if self.set[i].first.start == other.set[j].first.start:
                assert(self.set[i].first.length() == other.set[j].first.length())
                i += 1
                j += 1
                continue
            if self.set[i].first.start < other.set[j].first.start:
                del self.set[i]
            else:
                del other.set[j]
        del self.set[i:]
        del other.set[j:]

    def agp(self, rev=False):
        rslt = []
        for r in self.set:
            rslt.extend(r.agp(rev))
        if rev:
            def cmp(a, b):
                if a[0] < b[0]: return -1
                if a[0] > b[0]: return 1
                if a[1] < b[1]: return -1
                if a[1] > b[1]: return 1
            rslt.sort(cmp)
        return rslt

    def __str__(self):
        return reduce((lambda a, b: a + str(b)), self.set, '')


def _loadFile(fh, file, func):
    def loadTarFile(fh, file, func):
        with tarfile.open(mode='r', fileobj=fh) as tar:
            for f in tar:
                i = tar.extractfile(f)
                _loadFile(i, os.path.join(file, f.name), func)
                i.close()

    def loadText(fh, file, func):
        for i, line in enumerate(fh):
            func(line, "{}:{}".format(file, i))

    try:
        loadTarFile(fh, file, func)
        return
    except tarfile.ReadError:
        pass
    try:
        with gzip.GzipFile(fileobj=fh) as gz:
            loadText(gz, file[:-3] if file.endswith(".gz") else file, func)
            return
    except IOError:
        pass
    loadText(fh, file, func)


class AGP:
    def loadFile(self, fname):
        """ AGP Fields:
            object
            beg
            end
            partno      # not used, zero'ed out when flattened
            type        # gaps are ignored, infer them from the gaps between subobjects
            subobject
            sub_beg
            sub_end
            strand
            source      # ADDED, <file name>:<line number>, this is just for debugging
        """
        recs = []
        def cb(line, src):
            if line[0] == '#': return
            fld = line[:-1].split("\t")
            if len(fld) != 9: return
            if fld[4] == "N" or fld[4] == "U": return
            fld[0] = fld[0]
            fld[1] = int(fld[1])
            fld[2] = int(fld[2])
            fld[3] = int(fld[3])
            fld[6] = int(fld[6])
            fld[7] = int(fld[7])
            fld[8] = -1 if fld[8] == '-' else 1
            fld.append(src)
            assert(fld[2] - fld[1] == fld[7] - fld[6]) # lengths are the same
            recs.append(fld)
        with open(fname, "r") as fh:
            _loadFile(fh, fname, cb)
        self._data.extend(recs)

    def __init__(self):
        self._data = []

    def _topLevelObjects(self):
        sub = dict.fromkeys(v[5] for v in self._data)
        return dict.fromkeys(v[0] for v in self._data if v[0] not in sub)

    def topLevelObjects(self):
        return sorted(self._topLevelObjects().keys())

    def _index(self):
        """ Sorted by
                object (fld[0]),
                beg (fld[1]),
                end (fld[2]) desc
        """
        def index_cmp(a, b):
            if self._data[a][0] < self._data[b][0]:
                return -1
            if self._data[b][0] < self._data[a][0]:
                return 1
            if self._data[a][1] < self._data[b][1]:
                return -1
            if self._data[b][1] < self._data[a][1]:
                return 1
            if self._data[a][2] < self._data[b][2]:
                return 1
            if self._data[b][2] < self._data[a][2]:
                return -1
            return a - b
        return sorted(range(len(self._data)), index_cmp)

    def _clean(self):
        """ Returns a cleansed version of the input data
            without any redundant or duplicate records
        """
        rslt = []
        for i in self._index():
            v = self._data[i]
            if len(rslt) == 0:
                rslt.append(v)
                continue
            u = rslt[-1]
            if v[0] != u[0]: # different object
                rslt.append(v)
                continue
            if v[1] > u[2]:  # starts after the previous one ended
                rslt.append(v)
                continue
            if v[2] > u[2]:  # it's an error if it overlaps the previous one
                print("The record from {}".format(v[9]))
                print("\t".join(v))
                print("conflicts with the record from {}".format(rslt[-1][9]))
                print("\t".join(rslt[-1]))
                assert("The AGP data contains errors:")
        return rslt

    def _flattened(self, args):
        """ yields list of flattened AGP """
        index = {}
        for v in self._clean():
            if v[0] in index:
                index[v[0]].append(v)
            else:
                index[v[0]] = [v]
        isflat = {k:False for k in index}
        def flatten(k):
            flat = []
            for v in index[k]:
                sub = v[5]
                if sub not in index: # has no subparts
                    flat.append(v[0:3] + [0] + v[4:-1])
                    continue
                assert(v[8] > 0) # multipart objects aren't reversable?
                if not isflat[sub]: flatten(sub)
                for f in index[sub]:
                    if f[2] < v[6] or f[1] > v[7]:
                        continue
                    # parts aren't splitable
                    assert(f[1] >= v[6] and f[2] <= v[7])
                    n = [v[0], f[1]+v[1]-1, f[2]+v[1]-1, 0] + f[4:]
                    flat.append(n)
            index[k] = flat
            isflat[k] = True
        for k in args:
            flatten(k)
            yield index[k]

    def flattened(self, args=[]):
        objects = self._topLevelObjects() if len(args) == 0 else args
        for l in self._flattened(objects):
            for r in l:
                yield r

    def _leaves(self):
        rslt = {}
        for l in self._flattened(self._topLevelObjects().keys()):
            for r in l[:]:
                if r[5] not in rslt:
                    rslt[r[5]] = _loc_set()
                if r[8] < 0:
                    other = _loc(r[0], r[2], r[1])
                else:
                    other = _loc(r[0], r[1], r[2])
                rslt[r[5]].add(_loc(r[5], r[6], r[7]), other)
        return rslt

    def remap(self, other):
        def getSummary(acc):
            while True:
                summary = EUtils.summary(acc)
                if summary:
                    return summary

        superceeds_memo = {}
        def superceeds(a, b):
            try: return superceeds_memo[b] == a
            except KeyError:
                pass
            assemblyacc = None
            accessionversion = None
            replacedby = None
            try:
                assemblyacc = getSummary(a)['assemblyacc']
                accessionversion = getSummary(a)['accessionversion']
                replacedby = getSummary(b)['replacedby']
            except KeyError:
                pass
            if replacedby == None:
                superceeds_memo[b] = None
                return False
            if replacedby == assemblyacc or replacedby == accessionversion:
                superceeds_memo[b] = a
                return True
            return False

        def compact(l):
            last = None
            for r in sorted(l, map_pair.cmp):
                if last == None:
                    last = r
                    continue
                x = last.merge(r)
                if x:
                    last = x
                    continue
                y = last.agp()
                last = r
                yield y
            if last != None: yield last.agp()

        def remapper():
            s = self._leaves()
            o = other._leaves()
            for k in sorted(s.keys()):
                if k not in o: continue
                s[k].intersect(o[k])
                n = len(s[k].set)
                assert(n == len(o[k].set))
                for i in range(0, n):
                    src = s[k].set[i]
                    dst = o[k].set[i]
                    assert(src.first.start == dst.first.start)
                    assert(src.first.last  == dst.first.last )
                    src = src.second
                    dst = dst.second
                    if len(src) == 1 and len(dst) == 1:
                        yield map_pair(dst[0], src[0])
                    else:
                        recs = [(r1, r2) for r1 in src for r2 in dst if superceeds(r2.mol, r1.mol)]
                        for r in recs:
                            yield map_pair(r[1], r[0])

        return compact(remapper())


if __name__ == "__main__":
    import sys
    import subprocess
    import time


    def complement(b):
        if b == 'A': return 'T'
        if b == 'C': return 'G'
        if b == 'G': return 'C'
        if b == 'T': return 'A'
        return 'N'

    def clean(b):
        if b == 'A': return 'A'
        if b == 'C': return 'C'
        if b == 'G': return 'G'
        if b == 'T': return 'T'
        return 'N'

    def fetchFasta(acc, end):
        sleep = 0
        while True:
            fasta = ''
            defline = None
            sleep += 1
            try:
                for line in EUtils.fasta(acc):
                    if line.startswith(">"):
                        if defline == None:
                            defline = line
                        else:
                            break
                    else:
                        fasta += line
                if defline != None:
                    if end <= len(fasta):
                        return (defline, fasta)
                    msg = "got truncated response for {}; retrying in a moment".format(acc)
                else:
                    msg = "got no response for {}; retrying in a moment".format(acc)
            except KeyboardInterrupt:
                sys.stderr.write(" cancelled\n")
                exit(0)
            except:
                msg = "error fetching {}; retrying in a moment".format(acc)
                pass
            if sleep > 10:
                sys.stderr.write(" too many retries; try again later.\n")
                exit(1)
            sys.stderr.write(msg)
            sys.stderr.flush()
            time.sleep(sleep)
            sys.stderr.write("\033[{}D\033[K".format(len(msg)))
            sys.stderr.flush()


    cache = {}
    def fetchObject(acc, start=0, end=-1, rev=False):
        global cache
        if acc in cache:
            fasta = cache[acc]
        else:
            (defline, fasta) = fetchFasta(acc, end)
            cache[acc] = fasta
        if end < 0: end = len(fasta)
        assert(end <= len(fasta))
        assert(start >= 0)
        assert(start < end)
        if not rev:
            return ''.join(map(clean, list(fasta[start:end])))
        rslt = list(fasta[start:end])
        i = 0
        j = len(rslt)
        while (i < j):
            j -= 1
            A = complement(rslt[i])
            B = complement(rslt[j])
            rslt[j] = A
            rslt[i] = B
            i += 1
        return ''.join(rslt)

    def usage():
        print("{} [[ test | dump ] <agp source file> | remap <from agp> <to agp>]".format(sys.argv[0]))
        exit(1)

    agps = []
    try:
        for file in sys.argv[2:]:
            agp = AGP()
            agp.loadFile(file)
            agps.append(agp)
    except IndexError:
        usage()

    if len(agps) == 2:
        for r in agps[0].remap(agps[1]):
            print(r)
        exit(0)

    def unflat(agp):
        un = {}
        for r in agp.flattened():
            try: un[r[0]].append(r)
            except KeyError:
                un[r[0]] = [r]
        return un

    def test(agp):
        flat = unflat(agp)
        seqID = sorted(flat.keys())
        for object in seqID:
            next = 1
            test = ''
            sys.stderr.write("reconstructing {}  ".format(object))
            sys.stderr.flush()
            part = 0
            count = len(flat[object])
            progress = ''
            for r in flat[object]:
                erase = len(progress)
                progress = '.'*int((part * 100.0) / count)
                sys.stderr.write("\x08"*erase + progress)
                read = fetchObject(r[5], r[6] - 1, r[7], r[8] < 0)
                test += "N"*(r[1] - next)
                test += read
                part += 1
                next = r[2] + 1
            sys.stderr.write(" ")
            cache = {}
            (dummy, gold) = fetchFasta(object, next)
            gold = ''.join(map(clean, list(gold)))
            if len(test) < len(gold):
                test += "N"*(len(gold)-len(test))
            if gold == test:
                sys.stderr.write(" PASSED\n".format(object))
            else:
                sys.stderr.write(" FAILED\n".format(object))
                with open("{}.reconstructed.fasta".format(object), 'w') as fh:
                    fh.write(">{}\n".format(object))
                    i = 0
                    while i < len(test):
                        fh.write("{}\n".format(test[i:i+70]))
                        i += 70
                with open("{}.expected.fasta".format(object), 'w') as fh:
                    fh.write(">{}\n".format(object))
                    i = 0
                    while i < len(gold):
                        fh.write("{}\n".format(gold[i:i+70]))
                        i += 70
                exit(1)
        sys.stderr.write("All passed\n")

    def dump(agp):
        flat = unflat(agp)
        seqID = sorted(flat.keys())
        for object in seqID:
            for r in flat[object]:
                print("\t".join(map((lambda x: str(x)), r)))

    if sys.argv[1] == "test":
        test(agps[0])
    elif sys.argv[1] == "dump":
        dump(agps[0])
    elif sys.argv[1] == "remap" and len(agps) == 2:
        for r in agps[0].remap(agps[1]):
            print(r)
    else:
        usage()

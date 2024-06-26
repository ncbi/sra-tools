#!python3
import sys
import json
from argparse import ArgumentParser

assumeMissingIsEqual = False
''' Require given values to be equal, and missing nodes are assumed to be equal.
    This allows a reduced set of statistics to be compared to a complete set.
'''

diffs = []
''' Accumulates any observed differences. Gets printed at the end. '''


def recordDiffers(orig, cmpr, path):
    ''' Record a value difference. '''
    try:
        if path[-2] == 'hash' and path[-1] == 'bases':
            return
    except IndexError:
        pass
    diffs.append({'path': path, 'expected': orig, 'found': cmpr})


def compareFloat(a, b):
    """ Do two floats differ by more than ~1 part per 10^5 """
    if a > b:
        return compareFloat(b, a)
    if a < 0.0:
        return compareFloat(-b, -a) if b < 0.0 else False
    while a > 1.0 and b > 1.0:
        a /= 2.0;
        b /= 2.0;
    while a < 0.5 and b < 0.5:
        a *= 2.0;
        b *= 2.0;
    return (b - a) * 131072.0 < 1.0


def ignoreMissing(node, path):
    ''' Some differences are ignored, e.g. if an empty node is missing. '''
    try:
        # must check in an order that path indices don't cause short-circuiting exceptions

        if path[-1] == 'bases' and len(node) == 0:
            return True

        if path[-1] == 'spots' and len(node) == 0:
            return True

        if path[-1] == 'references' and len(node) == 0:
            return True

        if path[-1] == 'groups' and len(node) == 0:
            return True

        if path[-2] == 'bases': # missing element of the bases array
            if node.get('biological', 0) + node.get('technical', 0) == 0:
                return True

        if path[-2] == 'spots': # missing element of the spots array
            if node.get('total', 0) == 0:
                return True

        if path[-2] == 'references': # missing element of the references array
            if node.get('alignments', {}).get('total', 0) == 0:
                return True
            if len(node.get('chunks', [])) == 0:
                return True

        if path[-2] == 'chunks': # missing element of the chunks array
            if node.get('alignments', {}).get('total', 0) == 0:
                return True

        if path[-2] == 'reads': # missing member of the reads object
            if node.get('count', 0) == 0:
                return True

        if path[-2] == 'distances': # missing member of the distances object
            if node.get('count', 0) == 0:
                return True

        if path[-2] == 'hash' and path[-1] == 'bases':
            return True

        if path[-3] == 'bases': # missing member of the bases object
            if path[-1] in ['biological', 'technical'] and node == 0:
                return True

        if path[-3] == 'distances': # missing element of a distances/* array
            if node['count'] == 0:
                return True

        if path[-3] == 'spots':
            if node == 0:
                return True
    except KeyError:
        pass
    except IndexError:
        pass
    except TypeError:
        pass
    return False


def recordMissing(source, node, path):
    ''' Record a difference in structure. '''
    if (source == 'original' and assumeMissingIsEqual) or ignoreMissing(node, path):
        return
    diffs.append({'path': path, 'type': f'missing from {source}'})


id_getter = {
    'bases': lambda x: x['base'],
    'spots': lambda x: x['nreads'],
    'spectra': lambda x: x['wavelength'],
    'spot-layouts': lambda x: x['descriptor'],
    'references': lambda x: x['name'],
    'chunks': lambda x: x['start'],
    'groups': lambda x: x['group'],
}
''' Lambdas to get the identifying fields from a record in an array; keyed by the name of the array. '''


def idGetter(path):
    ''' Returns a lambda that extracts the primary key from the objects in a list. '''
    for key in reversed(path):
        try:
            return id_getter[key]
        except KeyError:
            pass
    raise KeyError


def compareStats(snode, cnode, path = []):
    ''' Recursive-descent-compare two JSON trees. '''

    def compareList():
        ''' Compare two JSON arrays. Equivalence is defined by the lambdas in id_getter.
        Returns
            True if original and compare nodes are both lists (and thus comparable).
        '''

        try:
            getID = idGetter(path)
            if len(snode) == len(cnode):
                sameOrder = True
                for i,s in enumerate(snode):
                    sid = getID(s)
                    cid = getID(cnode[i])
                    if sid != cid:
                        sameOrder = False
                        break
                if sameOrder:
                    for i,s in enumerate(snode):
                        compareStats(s, cnode[i], path + [getID(s)])
                    return True
            # generate indices
            sidx = {getID(v):i for i,v in enumerate(snode)}
            cidx = {getID(v):i for i,v in enumerate(cnode)}
        except TypeError:
            return False
        except KeyError:
            return False

        # Check for nodes in the original which are not in the compare.
        for k,i in sidx.items():
            if k not in cidx:
                recordMissing('comparee', snode[i], path + [k])

        for k,i in cidx.items():
            try:
                j = sidx[k]
            except KeyError:
                recordMissing('original', cnode[i], path + [k])
                continue

            compareStats(snode[j], cnode[i], path + [k])

        return True


    def compareKeyed():
        ''' Compare two JSON objects. Equivalence is by member name.
        Returns
            True if source and compare nodes are both keyed (and thus comparable).
        '''

        try:
            sharedKeys = [k for k in snode.keys() if k in cnode]
            for k in sharedKeys:
                compareStats(snode[k], cnode[k], path + [k]);
        except AttributeError:
            # doesn't have keys
            return False

        for k in cnode.keys():
            if k not in snode:
                recordMissing('original', cnode[k], path + [k])

        for k in snode.keys():
            if k not in cnode:
                recordMissing('comparee', snode[k], path + [k])

        return True


    # try to compare as keyed
    if compareKeyed():
        return

    # try to compare as lists
    if compareList():
        return

    try:
        # print(path, snode, cnode)
        # try to compare as values
        if snode == cnode:
            return
        if path[-1] == "power" and compareFloat(snode, cnode):
            return
        recordDiffers(snode, cnode, path)
    except:
        # Give up! With JSON data, this should not be reachable.
        diffs.append({'path': path, 'type': 'not comparable'})


def main():
    parser = ArgumentParser()
    parser.add_argument("--ignoreMissing", action='store_true', default=False, help="If set, assumes values not in source are equal.")
    parser.add_argument("source", help="The file containing the stats for the original.")
    parser.add_argument("compare", help="The file containing the stats to be compared against the original.")

    args = parser.parse_args()
    if args.ignoreMissing:
        global assumeMissingIsEqual
        assumeMissingIsEqual = True

    with open(args.source) as f:
        source = json.load(f)

    with open(args.compare) as f:
        compare = json.load(f)

    compareStats(source, compare)


main()
for diff in diffs:
    print(diff)

sys.exit(0 if len(diffs) == 0 else 1)

#!python3
import sys
import json
from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument("source", help="The file containing the stats for the original.")
parser.add_argument("compare", help="The file containing the stats to be compared against the original.")

args = parser.parse_args()

with open(args.source) as f:
    source = json.load(f)

with open(args.compare) as f:
    compare = json.load(f)

id_getter = {
    'bases': lambda x: (x['type'], x['base']) if 'base' in x else x['type'],
    'spots': lambda x: x['nreads'],
    'distances': lambda x: (x['base'], x['distance']),
    'references': lambda x: x['name'],
    'chunks': lambda x: x['start'],
    'groups': lambda x: x['group'],
}

diffs = []


def recordMissing(source, node, path):
    try:
        if path[-1] == 'bases' and len(node) == 0:
            return
        if path[-1] == 'spots' and len(node) == 0:
            return
        if path[-1] == 'distances' and len(node) == 0:
            return
        if path[-1] == 'references' and len(node) == 0:
            return
        if path[-1] == 'groups' and len(node) == 0:
            return
        if path[-2] == 'reads' and node['count'] == 0:
            return
        if path[-2] == 'bases' and node['count'] == 0:
            return
        if path[-2] == 'spots' and node['total'] == 0 and node['biological'] == 0 and node['technical'] == 0:
            return
        if path[-2] == 'chunks' and node['alignments']['total'] == 0:
            return
#         if path[-2] == 'references' and node['alignments']['total'] == 0:
#             return
#         if path[-2] == 'references' and ('chunks' not in node or len(node['chunks']) == 0):
#             return
    except KeyError:
        pass
    except IndexError:
        pass
    except TypeError:
        pass
    diffs.append({'path': path, 'type': f'missing from {source}'})

def compareStats(snode, cnode, path = []):
    def compareList():
        try:
            getID = id_getter[path[-1]]
            sidx = {getID(v):i for i,v in enumerate(snode)}
            cidx = {getID(v):i for i,v in enumerate(cnode)}
        except TypeError:
            return False
        except KeyError:
            return False

        for k,i in sidx.items():
            if k not in cidx:
                recordMissing('comparee', snode[i], path + [i])

        for k,i in cidx.items():
            try:
                j = sidx[k]
            except KeyError:
                recordMissing('original', cnode[i], path + [i])
                continue
            compareStats(snode[j], cnode[i], path + [i])

        return True

    def compareKeyed():
        sharedKeys = []

        try:
            sharedKeys = [k for k in snode.keys() if k in cnode]
        except AttributeError:
            # doesn't have keys
            return False

        for k in cnode.keys():
            if k not in snode:
                recordMissing('original', cnode[k], path + [k])

        for k in snode.keys():
            if k not in cnode:
                recordMissing('comparee', snode[k], path + [k])

        for k in sharedKeys:
            compareStats(snode[k], cnode[k], path + [k]);

        return True

    if compareKeyed():
        return
    if compareList():
        return

    try:
        if snode == cnode:
            return
        diffs.append({'path': path, 'type': 'differ'})
    except:
        diffs.append({'path': path, 'type': 'not comparable'})

compareStats(source, compare)
for diff in diffs:
    print(diff)

sys.exit(0 if len(diffs) == 0 else 1)

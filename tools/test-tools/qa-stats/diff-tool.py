#!python3

from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument("source", help="The file containing the stats for the original.")
parser.add_argument("compare", help="The file containing the stats to be compared against the original.")

args = parser.parse_args()

with open(args.source) as f:
    source = json.load(f)

with open(args.compare) as f:
    compare = json.load(f)

diffs = []
def compareStats(path, snode, cnode):
    try:
        skeys = {k:False for k in snode.keys()}
        for key in cnode.keys():
            newpath = f'{path}/{key}'
            try:
                skeys[key] = True
            except KeyError:
                diffs.append(f'{newpath} is new')
                continue
            compareStats(newpath, snode[key], cnode[key]);
        for key in skeys:
            if not skeys[key]:
                diffs.append(f'{path}/{key} is gone')
        return
    except AttributeError:
        # doesn't have keys
        pass

    try:
        skeys = {}
        for key, o in enumerate(cnode):
            skeys[key] = False
            newpath = f'{path}[key]'
            try:
                compareStats(newpath, snode[key], o);
                skeys[key] = True
            except IndexError:
                pass
        for key, _ in enumerate(snode):
            newpath = f'{path}[key]'
            try:
                if not skeys[key]:
                    diffs.append(f'{newpath} is new')
            except KeyError:
                diffs.append(f'{newpath} is gone')
        return
    except TypeError:
        # not iterable or not subscriptable
        pass

    try:
        if snode == cnode:
            return
        diffs.append(f'{path} differ')
    except:
        diffs.append(f'{path} are incomparable')

compareStats('', source, compare)
for diff in diffs:
    print(diff)

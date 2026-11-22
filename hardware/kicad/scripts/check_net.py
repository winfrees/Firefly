import sys; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from sexp import parse, find, first
from design import PARTS
t=parse(open(sys.argv[1]).read())
got={}
for n in find(first(t,'nets'),'net'):
    name=first(n,'name')[1]
    for nd in find(n,'node'):
        got[(first(nd,'ref')[1], first(nd,'pin')[1])]=name.lstrip('/')
bad=0
for p in PARTS:
    if p['ref'].startswith('#'): continue
    for pin,net in p['pins'].items():
        g=got.get((p['ref'],pin))
        if g!=net: bad+=1; print('MISMATCH',p['ref'],pin,net,g)
names={}
for k,v in got.items(): names.setdefault(v,[]).append(k)
for v,ks in sorted(names.items()): print(f'{v:10s} {len(ks):2d}', '' if len(ks)>1 else 'SINGLE-PIN!')
print('mismatches',bad)

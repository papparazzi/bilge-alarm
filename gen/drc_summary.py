import json,collections,sys
j=json.load(open(sys.argv[1]))
for sec in ('violations','unconnected_items'):
    L=j.get(sec,[]); print(sec,len(L), dict(collections.Counter((v['type'],v['severity']) for v in L)))
seen=set()
for sec in ('violations','unconnected_items'):
    for v in j.get(sec,[]):
        k=(v['type'],tuple(sorted(i['description'][:30] for i in v['items'])))
        if k in seen: continue
        seen.add(k)
        print(' -',v['type'],'|',v['description'][:95],'|',[i['description'][:55] for i in v['items']][:2],[(round(i['pos']['x'],1),round(i['pos']['y'],1)) for i in v['items']][:1])

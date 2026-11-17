#맵이 참조하는 에셋을 바이너리 내 '/Game/...' 패키지 경로로 재귀 추적해 폴더별 크기 집계 (에디터 없이 동작)
#서드파티 팩은 전체가 아니라 여기서 나온 목록만 커밋한다
#사용: python deps.py [맵 Maps/Lvl_X.umap] [맵 전용 에셋 폴더 Maps/X] [서드파티 폴더명 KiteDemo] [목록 출력 파일]
import os, re, sys

ROOT = 'D:/unreal/ActionPractice/Content'
DEFAULTS = ['Maps/Lvl_DeepForest.umap', 'Maps/DeepForest', 'KiteDemo',
            'D:/unreal/ActionPractice/Saved/MapLayout/DeepForest/kitedemo_deps.txt']
MAP, MAP_DIR, PACK, LIST_OUT = sys.argv[1:5] + DEFAULTS[len(sys.argv[1:5]):]
PAT = re.compile(rb'/Game/[A-Za-z0-9_./-]+')
BS = chr(92)

def norm(p):
    return os.path.normpath(p).replace(BS, '/')

def to_file(pkg):
    rel = pkg[len('/Game/'):].split('.')[0]
    for ext in ('.uasset', '.umap'):
        p = os.path.join(ROOT, rel + ext)
        if os.path.isfile(p):
            return norm(p)
    return None

todo = [norm(os.path.join(ROOT, MAP))]
for d, _, fs in os.walk(os.path.join(ROOT, MAP_DIR)):
    todo += [norm(os.path.join(d, f)) for f in fs]
seen = set()
while todo:
    f = todo.pop()
    if f in seen:
        continue
    seen.add(f)
    for m in set(PAT.findall(open(f, 'rb').read())):
        p = to_file(m.decode('ascii', 'ignore'))
        if p and p not in seen:
            todo.append(p)

groups = {}
for f in seen:
    rel = f[len(ROOT) + 1:]
    parts = rel.split('/')
    top = '/'.join(parts[:2]) if parts[0] in ('Maps', PACK) else parts[0]
    n, s = groups.get(top, (0, 0))
    groups[top] = (n + 1, s + os.path.getsize(f))
for k, (n, s) in sorted(groups.items(), key=lambda x: -x[1][1]):
    print('%-40s %5d files %9.1f MB' % (k, n, s / 1e6))
pack = sorted('Content/' + f[len(ROOT) + 1:] for f in seen if '/%s/' % PACK in f)
print('%s total' % PACK, len(pack), round(sum(os.path.getsize('D:/unreal/ActionPractice/' + k) for k in pack) / 1e6, 1), 'MB')
open(LIST_OUT, 'w').write('\n'.join(pack))

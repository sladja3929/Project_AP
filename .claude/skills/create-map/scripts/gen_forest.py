#숲 맵 지형·레이어·폴리지·게임플레이 좌표 생성 (순수 Python, numpy 없음). Lvl_DeepForest 제작에 사용한 원본
#사용: python gen_forest.py [출력 폴더]   (기본 Saved/MapLayout/DeepForest, 맵마다 복사해 ROUTE·CLEARINGS·GAMEPLAY 수정)
#좌표: 미터, X=북(정점 ix), Y=동(정점 iy), 랜드스케이프 정점 (0,0) = 월드 (0,0), 정점 간격 1m
import json, math, os, random, struct, sys, zlib

SEED = 20261117
random.seed(SEED)

N = 505
OUT = sys.argv[1] if len(sys.argv) > 1 else 'D:/unreal/ActionPractice/Saved/MapLayout/DeepForest'
HERE = OUT
os.makedirs(OUT + '/foliage', exist_ok=True)

K = '/Game/KiteDemo/Environments/'

#동선 노드 (x, y, 바닥 높이 m, 반폭 m)
ROUTE = [
    (78, 250, 0.0, 7.0),
    (95, 247, 0.0, 6.5),
    (118, 237, 0.4, 6.0),
    (150, 228, 1.0, 6.5),
    (180, 236, 1.5, 7.0),
    (205, 255, 1.8, 5.5),
    (225, 268, 2.0, 4.5),
    (255, 262, 2.5, 7.0),
    (283, 248, 4.0, 6.0),
    (303, 240, 6.0, 6.0),
    (325, 246, 7.0, 6.5),
    (347, 252, 8.5, 5.0),
    (362, 252, 6.0, 6.0),
    (382, 252, 5.5, 8.0),
]
#공터 (x, y, 바닥 높이, 반지름, 종류) 종류 1=공터, 2=보스룸(가파른 사면)
CLEARINGS = [
    (86, 250, 0.0, 14.0, 1),
    (180, 236, 1.5, 19.0, 1),   #1차 필드: Wolf 5마리 전투용으로 확장
    (255, 262, 2.5, 13.0, 1),   #2차 필드: StickMan 1명
    (325, 246, 7.0, 13.0, 1),
    (226, 291, 2.6, 6.0, 1),
    (384, 252, 5.5, 25.0, 2),
]
#곁가지 (스태미나 오브 공간으로 이어지는 좁은 길)
BRANCH = [(224, 268, 2.0, 3.0), (226, 291, 2.6, 3.0)]

GAMEPLAY = {
    'player_start': {'x': 80, 'y': 250, 'yaw': -15},
    #모닥불: 시작 지점 + 보스 직전 휴식 지점(공터 C, 길 동쪽 가장자리)
    'bonfires': [{'x': 89, 'y': 258}, {'x': 322, 'y': 254}],
    'beer': {'x': 88, 'y': 242},
    'stamina_orb': {'x': 227, 'y': 293},
    'boss': {'x': 390, 'y': 252, 'yaw': 180},
    #Wolf 5마리 무리: 1차 필드(공터 A, 반경 19m) 북쪽 절반, 남서쪽 진입로를 바라봄
    'wolves': [
        {'x': 186, 'y': 230, 'yaw': 195}, {'x': 189, 'y': 237, 'yaw': 193},
        {'x': 186, 'y': 244, 'yaw': 190}, {'x': 194, 'y': 232, 'yaw': 196},
        {'x': 194, 'y': 241, 'yaw': 192},
    ],
    #StickMan 1명: 2차 필드(공터 B) 북쪽, 남서쪽 진입로를 바라봄
    'stickmen': [
        {'x': 260, 'y': 262, 'yaw': 170},
    ],
    #랜드마크: 보스룸 너머 능선의 거대 수목 (시작 지점에서 수관 위로 보이는 목적지 표식)
    'landmarks': [
        {'mesh': K + 'Trees/HillTree_Tall_02/HillTree_Tall_02', 'x': 414, 'y': 262, 'scale': 2.4, 'yaw': 40},
        {'mesh': K + 'Trees/ScotsPineTall_01/ScotsPineTall_01', 'x': 418, 'y': 240, 'scale': 2.1, 'yaw': 200},
    ],
}

#값 노이즈 (해시 격자 + 스무스스텝 보간)
def _hash(ix, iy, s):
    h = (ix * 374761393 + iy * 668265263 + s * 2147483647) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0

def vnoise(x, y, s):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = _hash(ix, iy, s), _hash(ix + 1, iy, s)
    c, d = _hash(ix, iy + 1, s), _hash(ix + 1, iy + 1, s)
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy

def fbm(x, y, s, octaves=4, scale=60.0):
    v, amp, tot, f = 0.0, 1.0, 0.0, 1.0 / scale
    for o in range(octaves):
        v += amp * vnoise(x * f, y * f, s + o * 17)
        tot += amp
        amp *= 0.5
        f *= 2.0
    return v / tot * 2.0 - 1.0

def smooth(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3 - 2 * t)

def idx(ix, iy):
    return iy * N + ix

#1) 바닥 영역 래스터화: 바닥 높이, 종류, 중심선 거리 비율
INF = 1e9
floor_h = [0.0] * (N * N)
floor_kind = [-1] * (N * N)
center_ratio = [INF] * (N * N)   #중심선 거리 / 반폭 (길 흙 레이어용)

def raster_segment(a, b, kind):
    x0, y0, h0, w0 = a
    x1, y1, h1, w1 = b
    pad = max(w0, w1) + 1
    dx, dy = x1 - x0, y1 - y0
    L2 = dx * dx + dy * dy
    for ix in range(max(0, int(min(x0, x1) - pad)), min(N, int(max(x0, x1) + pad) + 1)):
        for iy in range(max(0, int(min(y0, y1) - pad)), min(N, int(max(y0, y1) + pad) + 1)):
            t = min(max(((ix - x0) * dx + (iy - y0) * dy) / L2, 0.0), 1.0)
            px, py = x0 + dx * t, y0 + dy * t
            dist = math.hypot(ix - px, iy - py)
            w = w0 + (w1 - w0) * t
            if dist < w:
                k = idx(ix, iy)
                r = dist / w
                if r < center_ratio[k]:
                    center_ratio[k] = r
                    if floor_kind[k] != 2:
                        floor_h[k] = h0 + (h1 - h0) * t
                        floor_kind[k] = kind

for a, b in zip(ROUTE, ROUTE[1:]):
    raster_segment(a, b, 0)
raster_segment(BRANCH[0], BRANCH[1], 0)

for cx, cy, ch, cr, kind in CLEARINGS:
    for ix in range(max(0, int(cx - cr - 1)), min(N, int(cx + cr + 2))):
        for iy in range(max(0, int(cy - cr - 1)), min(N, int(cy + cr + 2))):
            dist = math.hypot(ix - cx, iy - cy)
            if dist < cr:
                k = idx(ix, iy)
                #공터는 가장자리로 갈수록 살짝 올라가는 접시형
                h = ch + 0.6 * (dist / cr) ** 2
                if floor_kind[k] == -1 or kind == 2 or dist / cr < 0.85:
                    floor_h[k] = h if floor_kind[k] == -1 or kind == 2 else min(floor_h[k], h)
                    floor_kind[k] = max(floor_kind[k], kind)

#2) 챔퍼 거리 변환: 바닥 밖 거리와 가장 가까운 바닥의 높이·종류 전파
dist = [0.0 if floor_kind[k] >= 0 else INF for k in range(N * N)]
near_h = floor_h[:]
near_kind = floor_kind[:]
D1, D2 = 1.0, math.sqrt(2.0)
fwd = [(-1, 0, D1), (0, -1, D1), (-1, -1, D2), (1, -1, D2)]
bwd = [(1, 0, D1), (0, 1, D1), (1, 1, D2), (-1, 1, D2)]
for passes in range(2):
    for iy in range(N):
        for ix in range(N):
            k = idx(ix, iy)
            for ox, oy, w in fwd:
                jx, jy = ix + ox, iy + oy
                if 0 <= jx < N and 0 <= jy < N:
                    j = idx(jx, jy)
                    if dist[j] + w < dist[k]:
                        dist[k] = dist[j] + w
                        near_h[k] = near_h[j]
                        near_kind[k] = near_kind[j]
    for iy in range(N - 1, -1, -1):
        for ix in range(N - 1, -1, -1):
            k = idx(ix, iy)
            for ox, oy, w in bwd:
                jx, jy = ix + ox, iy + oy
                if 0 <= jx < N and 0 <= jy < N:
                    j = idx(jx, jy)
                    if dist[j] + w < dist[k]:
                        dist[k] = dist[j] + w
                        near_h[k] = near_h[j]
                        near_kind[k] = near_kind[j]

#가장 가까운 바닥 높이는 바닥 구간 경계(보로노이)에서 단차가 생기므로 넓게 블러한 값과 섞음
def box_blur(src, r):
    out = src[:]
    for _ in range(3):
        tmp = [0.0] * (N * N)
        for iy in range(N):
            row = [out[idx(ix, iy)] for ix in range(N)]
            acc = sum(row[0:r + 1]) + row[0] * r
            for ix in range(N):
                tmp[idx(ix, iy)] = acc / (2 * r + 1)
                acc += row[min(ix + r + 1, N - 1)] - row[max(ix - r, 0)]
        for ix in range(N):
            col = [tmp[idx(ix, iy)] for iy in range(N)]
            acc = sum(col[0:r + 1]) + col[0] * r
            for iy in range(N):
                out[idx(ix, iy)] = acc / (2 * r + 1)
                acc += col[min(iy + r + 1, N - 1)] - col[max(iy - r, 0)]
    return out

near_h_soft = box_blur(near_h, 18)
#보스룸 사면 비중도 블러 (종류별 사면 프로필 경계 단차 방지)
arena_w = box_blur([1.0 if kd == 2 else 0.0 for kd in near_kind], 14)

#3) 높이 합성
height = [0.0] * (N * N)
for iy in range(N):
    for ix in range(N):
        k = idx(ix, iy)
        d = dist[k]
        blend = smooth(0.0, 12.0, d)
        base = near_h[k] * (1 - blend) + near_h_soft[k] * blend
        #보스룸: 10m 안에 9m 솟는 가파른 벽 + 바깥 완만 상승, 일반: 완만한 숲 사면
        rise_arena = 9.0 * smooth(0, 10, d) + 7.0 * smooth(10, 45, d)
        rise_path = 11.0 * smooth(0, 28, d) + 8.0 * smooth(28, 90, d)
        aw = arena_w[k] if near_kind[k] == 2 or d > 0 else 0.0
        rise = rise_path + (rise_arena - rise_path) * aw
        edge = min(ix, iy, N - 1 - ix, N - 1 - iy)
        rise += 20.0 * smooth(55, 0, edge)
        n_amp = 0.12 + 3.2 * smooth(0, 35, d)
        n = fbm(ix, iy, 11) * n_amp + fbm(ix, iy, 29, 3, 14.0) * 0.25 * smooth(0, 6, d)
        height[k] = base + rise + n

#바닥 가장자리 이음 완화 (3x3 평균 2회, 바닥 내부 포함)
for _ in range(2):
    src = height[:]
    for iy in range(1, N - 1):
        for ix in range(1, N - 1):
            s = 0.0
            for oy in (-1, 0, 1):
                for ox in (-1, 0, 1):
                    s += src[idx(ix + ox, iy + oy)]
            height[idx(ix, iy)] = s / 9.0

def h_at(x, y):
    #월드 미터 좌표의 이중선형 높이
    x = min(max(x, 0.0), N - 1.001)
    y = min(max(y, 0.0), N - 1.001)
    ix, iy = int(x), int(y)
    fx, fy = x - ix, y - iy
    a, b = height[idx(ix, iy)], height[idx(ix + 1, iy)]
    c, d = height[idx(ix, iy + 1)], height[idx(ix + 1, iy + 1)]
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy

def grad_at(ix, iy):
    ix0, ix1 = max(ix - 1, 0), min(ix + 1, N - 1)
    iy0, iy1 = max(iy - 1, 0), min(iy + 1, N - 1)
    gx = (height[idx(ix1, iy)] - height[idx(ix0, iy)]) / (ix1 - ix0)
    gy = (height[idx(ix, iy1)] - height[idx(ix, iy0)]) / (iy1 - iy0)
    return gx, gy

def dist_at(x, y):
    return dist[idx(min(max(int(round(x)), 0), N - 1), min(max(int(round(y)), 0), N - 1))]

#4) 폴리지 산포 (가변 반경 다트 스로잉 + 공간 해시)
class Scatter:
    def __init__(self, cell):
        self.cell = cell
        self.grid = {}

    def ok(self, x, y, r):
        cx, cy = int(x // self.cell), int(y // self.cell)
        reach = int(math.ceil(r / self.cell))
        for gx in range(cx - reach, cx + reach + 1):
            for gy in range(cy - reach, cy + reach + 1):
                for px, py, pr in self.grid.get((gx, gy), ()):
                    if (px - x) ** 2 + (py - y) ** 2 < max(r, pr) ** 2:
                        return False
        return True

    def clear_of(self, x, y, r):
        #기존 점의 반경은 무시하고 고정 거리 r만 검사 (나무 줄기 옆 소품 배치용)
        cx, cy = int(x // self.cell), int(y // self.cell)
        reach = int(math.ceil(r / self.cell))
        for gx in range(cx - reach, cx + reach + 1):
            for gy in range(cy - reach, cy + reach + 1):
                for px, py, _ in self.grid.get((gx, gy), ()):
                    if (px - x) ** 2 + (py - y) ** 2 < r * r:
                        return False
        return True

    def add(self, x, y, r):
        self.grid.setdefault((int(x // self.cell), int(y // self.cell)), []).append((x, y, r))

TREE_MESH = {
    'HillTree_02': K + 'Trees/HillTree_02/HillTree_02',
    'HillTree_Tall_02': K + 'Trees/HillTree_Tall_02/HillTree_Tall_02',
    'ScotsPine_01': K + 'Trees/ScotsPine_01/ScotsPine_01',
    'ScotsPineTall_01': K + 'Trees/ScotsPineTall_01/ScotsPineTall_01',
}
PROP_MESH = {
    'BogMyrtleBush_01': K + 'Foliage/BogMyrtleBush_01/BogMyrtleBush_01',
    'BogMyrtleBush_02': K + 'Foliage/BogMyrtleBush_02/BogMyrtleBush_02',
    'Fern_01': K + 'Foliage/Ferns/SM_Fern_01',
    'Fern_03': K + 'Foliage/Ferns/SM_Fern_03',
    'Medium_Boulder_001': K + 'Rocks/Medium_Boulder_001/Medium_Boulder_001',
    'Boulder05a': K + 'Rocks/Boulder_05a/SM_Boulder05a',
    'GroundRevealRock001': K + 'Rocks/GroundRevealRock001/SM_GroundRevealRock001',
    'GroundRevealRock002': K + 'Rocks/GroundRevealRock002/SM_GroundRevealRock002',
    'River_Rock_01': K + 'Rocks/River_Rock_01/SM_River_Rock_01',
    'Vegetation_Debris_002': K + 'Trees/Vegetation_Debris_002/SM_Vegetation_Debris_002',
    'Tree_Stump_01': K + 'Trees/Tree_Stump_01/Tree_Stump_01',
}
#종류별 크기·침하(cm)·충돌
SPEC = {
    'HillTree_02': ((0.6, 0.85), 30, True),
    'HillTree_Tall_02': ((0.65, 0.9), 30, True),
    'ScotsPine_01': ((0.6, 0.85), 30, True),
    'ScotsPineTall_01': ((0.65, 0.95), 30, True),
    'BogMyrtleBush_01': ((1.3, 2.2), 12, False),
    'BogMyrtleBush_02': ((1.3, 2.2), 12, False),
    'Fern_01': ((0.8, 1.3), 3, False),
    'Fern_03': ((0.8, 1.3), 3, False),
    'Medium_Boulder_001': ((1.0, 2.2), 40, True),
    'Boulder05a': ((1.5, 3.0), 8, True),
    'GroundRevealRock001': ((0.6, 1.2), 25, True),
    'GroundRevealRock002': ((0.6, 1.2), 25, True),
    'River_Rock_01': ((1.5, 3.0), 6, True),
    'Vegetation_Debris_002': ((1.0, 1.6), 12, True),
    'Tree_Stump_01': ((0.6, 0.9), 10, True),
}
placed = {key: [] for key in list(TREE_MESH) + list(PROP_MESH)}

def place(key, x, y, align=0.0):
    (s0, s1), sink, _ = SPEC[key]
    s = random.uniform(s0, s1)
    z = h_at(x, y) * 100.0 - sink * s
    pitch = roll = 0.0
    if align > 0:
        gx, gy = grad_at(int(round(x)), int(round(y)))
        #경사 방향으로 기울임 (X=북 기준 pitch, Y=동 기준 roll)
        pitch = -math.degrees(math.atan(gx)) * align
        roll = math.degrees(math.atan(gy)) * align
    pitch += random.uniform(-3, 3)
    roll += random.uniform(-3, 3)
    placed[key].append([round(x * 100.0, 1), round(y * 100.0, 1), round(z, 1), round(pitch, 2), round(random.uniform(0, 360), 1), round(roll, 2), round(s, 3)])

def pick(weights):
    r = random.random() * sum(w for _, w in weights)
    for k, w in weights:
        r -= w
        if r <= 0:
            return k
    return weights[-1][0]

in_arena = lambda x, y: math.hypot(x - 384, y - 252) < 27.0
near_landmark = lambda x, y: any(math.hypot(x - m['x'], y - m['y']) < 9 for m in GAMEPLAY['landmarks'])

trees = Scatter(4.0)
for m in GAMEPLAY['landmarks']:
    trees.add(m['x'], m['y'], 9.0)

#동선 주변 후보 영역 (전체 맵에서 뽑으면 좁은 띠에 거의 떨어지지 않음)
RX0 = min(p[0] for p in ROUTE) - 80
RX1 = max(p[0] for p in ROUTE) + 80
RY0 = min(p[1] for p in ROUTE) - 80
RY1 = max(p[1] for p in ROUTE) + 80
def near_sample():
    return random.uniform(max(RX0, 2), min(RX1, N - 3)), random.uniform(max(RY0, 2), min(RY1, N - 3))
def any_sample():
    return random.uniform(2, N - 3), random.uniform(2, N - 3)

#나무: 길 가장자리(수관이 넓은 나무로 머리 위를 덮음) → 숲 안쪽(고밀도) → 원경 순
for zone, (dmin, dmax, radius, tries, mix, sampler) in enumerate([
        (1.8, 8.0, 3.8, 30000, [('HillTree_Tall_02', 4), ('HillTree_02', 4), ('ScotsPine_01', 2)], near_sample),
        (8.0, 60.0, 3.8, 160000, [('HillTree_Tall_02', 35), ('ScotsPineTall_01', 35), ('ScotsPine_01', 15), ('HillTree_02', 15)], near_sample),
        #원경: 동선에서 140m 밖은 포그와 능선에 가려 보이지 않으므로 생성하지 않음 (최적화)
        (60.0, 140.0, 6.5, 80000, [('ScotsPineTall_01', 60), ('ScotsPine_01', 25), ('HillTree_Tall_02', 15)], any_sample),
]):
    for _ in range(tries):
        x, y = sampler()
        d = dist_at(x, y)
        if not (dmin <= d < dmax) or in_arena(x, y) or near_landmark(x, y):
            continue
        if trees.ok(x, y, radius):
            trees.add(x, y, radius)
            place(pick(mix), x, y)

#보스룸 액자: 가장자리 링에 큰 나무 (수관은 열어 둠)
for i in range(22):
    a = 2 * math.pi * i / 22 + random.uniform(-0.08, 0.08)
    r = random.uniform(29, 33)
    x, y = 384 + r * math.cos(a), 252 + r * math.sin(a)
    if math.hypot(x - 352, y - 252) < 9:   #입구 비움
        continue
    if trees.ok(x, y, 4.0):
        trees.add(x, y, 4.0)
        place(pick([('HillTree_Tall_02', 1), ('ScotsPineTall_01', 1)]), x, y)

#길 가장자리 덤불·고사리 (충돌 없음)
props = Scatter(2.0)
for _ in range(30000):
    x, y = near_sample()
    d = dist_at(x, y)
    if d < 0.6 or d > 9.0 or in_arena(x, y):
        continue
    r = 1.6 if d < 4 else 2.6
    if props.ok(x, y, r):
        props.add(x, y, r)
        place(pick([('BogMyrtleBush_01', 3), ('BogMyrtleBush_02', 3), ('Fern_01', 2), ('Fern_03', 2)]), x, y, align=0.5)
#숲 속 덤불·고사리 (하층 식생)
for _ in range(50000):
    x, y = near_sample()
    d = dist_at(x, y)
    if d < 9.0 or d > 70.0:
        continue
    if props.ok(x, y, 3.2) and trees.clear_of(x, y, 1.5):
        props.add(x, y, 3.2)
        place(pick([('BogMyrtleBush_01', 1), ('BogMyrtleBush_02', 1), ('Fern_01', 1), ('Fern_03', 1)]), x, y, align=0.5)

#바위·쓰러진 나무·그루터기 (충돌)
rocks = Scatter(4.0)
for key, count, dmin, dmax, radius, align in [
        ('Medium_Boulder_001', 70, 3.0, 60.0, 9.0, 0.4),
        ('Boulder05a', 60, 2.0, 60.0, 8.0, 0.6),
        ('GroundRevealRock001', 50, 1.0, 40.0, 7.0, 0.8),
        ('GroundRevealRock002', 50, 1.0, 40.0, 7.0, 0.8),
        ('River_Rock_01', 40, 1.0, 20.0, 5.0, 0.8),
        ('Vegetation_Debris_002', 60, 2.5, 50.0, 8.0, 0.9),
        ('Tree_Stump_01', 45, 2.0, 50.0, 6.0, 0.3),
]:
    n = 0
    for _ in range(count * 200):
        if n >= count:
            break
        x, y = near_sample()
        d = dist_at(x, y)
        if not (dmin <= d < dmax) or in_arena(x, y):
            continue
        if rocks.ok(x, y, radius) and trees.clear_of(x, y, 2.0):
            rocks.add(x, y, radius)
            place(key, x, y, align=align)
            n += 1

#보스룸 가장자리 바위 (순환 이동 방해하지 않게 벽 쪽에만)
for i in range(9):
    a = 2 * math.pi * i / 9 + 0.3
    x, y = 384 + 23.5 * math.cos(a), 252 + 23.5 * math.sin(a)
    if math.hypot(x - 360, y - 252) < 8:
        continue
    place('Medium_Boulder_001', x, y, align=0.5)

#5) 레이어 웨이트 (Moss 기본, ForestFloor 낙엽, Path 흙, Rock 경사)
w_path = [0.0] * (N * N)
w_rock = [0.0] * (N * N)
w_leaf = [0.0] * (N * N)
leaf_bonus = [0.0] * (N * N)
for key in TREE_MESH:
    for x, y, *_ in placed[key]:
        cx, cy = x / 100.0, y / 100.0
        for ix in range(max(0, int(cx - 4)), min(N, int(cx + 5))):
            for iy in range(max(0, int(cy - 4)), min(N, int(cy + 5))):
                v = 1.0 - smooth(1.5, 4.0, math.hypot(ix - cx, iy - cy))
                k = idx(ix, iy)
                leaf_bonus[k] = max(leaf_bonus[k], v)

for iy in range(N):
    for ix in range(N):
        k = idx(ix, iy)
        n1 = fbm(ix, iy, 53, 3, 9.0)
        cr = center_ratio[k]
        p = 0.0
        if cr < INF:
            p = 1.0 - smooth(0.25, 0.7, cr + n1 * 0.25)
            if floor_kind[k] >= 1:
                p = max(p * 0.6, smooth(0.25, 0.6, fbm(ix, iy, 71, 3, 7.0)) * 0.8)
        gx, gy = grad_at(ix, iy)
        slope = math.hypot(gx, gy)
        rk = smooth(0.62, 0.95, slope + n1 * 0.1)
        lf = 0.0
        if dist[k] > 1.5:
            lf = max(smooth(0.15, 0.55, fbm(ix, iy, 91, 4, 18.0)) * 0.7, leaf_bonus[k] * 0.55)
        w_rock[k] = rk
        w_path[k] = p * (1 - rk)
        w_leaf[k] = lf * (1 - rk) * (1 - p)

def to_bytes(ws):
    return bytes(ws)

moss_b, leaf_b, path_b, rock_b = bytearray(N * N), bytearray(N * N), bytearray(N * N), bytearray(N * N)
for k in range(N * N):
    r = int(round(w_rock[k] * 255))
    p = int(round(w_path[k] * 255))
    l = int(round(w_leaf[k] * 255))
    total = r + p + l
    if total > 255:
        sc = 255.0 / total
        r, p, l = int(r * sc), int(p * sc), int(l * sc)
    rock_b[k], path_b[k], leaf_b[k] = r, p, l
    moss_b[k] = 255 - r - p - l

#6) 출력
#높이: 32768 = 0m, 단위 = 100/128 cm (scale_z 100)
with open(OUT + '/height.r16', 'wb') as f:
    f.write(struct.pack('<%dH' % (N * N), *[min(max(int(round(32768 + h * 128.0)), 0), 65535) for h in height]))
for name, data in [('Moss', moss_b), ('ForestFloor', leaf_b), ('Path', path_b), ('Rock', rock_b)]:
    with open(OUT + '/w_%s.raw' % name, 'wb') as f:
        f.write(bytes(data))
for key, lst in placed.items():
    with open(OUT + '/foliage/%s.json' % key, 'w') as f:
        json.dump(lst, f)

#투명 벽: 바닥에서 22m 떨어진 거리 등고선을 따라 선분 (마칭 스퀘어 → 단순화)
#챔퍼 거리는 1과 √2의 합이라 정수값이 흔함, 정점이 등고선 값과 같아지지 않도록 비정수 사용
WALL_D = 22.37
#교차점은 격자 모서리(정점 쌍) 단위로 한 번만 계산해 이웃 셀과 같은 점을 공유
crossing = {}
def edge_point(pa, pb):
    key = (min(pa, pb), max(pa, pb))
    if key not in crossing:
        a, b = dist[idx(*key[0])], dist[idx(*key[1])]
        t = (WALL_D - a) / (b - a)
        crossing[key] = (key[0][0] + (key[1][0] - key[0][0]) * t, key[0][1] + (key[1][1] - key[0][1]) * t)
    return key
segs = []
for iy in range(N - 1):
    for ix in range(N - 1):
        pts = [(ix, iy), (ix + 1, iy), (ix + 1, iy + 1), (ix, iy + 1)]
        cross = []
        for e in range(4):
            pa, pb = pts[e], pts[(e + 1) % 4]
            if (dist[idx(*pa)] - WALL_D) * (dist[idx(*pb)] - WALL_D) < 0:
                cross.append(edge_point(pa, pb))
        if len(cross) == 2:
            segs.append(tuple(cross))
        elif len(cross) == 4:
            segs.append((cross[0], cross[1]))
            segs.append((cross[2], cross[3]))
#선분을 체인으로 연결 (점 = 모서리 키)
from collections import defaultdict
key_of = lambda p: p
adj = defaultdict(list)
for i, (a, b) in enumerate(segs):
    adj[a].append(i)
    adj[b].append(i)
used = [False] * len(segs)
chains = []
for i in range(len(segs)):
    if used[i]:
        continue
    used[i] = True
    chain = [segs[i][0], segs[i][1]]
    for end in (1, 0):
        while True:
            p = chain[-1] if end == 1 else chain[0]
            nxt = None
            for j in adj[key_of(p)]:
                if not used[j]:
                    nxt = j
                    break
            if nxt is None:
                break
            used[nxt] = True
            a, b = segs[nxt]
            q = b if key_of(a) == key_of(p) else a
            if end == 1:
                chain.append(q)
            else:
                chain.insert(0, q)
    chains.append(chain)
walls = []
for chain in chains:
    if len(chain) < 10:
        continue
    chain = [crossing[k] for k in chain]
    #약 12m 간격으로 끊어 벽 선분 생성
    acc, start = 0.0, chain[0]
    prev = chain[0]
    for p in chain[1:]:
        acc += math.hypot(p[0] - prev[0], p[1] - prev[1])
        prev = p
        if acc >= 12.0 or p is chain[-1]:
            mx, my = (start[0] + p[0]) / 2, (start[1] + p[1]) / 2
            L = math.hypot(p[0] - start[0], p[1] - start[1])
            if L > 1.0:
                walls.append({'x': round(mx * 100), 'y': round(my * 100), 'z': round(h_at(mx, my) * 100),
                              'yaw': round(math.degrees(math.atan2(p[1] - start[1], p[0] - start[0])), 2), 'len': round(L * 100 + 150)})
            start, acc = p, 0.0

gp = json.loads(json.dumps(GAMEPLAY))
def to_world(o):
    o['z'] = round(h_at(o['x'], o['y']) * 100)
    o['x'] = round(o['x'] * 100)
    o['y'] = round(o['y'] * 100)
for k in ('player_start', 'beer', 'stamina_orb', 'boss'):
    to_world(gp[k])
for o in gp['wolves'] + gp['stickmen'] + gp['bonfires'] + gp['landmarks']:
    to_world(o)
xs = [p[0] for p in ROUTE] + [c[0] for c in CLEARINGS]
ys = [p[1] for p in ROUTE] + [c[1] for c in CLEARINGS]
gp['navmesh'] = {'min': [min(xs) - 40, min(ys) - 40, -20], 'max': [max(xs) + 40, max(ys) + 40, max(height) + 10]}
gp['walls'] = walls
with open(OUT + '/gameplay.json', 'w') as f:
    json.dump(gp, f, indent=1)

#7) 미리보기 PNG (위=북, 오른쪽=동)
S = 2
W = H = N * S // 1
hmin, hmax = min(height), max(height)
img = [[(0, 0, 0)] * N for _ in range(N)]
for iy in range(N):
    for ix in range(N):
        k = idx(ix, iy)
        gx, gy = grad_at(ix, iy)
        shade = max(0.35, min(1.0, 0.75 - gx * 0.9 + gy * 0.5))
        col = (60, 140, 50)
        if path_b[k] > 120:
            col = (150, 115, 70)
        elif rock_b[k] > 120:
            col = (125, 125, 120)
        elif leaf_b[k] > 120:
            col = (110, 120, 45)
        img[N - 1 - ix][iy] = tuple(int(c * shade) for c in col)
def dot(x, y, r, c):
    px, py = int(y), N - 1 - int(x)
    for yy in range(py - r, py + r + 1):
        for xx in range(px - r, px + r + 1):
            if 0 <= xx < N and 0 <= yy < N and (xx - px) ** 2 + (yy - py) ** 2 <= r * r:
                img[yy][xx] = c
for key in TREE_MESH:
    for x, y, *_ in placed[key]:
        dot(x / 100, y / 100, 1, (20, 60, 20))
for key in ('Medium_Boulder_001', 'Boulder05a', 'GroundRevealRock001', 'GroundRevealRock002', 'River_Rock_01'):
    for x, y, *_ in placed[key]:
        dot(x / 100, y / 100, 1, (200, 200, 200))
for w in walls:
    a = math.radians(w['yaw'])
    for t in range(-int(w['len'] / 200), int(w['len'] / 200) + 1):
        dot(w['x'] / 100 + math.cos(a) * t, w['y'] / 100 + math.sin(a) * t, 0, (230, 30, 30))
for o in gp['wolves']:
    dot(o['x'] / 100, o['y'] / 100, 2, (255, 140, 0))
dot(gp['boss']['x'] / 100, gp['boss']['y'] / 100, 4, (220, 0, 200))
dot(gp['player_start']['x'] / 100, gp['player_start']['y'] / 100, 3, (0, 120, 255))
dot(gp['stamina_orb']['x'] / 100, gp['stamina_orb']['y'] / 100, 2, (0, 255, 255))
for m in gp['landmarks']:
    dot(m['x'] / 100, m['y'] / 100, 3, (255, 255, 0))
raw = b''.join(b'\x00' + bytes(v for p in row for v in p) for row in img)
def chunk(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', N, N, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
open(os.path.join(HERE, 'preview.png'), 'wb').write(png)

print('height range', round(hmin, 1), round(hmax, 1))
print('walls', len(walls), 'chains', len(chains))
print({k: len(v) for k, v in placed.items()})
print('gameplay z', {k: gp[k]['z'] for k in ('player_start', 'boss')}, [w['z'] for w in gp['wolves']])

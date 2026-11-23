#캡처 결과 JSON(base64 PNG)을 PNG로 저장
import base64, json, sys, re

src, dst = sys.argv[1], sys.argv[2]
s = open(src, encoding='utf-8').read()
m = re.search(r'"data":"([A-Za-z0-9+/=]+)"', s)
open(dst, 'wb').write(base64.b64decode(m.group(1)))
print(dst)

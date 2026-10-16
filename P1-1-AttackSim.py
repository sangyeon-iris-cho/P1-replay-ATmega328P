배경 지식: 왜 Python으로 공격하나?

Arduino의 시리얼 포트는 PC에서 보면 그냥 COM 포트(Windows) 또는 /dev/ttyUSB0(Linux/Mac)예요. Python의 pyserial 라이브러리로 이 포트에 문자열을 보내면 Arduino가 명령으로 읽어요.

bash
# Python 환경 준비
pip install pyserial pandas
python
# attack_simulator.py
import serial, time, csv

PORT   = 'COM3'        # Windows: COM3, Mac/Linux: /dev/ttyUSB0
BAUD   = 115200
OUTFILE = 'results.csv'

results = []

with serial.Serial(PORT, BAUD, timeout=1) as p:
    time.sleep(2)  # Arduino 리셋 대기

    # ── 정상 시퀀스 (SEQ 1~10) ─────────────
    for i in range(1, 11):
        msg = f"CMD:OPEN:SEQ:{i}\n".encode()
        t_send = time.perf_counter()
        p.write(msg)
        resp = p.readline().decode().strip()
        t_recv = time.perf_counter()
        rtt = (t_recv - t_send) * 1000  # ms
        results.append({'seq': i, 'type': 'normal', 'resp': resp, 'rtt_ms': rtt})
        print(f"SEQ {i:3d} → {resp}  ({rtt:.2f}ms)")

    # ── 재생 공격: seq=5 재전송 ───────────
    for replay_seq in [5, 3, 1]:
        msg = f"CMD:OPEN:SEQ:{replay_seq}\n".encode()
        p.write(msg)
        resp = p.readline().decode().strip()
        results.append({'seq': replay_seq, 'type': 'replay', 'resp': resp, 'rtt_ms': 0})
        print(f"REPLAY {replay_seq} → {resp}")  # 기대값: REJECTED

# CSV 저장
with open(OUTFILE, 'w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=results[0].keys())
    writer.writeheader()
    writer.writerows(results)

print(f"\n결과 저장: {OUTFILE}")

기대 출력:

SEQ   1 → SEQ:1:ACCEPTED  (3.21ms)
SEQ   2 → SEQ:2:ACCEPTED  (3.18ms)
...
SEQ  10 → SEQ:10:ACCEPTED (3.22ms)
REPLAY 5 → SERIAL|SEQ:5|LAST:10|REJECTED
REPLAY 3 → SERIAL|SEQ:3|LAST:10|REJECTED
REPLAY 1 → SERIAL|SEQ:1|LAST:10|REJECTED


# analyze_results.py
import pandas as pd

df = pd.read_csv('results.csv')

normal  = df[df.type == 'normal']
replays = df[df.type == 'replay']

print("=== 정상 명령 ===")
print(f"수락률: {(normal.resp.str.contains('ACCEPTED').sum() / len(normal) * 100):.0f}%")
print(f"평균 RTT: {normal.rtt_ms.mean():.2f}ms")

print("\n=== 재생 공격 ===")
rejected = replays.resp.str.contains('REJECTED').sum()
print(f"차단률: {rejected}/{len(replays)} = {rejected/len(replays)*100:.0f}%")

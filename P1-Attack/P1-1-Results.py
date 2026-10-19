
# analyze_results
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

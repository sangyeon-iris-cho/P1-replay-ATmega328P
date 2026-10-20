
# analyze_results
import pandas as pd

df = pd.read_csv('results.csv')

normal  = df[df.type == 'normal']
replays = df[df.type == 'replay']
wrongpw = df[df.type == 'wrongpassword']
brutef = df[df.type == 'bruteforce']

print("----- Normal Command -----")
print(f"Acceptance: {(normal.resp.str.contains('ACCEPTED').sum() / len(normal) * 100):.0f}%")
print(f"Avg RTT: {normal.rtt_ms.mean():.2f}ms")

print("\n----- Replay Attack -----")
rejected = replays.resp.str.contains('REJECTED').sum()
print(f"Rejected Rate: {rejected}/{len(replays)} = {rejected/len(replays)*100:.0f}%")

print("\n----- Wrong Password -----")
rejected = wrongpw.resp.str.contains('REJECTED').sum()
print(f"Rejected Rate: {rejected}/{len(wrongpw)} = {rejected/len(wrongpw)*100:.0f}%")

print("\n----- Brute Force, Wrong Password Repeated -----")
lockedout = brutef.resp.str.contains('REJECTED-LOCKEDOUT').sum()
print(f"Locked Out Rate: {lockedout}/{len(brutef)} = {lockedout/len(brutef)*100:.0f}%")

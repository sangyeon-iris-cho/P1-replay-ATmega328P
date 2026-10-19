import serial # installed pyserial through cmd admin prompt
import time
import csv #to store result
from datetime import datetime

PORT   = 'COM3' #ur port
BAUD   = 115200
PASS = "123456789"
TIMEOUT = 3 #응답 대기 시간 second
FILENAME = 'results.csv'

results = []

def connect(port, baud):
    ser = serial.Serial(PORT, BAUD, timeout = TIMEOUT)
    time.sleep(2) # let the arduino sleep for a reset
    ser.flushInput()
    print(f"successfully connected!: {port} @ {baud} baud \n")
    return ser
    #connect with serial

def send_receive_c(ser, cmd):
    ser.write((cmd + "\n").encode())
    time.sleep(0.5) #send commands

    response = ""
    while ser.in_waiting:
        response += ser.readline().decode(errors = "ignore")

    return response.strip() #receive response

def parse_proc(response):
    for line in response.splitlines():
        if "T_proc" in line:
            try:
                return float(line.split(":")[1].replace("us", "").strip())
            except:
                pass

    return None


def save(results, filename):
    with open(filename, "w", newline = "") as f:
        writer = csv.DictWriter(f, fieldnames = ["test", "input", "result", "T_proc_us"]) 
            # writer = csv.DictWriter(f, fieldnames=results[0].keys())
        writer.writeheader()
        writer.writerows(results)
    print(f"\nSaved! @ {filename}") # CSV ssave
    

#--------------------------------------Simulator---------------------------------------------------
def sim_normal(ser, seq_start = 0, count = 5): #normal sequence from 0 to 4
    print("-" * 50)
    print("Sim1: Normal Login Test")
    results = []
    for i in range(count):
        seq = seq_start + i
        cmd = f"{PASS}:{seq}"
        print(f"\nSent: {cmd}")
        resp = send_command(ser, cmd)
        print(f"\rReceived: {resp}\n")

        t = parse_tproc(resp)
        results.append({"Simulator1": "normal_login", "input": cmd,
                        "result": "ACCEPTED" if "ACCEPTED" in resp else "REJECTED",
                        "T_proc_us": t})
        time.sleep(1)

    return results

    #    msg = f"CMD:OPEN:SEQ:{i}\n".encode()
     #   t_send = time.perf_counter()
      #  p.write(msg)
   #     resp = p.readline().decode().strip()
    #    t_recv = time.perf_counter()
     #   rtt = (t_recv - t_send) * 1000  # ms
      #  print(f"SEQ {i:3d} → {resp}  ({rtt:.2f}ms)")

def sim_RA(ser, seq_RA):
    print("-" * 50)
    print("Simulator2: Replay Attack")
    results = []

    for attempt in range(5):
        cmd = f"{PASS}:{seq_RA}"
        print(f"\nReplay Attack of {attempt+1}: {cmd}")
        resp = send_command(ser,cmd)
        print(f"\nResponse: {resp}\n")

        t = parse_proc(resp)
        results.append({"Simulator2": "Replay Attack", "input" = cmd,
                       "result": "REPLAY ATTACK DETECTED" if "REPLAY" in resp else ("ACCEPTED" if "ACCEPTED" in resp else "REJECTED"),
                       "T_proc_us": t})
        time.sleep(1)

    return results

#for replay_seq in [5, 3, 1]:
 #       msg = f"CMD:OPEN:SEQ:{replay_seq}\n".encode()
  #      p.write(msg)
   #     resp = p.readline().decode().strip()
    #    results.append({'seq': replay_seq, 'type': 'replay', 'resp': resp, 'rtt_ms': 0})
     #   print(f"REPLAY {replay_seq} → {resp}")  # 기대값: REJECTED

def t_wrong_input(ser):
    print("-" *50)
    print("Sim3: Wrong PASSWORD Input\n")
    results = []

    malformed = [] #이렇게 정해진 잘못된 비밀번호 말고 전체적으로 음수면 -1말고도 -2, -10 도 잘못 되었다고 받아들이는 그런 코드였으면 좋겠어
      malformed = [
        f"{PASSWORD}:-1",       # 음수
        f"{PASSWORD}:abc",      # 문자
        f"{PASSWORD}:",         # seq 없음
        f"{PASSWORD}:99999",    # 범위 초과
        "wrongpass:0",          # 틀린 비번
        f"{PASSWORD}",          # 콜론 없음
    ]

    for cmd in malformed:
        print(f"  전송: {cmd}")
        resp = send_command(ser, cmd)
        print(f"  응답: {resp}\n")
        t = parse_tproc(resp)
        results.append({
            "test": "malformed",
            "input": cmd,
            "result": resp.splitlines()[0] if resp else "NO_RESPONSE",
            "T_proc_us": t
        })
        time.sleep(1)

    return results

# ══════════════════════════════════════════════════════════
# 실험 4: Brute Force (5번 실패 → lockout 확인)
# ══════════════════════════════════════════════════════════
def test_brute_force(ser):
    print("=" * 50)
    print("실험 4: Brute Force → Lockout 테스트")
    print("=" * 50)
    results = []

    for i in range(7):   # 5번 넘겨서 lockout 확인
        cmd = f"wrongpassword:{i}"
        print(f"  시도 {i+1}: {cmd}")
        resp = send_command(ser, cmd)
        print(f"  응답: {resp}\n")
        results.append({
            "test": "brute_force",
            "input": cmd,
            "result": "LOCKED" if "LOCK" in resp else "REJECTED",
            "T_proc_us": None
        })
        time.sleep(1)

    return results

# ══════════════════════════════════════════════════════════
# 메인 실행
# ══════════════════════════════════════════════════════════
if __name__ == "__main__":
    ser = connect(PORT, BAUD)
    all_results = []

    try:
        # 실험 1: 정상 로그인 seq 0~4
        all_results += test_normal_login(ser, start_seq=0, count=5)

        # 실험 2: seq 0 재전송 (이미 사용됨)
        all_results += test_replay_attack(ser, captured_seq=0)

        # 실험 3: 이상한 입력
        all_results += test_malformed_input(ser)

        # 실험 4: Brute force
        all_results += test_brute_force(ser)

    finally:
        ser.close()
        print("[연결 종료]")

    # 결과 저장
    fname = f"attack_results_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    save_csv(all_results, fname)



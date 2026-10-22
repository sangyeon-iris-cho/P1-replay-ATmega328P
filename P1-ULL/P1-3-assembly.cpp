import csv
import re
import sys
import time
from datetime import datetime

import serial  # pip install pyserial

PORT = 'COM3'
BAUD = 115200
PASS = "123456789"
TIMEOUT = 4      # 응답 최대 대기 시간(초)
QUIET = 0.8      # 이 시간 동안 추가 수신이 없으면 응답이 끝났다고 판단(초)
MODE = None      # handshake()에서 설정됨: "baseline" / "hardened"


class HandshakeError(Exception):
    pass


# ------------------------------------------------ 시리얼 통신 ------------------------------------------------
def connect(port, baud):
    try:
        ser = serial.Serial(port, baud, timeout=TIMEOUT)
    except serial.SerialException as e:
        print(f"NO_RESPONSE: 포트를 열 수 없습니다 ({port}): {e}")
        sys.exit(1)
    time.sleep(2)  # 포트를 열면 아두이노가 자동 리셋되므로 부팅 대기
    print(f"successfully connected!: {port} @ {baud} baud\n")
    return ser


def send_cmd(ser, cmd):
    """명령 전송 후, 응답이 끝날 때까지(QUIET초 동안 추가 수신 없음) 또는 TIMEOUT까지 수신"""
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    ser.flush()

    buf = ""
    start = time.time()
    last_rx = None
    while time.time() - start < TIMEOUT:
        n = ser.in_waiting
        if n:
            buf += ser.read(n).decode(errors="ignore")
            last_rx = time.time()
        else:
            if last_rx is not None and time.time() - last_rx >= QUIET:
                break
            time.sleep(0.02)
    return buf.strip()


def reset_board(ser):
    """DTR 토글로 아두이노 하드 리셋 (Uno/Nano/Mega 등 auto-reset 회로가 있는 보드)"""
    print("\n[RESET] DTR toggle")
    ser.dtr = False
    time.sleep(0.1)
    ser.dtr = True
    ser.reset_input_buffer()  # 리셋 직전의 쓰레기 데이터 제거 (부팅 메시지는 이후에 도착)


def query_seq(ser):
    resp = send_cmd(ser, "SEQ?")
    for line in resp.splitlines():
        line = line.strip()
        if line.startswith("SEQ:"):
            try:
                return int(line.split(":", 1)[1].strip())
            except ValueError:
                pass
    return None


def handshake(ser, window=5):
    global MODE
    mode = None
    seq = None
    deadline = time.time() + window

    while time.time() < deadline:
        if ser.in_waiting:
            line = ser.readline().decode(errors="ignore").strip()
            if line.startswith("MODE:"):
                mode = line.split(":", 1)[1].strip()
            elif line.startswith("The Next Sequence Number is:"):
                try:
                    seq = int(line.split(":", 1)[1].strip())
                except ValueError:
                    pass
            if mode is not None and (mode == "baseline" or seq is not None):
                break
        else:
            time.sleep(0.05)

    if mode is None:
        raise HandshakeError("MODE ERROR - 아두이노로부터 MODE 응답이 없습니다")

    MODE = mode
    if mode == "baseline":
        seq = 0
    else:
        queried = query_seq(ser)
        if queried is not None:
            seq = queried
        if seq is None:
            raise HandshakeError("SEQ ERROR - 시퀀스 번호를 확인할 수 없습니다")

    print("HANDSHAKE RESULT")
    print(f"current MODE is: {MODE}")
    print(f"current sequence number is: {seq}\n")
    return MODE, seq


def reset_and_handshake(ser):
    reset_board(ser)
    return handshake(ser)


# ------------------------------------------------ 응답 해석 ------------------------------------------------
def finalpass(pw, seq):
    if MODE == "baseline":
        return pw
    return f"{pw}:{seq}"


def parse_time(resp, key):
    """'T_proc: 123 micro seconds' / 'T_act: 123 micro seconds' 에서 숫자만 추출"""
    m = re.search(rf"{key}\s*:\s*([\d.]+)\s*micro\s*seconds", resp, re.I)
    return float(m.group(1)) if m else None


def classify(resp):
    if not resp:
        return "NO_RESPONSE"
    u = resp.upper()
    if "LOCK" in u:
        return "LOCKED_OUT"
    if "REPLAY" in u:
        return "REPLAY_DETECTED"
    if "FORMAT IS" in u:
        return "REJECTED_FORMAT"
    if "ACCEPTED" in u:
        return "ACCEPTED"
    if "WRONG PASSWORD" in u:
        return "REJECTED_WRONG_PW"
    return "UNKNOWN"


def extract_detail(resp):
    parts = []
    m = re.search(r"(\d+)\s+chances?\s+left", resp, re.I)
    if m:
        parts.append(f"chances_left={m.group(1)}")
    m = re.search(r"Expected Sequence Number was:\s*(-?\d+).*?current Sequence Number is:\s*(-?\d+)",
                  resp, re.I | re.S)
    if m:
        parts.append(f"expected_seq={m.group(1)}")
        parts.append(f"got_seq={m.group(2)}")
    m = re.search(r"SEQ number accepted for this input:\s*(-?\d+)", resp, re.I)
    if m:
        parts.append(f"seq_accepted={m.group(1)}")
    return ";".join(parts)


def record(results, test, step, cmd, resp, extra=""):
    result = classify(resp)
    detail = ";".join(p for p in (extra, extract_detail(resp)) if p)
    results.append({
        "test": test,
        "step": step,
        "input": cmd,
        "result": result,
        "detail": detail,
        "T_proc_us": parse_time(resp, "T_proc"),
        "T_act_us": parse_time(resp, "T_act"),
    })
    return result


def save(results, filename):
    with open(filename, "w", newline="", encoding="utf-8-sig") as f:
        writer = csv.DictWriter(
            f, fieldnames=["test", "step", "input", "result", "detail", "T_proc_us", "T_act_us"])
        writer.writeheader()
        writer.writerows(results)
    print(f"\nSaved! @ {filename}")


# ------------------------------------------------ Simulator ------------------------------------------------
def sim_normal(ser, seq_start=0, count=5):
    print("-" * 50)
    print("Sim1: Normal Login Test")
    results = []
    for i in range(count):
        cmd = finalpass(PASS, seq_start + i)
        print(f"\nSent: {cmd}")
        resp = send_cmd(ser, cmd)
        result = record(results, "normal_login", i + 1, cmd, resp)
        print(f"Received: {resp}\n=> {result}")
        time.sleep(1)
    return results


def sim_RA(ser, seq_RA):
    print("-" * 50)
    print("Sim2: Replay Attack")
    results = []
    cmd = finalpass(PASS, seq_RA)  # sim1에서 이미 사용한 명령을 그대로 재전송

    for attempt in range(5):
        print(f"\n{attempt + 1} Attempts: {cmd}")
        resp = send_cmd(ser, cmd)
        result = record(results, "replay_attack", attempt + 1, cmd, resp)
        print(f"Response: {resp}\n=> {result}")
        time.sleep(1)
    return results


def sim_wrong_input(ser, seq):
    print("-" * 50)
    print("Sim3: Wrong PASSWORD Input")
    results = []

    if MODE == "baseline":
        cases = [
            ("string_pw", "wrongpass"),
            ("wrong_pw", "987654321"),
            ("empty_pw", ""),
            ("overflow_pw", "1" * 50),
        ]
    else:
        cases = [
            ("neg_seq", f"{PASS}:-1"),
            ("string_seq", f"{PASS}:abc"),
            ("empty_seq", f"{PASS}:"),
            ("overflow_seq", f"{PASS}:5000000000"),
            ("wrong_pw_right_seq", f"987654321:{seq}"),
            ("no_seq", PASS),
            ("no_colon", f"{PASS}{seq}"),
        ]

    for step, (label, cmd) in enumerate(cases, start=1):
        print(f"\nInput [{label}]: {cmd!r}")
        resp = send_cmd(ser, cmd)
        result = record(results, "wrong_password_input", step, cmd, resp, extra=f"case={label}")
        print(f"Response: {resp}\n=> {result}")
        time.sleep(1)

    # 잘못된 입력이 시퀀스 번호를 소모했는지 확인 (리셋 전에 조회)
    if MODE != "baseline":
        after = query_seq(ser)
        results.append({
            "test": "wrong_password_input", "step": len(cases) + 1, "input": "SEQ?",
            "result": "SEQ_CHECK", "detail": f"seq_before={seq};seq_after={after}",
            "T_proc_us": None, "T_act_us": None,
        })
        print(f"\nSEQ check: before={seq}, after={after}")

    return results


def sim_bruteforce(ser, seq):
    print("-" * 50)
    print("Sim4: Brute Force")
    results = []

    for i in range(11):  # MAX_ATTEMPTS(10) 초과까지 시도
        cmd = f"987654321:{seq}" if MODE == "hardened" else "987654321"
        print(f"\n{i + 1} Attempts: {cmd}")
        resp = send_cmd(ser, cmd)
        result = record(results, "brute_force", i + 1, cmd, resp)
        print(f"Response: {resp}\n=> {result}")
        time.sleep(1)
    return results


# ------------------------------------------------ main ------------------------------------------------
if __name__ == "__main__":
    ser = connect(PORT, BAUD)
    all_results = []
    failed = False

    try:
        MODE, seq = reset_and_handshake(ser)          # 시작 시 깨끗한 상태 + 핸드셰이크

        all_results += sim_normal(ser, seq_start=seq, count=5)   # Sim1
        all_results += sim_RA(ser, seq_RA=seq)                   # Sim2

        MODE, seq = reset_and_handshake(ser)          # Sim3 전 리셋 (seq 재확인)
        all_results += sim_wrong_input(ser, seq=seq)  # Sim3
        MODE, seq = reset_and_handshake(ser)          # Sim3 후 리셋 (시도 횟수 초기화)

        all_results += sim_bruteforce(ser, seq=seq)   # Sim4

    except HandshakeError as e:
        print(f"\nNO_RESPONSE: 핸드셰이크 실패 -> 프로그램을 종료합니다.\n({e})")
        failed = True
    finally:
        ser.close()
        print("-" * 50)
        print("FINISHED")

    if all_results:  # 핸드셰이크 실패로 중단되어도 그때까지의 결과는 저장
        fname = f"Results_{MODE}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        save(all_results, fname)

    if failed:
        sys.exit(1)

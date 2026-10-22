import serial # installed pyserial through cmd admin prompt
import time
import csv #to store result
import sys #for exiting the system when not connected
import re
from datetime import datetime # import datetime class from the library datetime

PORT   = 'COM5' #ur port
BAUD   = 115200
PASS = "123456789"
TIMEOUT = 4 #응답 대기 시간 second
FILENAME = 'results.csv'
SEQ = 0

def connect(port, baud):
    try:
        ser = serial.Serial(port, baud, timeout = TIMEOUT)
        time.sleep(2) # let the arduino sleep for a reset
        print(f"successfully connected!: {port} @ {baud} baud \n")
        return ser
    except serial.SerialException as e:
        print(f"CONNECTION ERROR: {e}")
        sys.exit(1)
    #connect with serial

def send_cmd(ser, cmd):
    ser.write((cmd + "\n").encode())
    time.sleep(TIMEOUT) #send commands

    response = ""
    while ser.in_waiting:
        response += ser.readline().decode(errors = "ignore")

    return response.strip() #receive response

def handshake(ser):
    global MODE
    MODE = None
    mode = None
    seq = None
    MODE = "baseline" #default
    dead = time.time() + 10
    
    while time.time() < dead:
        line = ser.readline().decode(errors = "ignore").strip()
        if not line: continue

        print(f"HANDSHAKE: {line}")    
        if line.startswith("MODE:"):
            mode = line.split(":", 1)[1].strip().lower()
            MODE = mode
            if mode == "baseline":
                        seq = 0
                        return mode, seq
            if mode != "baseline" and seq is not None:
                        return mode, seq

        match = re.search(r"The Next Sequence Number is: \s*(\d+)", line, re.IGNORECASE)
        if match:
            seq = int(match.group(1))

        if mode != "baseline" and seq is not None:
            print("HANDSHAKE RESULT\n")
            print(f"current MODE is: {MODE} \rcurrent sequence number is: {seq}\n")
            return mode, seq

    if mode is None:
        raise RuntimeError("Handshake with Arduino was not successful - MODE ERROR")
    elif mode != "baseline" and seq is None:
        raise RuntimeError("Handshake with Arduino was not successful - SEQ ERROR")
    raise RuntimeError("Exception Error")

def finalpass(pw, seq):
    if MODE == "baseline":
        return pw
    else:
        return f"{pw}:{seq}"

def parse_proc(response):
    for line in response.splitlines():
        if "T_proc" in line:
            try:
                return float(line.split(":")[1].replace("micro seconds", "").strip())
            except:
                pass
    return None

def classify(resp):
    if not resp:
        return "NO_RESPONCE"

    u = resp.upper()
    if "LOCK" in u: return "LOCKEDOUT"
    if "REPLAY" in u: return "REPLAY-ATTACK!"
    if "FORMAT IS" in u: return "WRONG_FORMAT"
    if "WRONG PASSWORD" in u or "REJECTED" in u:
        return "REJECTED"
    if "FUTURE SEQ" in u: return "FUTURE_SEQ"
    if "EXCEPTION ERROR" in u: return "EXCEPTION_ERROR"
    if "ACCEPTED" in u: return "ACCEPTED"
    if "" in u: return "BLANK"

def reset(ser):
    print("\n\nRESETTING ARDUINO")
    ser.reset_input_buffer() #remove the rest of the previous simulator results, inputs
    ser.dtr = False
    time.sleep(0.1)
    ser.dtr = True #reset the arduino by toggling the DTR
    
    time.sleep(2) #for booting and preparing for the initialization

    mode, seq = handshake(ser)
    print(f"RESET COMPLETED! MODE: {mode}, SEQ: {seq}")
    return mode, seq

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
    seq = seq_start
    
    for i in range(count):
        seq = seq_start + i
        cmd = finalpass(PASS, seq)
        print(f"\n{i+1} Attempt")
        print(f"\nSent: {cmd}")
        resp = send_cmd(ser, cmd)
        print(f"\rReceived: {resp}\n")

        t = parse_proc(resp)
        results.append({"test": "normal_login", "input": cmd,
                        "result": classify(resp), "T_proc_us": t})

        if classify(resp) == "ACCEPTED":
            if seq == 65535:
                print("WARNING: Sequence number reached to Max")
                break
            seq = (seq + 1) % 65536 #for the next sequence number, only when this message is valid
        else:
            print("WARNING: Sequence number is unknown.")
            break
        time.sleep(1)

    return results, seq 
    
def sim_RA(ser, seq_RA):
    print("-" * 50)
    print("Simulator2: Replay Attack")
    results = []

    cmd = finalpass(PASS, seq_RA)

    for attempt in range(5):
        print(f"\n{attempt+1} Attempts: {cmd}") #attempt starts from 0
        resp = send_cmd(ser, cmd)

        print(f"\rResponse: {resp}\n")
        t = parse_proc(resp)
        results.append({"test": "Replay_Attack", "input": cmd,
                        "result": classify(resp), "T_proc_us": t})
        time.sleep(1)
        
    return results

def sim_wrong_input(ser, seq):
    print("-" *50)
    print("Sim3: Wrong PASSWORD Input\n")
    results = []

    if MODE == "baseline":
        wrongpw = [ #basic wrong password options
            "wrongpass",            # what if a string / character array was received instead of 123456789(number type)?
            "987654321",            # wrong password
            "",                     # what if NULL was received as a password
            "1" * 50               # overflow of password
        ]
    else:
        wrongpw = [
            f"{PASS}:-1",       # what if a negative number was received for seq?
            f"{PASS}:abc",      # what if a string / character array was received?
            f"{PASS}:",         # what if NULL was received?
            f"{PASS}:5000000000",    # overflow of seq
            f"987654321:{seq}",      # wrong password with the right sequence number
            PASS,               # no seq
            f"{PASS}{seq}"      #no colon
        ]

    for cmd in wrongpw:
        print(f"\nInput: {cmd}")
        resp = send_cmd(ser, cmd)
        print(f"\rResponse: {resp}\n")
        
        t = parse_proc(resp)
        results.append({
            "test": "wrong_password_input",
            "input": cmd,
            "result": classify(resp), "T_proc_us": t})
        time.sleep(1)
        
    return results

def sim_bruteforce(ser, seq):
    print("-" * 50)
    print("Sim 4: Brute Force")
    results = []

    if MODE == "hardened":
        for i in range(11):  
            cmd = f"987654321:{seq}" # tries wrong password for several times
            print(f"\n{i+1} Attempts: {cmd}")
            resp = send_cmd(ser, cmd)
            print(f"\nResponse: {resp}\n")
            results.append({
                "test": "brute_force",
                "input": cmd,
                "result": classify(resp), "T_proc_us": None}) #locked out, no signals to arduino
            time.sleep(1)
            
    else: #baseline
        for i in range(11):  
            cmd = "987654321"
            print(f"\n{i+1} Attempts: {cmd}")
            resp = send_cmd(ser, cmd)
            print(f"\nResponse: {resp}\n")
            results.append({
                "test": "brute_force",
                "input": cmd,
                "result": classify(resp), "T_proc_us": parse_proc(resp)
            })
            time.sleep(1)

    return results



#--------------------------------------------main function-----------------------------------------------
if __name__ == "__main__":
    ser = connect(PORT, BAUD)
    all_results = []

    try:
        MODE, seq = handshake(ser)
# Simulator 1: Normal 5 Signals
        r1, SeqNext = sim_normal(ser, seq_start = seq, count = 5)
        all_results += r1
#Simulator 2: Replay Attack
        all_results += sim_RA(ser, seq_RA = seq) #seq was already used for the very previous command

        MODE, seq = reset(ser)
# Simulator 3: Wrong Password input
        all_results += sim_wrong_input(ser, seq = SeqNext)

        
        MODE, seq = reset(ser)
#Simulator 4: Brute Force
        all_results += sim_bruteforce(ser, seq = SeqNext)

    except(RuntimeError, serial.SerialException) as e:
        print(f"Exception Error: {e}")

    finally:
        ser.close()
        print("-" * 50)
        print("FINISHED")
        
    fname = f"Results_{MODE}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    save(all_results, fname) #save all of those results in a file
 

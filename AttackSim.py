import serial # installed pyserial through cmd admin prompt
import time
import csv #to store result
from datetime import datetime # import datetime class from the library datetime

PORT   = 'COM3' #ur port
BAUD   = 115200
PASS = "123456789"
TIMEOUT = 3 #응답 대기 시간 second
FILENAME = 'results.csv'
MODE = "baseline" #default
SEQ = 0

def connect(port, baud):
    ser = serial.Serial(port, baud, timeout = TIMEOUT)
    time.sleep(2) # let the arduino sleep for a reset
    ser.flushInput()
    print(f"successfully connected!: {port} @ {baud} baud \n")
    print(f"MODE: {MODE}\n\n")
    return ser
    #connect with serial

def send_cmd(ser, cmd):
    ser.write((cmd + "\n").encode())
    time.sleep(0.5) #send commands

    response = ""
    while ser.in_waiting:
        response += ser.readline().decode(errors = "ignore")

    return response.strip() #receive response

def handshake(ser):
    dead = time.time() + 3
    while time.time() < dead:
        if ser.in_waiting:
            line = ser.readline().decode(errors = "ignore").strip()

            if line.startswith("MODE:"):
                mode = line.split(":")[1].strip()
                MODE = mode

            if line.startswith("Next SEQ:"):
                seq = int(line.split(":")[1].strip())

    if mode != "baseline":
        ser.write(b"SEQ?\n") #send a query "SEQ?" in a byte mode for the serial to interprete
        time.sleep(0.5)
            
        while ser.in_waiting:
            line = ser.readline().decode(errors = "ignore").strip()
            if line.startswith("SEQ:"):
                seq = int(line.split(":")[1].strip())
                    
    print(f"HANDSHAKE RESULT: current sequence number is: {seq}\n")
    return seq

def finalpass(pw, seq):
    if MODE == "baseline":
        return pw
    else:
        return f"{pw}:{seq}"

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
        cmd = finalpass(PASS, seq)
        print(f"\nSent: {cmd}")
        resp = send_cmd(ser, cmd)
        print(f"\rReceived: {resp}\n")

        t = parse_proc(resp)
        results.append({"test": "normal_login", "input": cmd,
                        "result": "ACCEPTED" if "ACCEPTED" in resp else "REJECTED",
                        "T_proc_us": t})
        time.sleep(1)

    return results, seq_start + count #for the next sequence number

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
                           "result": "REPLAY ATTACK DETECTED" if "REPLAY" in resp #hardened result
                                        else ("ACCEPTED" if "ACCEPTED" in resp else "REJECTED"), #baseline result
                           "T_proc_us": t})
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
            f"{PASS}:99999",    # overflow of seq
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
            "result": resp.splitlines()[0] if resp else "NO_RESPONSE",
            "T_proc_us": t
        })
        time.sleep(1)
        
    return results

def sim_bruteforce(ser, seq):
    print("-" * 50)
    print("Sim 4: Brute Force")
    results = []

    if MODE == "hardened":
        for i in range(7):  
            cmd = f"987654321:{seq + i}" # tries wrong password for several times
            print(f"\n{i+1} Attempts: {cmd}")
            resp = send_command(ser, cmd)
            print(f"\nResponse: {resp}\n")
            results.append({
                "test": "brute_force",
                "input": cmd,
                "result": "REJECTED-LOCKEDOUT" if "LOCK" in resp else "REJECTED",
                "T_proc_us": None #locked out, no signals to arduino
            })
            time.sleep(1)
    else: #baseline
        for i in range(7):  
            cmd = "123456789"
            print(f"\n{i+1} Attempts: {cmd}")
            resp = send_command(ser, cmd)
            print(f"\nResponse: {resp}\n")
            results.append({
                "test": "brute_force",
                "input": cmd,
                "result": "ALLOWED",
                "T_proc_us": parse_proc(resp) #locked out, no signals to arduino
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
# Simulator 3: Wrong Password input
        all_results += t_wrong_input(ser, seq = SeqNext)
#Simulator 4: Brute Force
        all_results += t_bruteforce(ser, seq = SeqNext)

    finally:
        ser.close()
        print("-" * 50)
        print("FINISHED")
        
    fname = f"Results_{MODE}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    save(all_results, fname) #save all of those results in a file



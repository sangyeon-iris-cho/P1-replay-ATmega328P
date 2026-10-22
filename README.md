[Results_baseline_1.csv](https://github.com/user-attachments/files/33831912/Results_baseline_1.csv)# P1-replay-ATmega328P

Resource constrained Embedded security project, focusing on defensing replay-attacks, by firmware level mitigation, comparing the real-time performance between different modules.

project report will be done
results:
P1-1-Attack
base:

<img width="288" height="171" alt="baseline-by hand-1" src="https://github.com/user-attachments/assets/18f963fc-e6f6-442f-9226-15174fc75115" />
<img width="416" height="363" alt="baseline-by hand-2" src="https://github.com/user-attachments/assets/d1bf40e8-58ff-4e46-88d6-ac6bf18ada95" />
 
  Simulator
  
  1. [Uploading Rtest,input,result,T_proc_us
normal_login,123456789,ACCEPTED,0.81
normal_login,123456789,ACCEPTED,0.81
normal_login,123456789,ACCEPTED,0.75
normal_login,123456789,ACCEPTED,0.81
normal_login,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.81
Replay_Attack,123456789,ACCEPTED,0.81
Replay_Attack,123456789,ACCEPTED,0.81
Replay_Attack,123456789,ACCEPTED,0.81
wrong_password_input,wrongpass,REJECTED,0.19
wrong_password_input,987654321,REJECTED,0.19
wrong_password_input,,NO_RESPONCE,
wrong_password_input,1.11111E+49,,
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
esults_baseline_1.csv…]()
  2. [Results_baseline_2.csv](https://github.com/user-attachments/files/33831915/Results_baseline_2.csv)
test,input,result,T_proc_us
normal_login,123456789,ACCEPTED,0.75
normal_login,123456789,ACCEPTED,0.81
normal_login,123456789,ACCEPTED,0.75
normal_login,123456789,ACCEPTED,0.81
normal_login,123456789,ACCEPTED,0.81
Replay_Attack,123456789,ACCEPTED,0.81
Replay_Attack,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.75
wrong_password_input,wrongpass,REJECTED,0.19
wrong_password_input,987654321,REJECTED,0.19
wrong_password_input,,NO_RESPONCE,
wrong_password_input,1.11111E+49,BLANK,
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.25
  3. [Results_baseline_3.csv](https://github.com/user-attachments/files/33831916/Results_baseline_3.csv)
test,input,result,T_proc_us
normal_login,123456789,ACCEPTED,0.75
normal_login,123456789,ACCEPTED,0.81
normal_login,123456789,ACCEPTED,0.81
normal_login,123456789,ACCEPTED,0.75
normal_login,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.75
Replay_Attack,123456789,ACCEPTED,0.75
wrong_password_input,wrongpass,REJECTED,0.19
wrong_password_input,987654321,REJECTED,0.25
wrong_password_input,,NO_RESPONCE,
wrong_password_input,1.11111E+49,BLANK,
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.25
brute_force,987654321,REJECTED,0.19
brute_force,987654321,REJECTED,0.19




hard:

<img width="351" height="433" alt="hard-by hand-4" src="https://github.com/user-attachments/assets/fffc7cad-58db-4f7b-a55f-532541bc8725" />
<img width="342" height="420" alt="hard-by hand-3" src="https://github.com/user-attachments/assets/a741f559-a4ca-4826-ae8b-20e40d9ca42f" />
<img width="579" height="395" alt="hard-by hand-2" src="https://github.com/user-attachments/assets/8f5ae766-1c87-4cb2-866a-e5ac33693f6e" />
<img width="342" height="440" alt="hard-by hand-1" src="https://github.com/user-attachments/assets/44211a1c-67c4-4f58-8ee2-86a32dbaca00" />
<img width="233" height="214" alt="hard-by hand-5" src="https://github.com/user-attachments/assets/8b8e1e7b-13fc-4f33-bb99-af323cef9921" />

  Simulator
  1. [Results_hard_1.csv](https://github.com/user-attachments/files/33831920/Results_hard_1.csv)test,input,result,T_proc_us
normal_login,123456789:5,ACCEPTED,0.81
normal_login,123456789:6,ACCEPTED,0.81
normal_login,123456789:7,ACCEPTED,0.81
normal_login,123456789:8,ACCEPTED,0.81
normal_login,123456789:9,ACCEPTED,0.81
Replay_Attack,123456789:5,REPLAY-ATTACK!,
Replay_Attack,123456789:5,REPLAY-ATTACK!,
Replay_Attack,123456789:5,REPLAY-ATTACK!,
Replay_Attack,123456789:5,REPLAY-ATTACK!,
Replay_Attack,123456789:5,REPLAY-ATTACK!,
wrong_password_input,123456789:-1,WRONG_FORMAT,
wrong_password_input,123456789:abc,WRONG_FORMAT,
wrong_password_input,123456789:,WRONG_FORMAT,
wrong_password_input,123456789:5000000000,WRONG_FORMAT,
wrong_password_input,987654321:10,REJECTED,
wrong_password_input,123456789,WRONG_FORMAT,
wrong_password_input,12345678910,WRONG_FORMAT,
brute_force,987654321:10,REJECTED,
brute_force,987654321:10,REJECTED,
brute_force,987654321:10,REJECTED,
brute_force,987654321:10,REJECTED,
brute_force,987654321:10,REJECTED,
brute_force,987654321:10,REJECTED,
brute_force,987654321:10,REJECTED,
brute_force,987654321:10,REJECTED,
brute_force,987654321:10,REJECTED,
brute_force,987654321:10,LOCKEDOUT,
brute_force,987654321:10,LOCKEDOUT,
  2. [Results_hard_2.csv](https://github.com/user-attachments/files/33831922/Results_hard_2.csv)test,input,result,T_proc_us
normal_login,123456789:10,ACCEPTED,0.81
normal_login,123456789:11,ACCEPTED,0.81
normal_login,123456789:12,ACCEPTED,0.81
normal_login,123456789:13,ACCEPTED,0.81
normal_login,123456789:14,ACCEPTED,0.81
Replay_Attack,123456789:10,REPLAY-ATTACK!,
Replay_Attack,123456789:10,REPLAY-ATTACK!,
Replay_Attack,123456789:10,REPLAY-ATTACK!,
Replay_Attack,123456789:10,REPLAY-ATTACK!,
Replay_Attack,123456789:10,REPLAY-ATTACK!,
wrong_password_input,123456789:-1,WRONG_FORMAT,
wrong_password_input,123456789:abc,WRONG_FORMAT,
wrong_password_input,123456789:,WRONG_FORMAT,
wrong_password_input,123456789:5000000000,WRONG_FORMAT,
wrong_password_input,987654321:15,REJECTED,
wrong_password_input,123456789,WRONG_FORMAT,
wrong_password_input,12345678915,WRONG_FORMAT,
brute_force,987654321:15,REJECTED,
brute_force,987654321:15,REJECTED,
brute_force,987654321:15,REJECTED,
brute_force,987654321:15,REJECTED,
brute_force,987654321:15,REJECTED,
brute_force,987654321:15,REJECTED,
brute_force,987654321:15,REJECTED,
brute_force,987654321:15,REJECTED,
brute_force,987654321:15,REJECTED,
brute_force,987654321:15,LOCKEDOUT,
brute_force,987654321:15,LOCKEDOUT,
  3. [Results_hard_3.csv](https://github.com/user-attachments/files/33831924/Results_hard_3.csv)
test,input,result,T_proc_us
normal_login,123456789:15,ACCEPTED,0.81
normal_login,123456789:16,ACCEPTED,0.81
normal_login,123456789:17,ACCEPTED,0.81
normal_login,123456789:18,ACCEPTED,0.81
normal_login,123456789:19,ACCEPTED,0.81
Replay_Attack,123456789:15,REPLAY-ATTACK!,
Replay_Attack,123456789:15,REPLAY-ATTACK!,
Replay_Attack,123456789:15,REPLAY-ATTACK!,
Replay_Attack,123456789:15,REPLAY-ATTACK!,
Replay_Attack,123456789:15,REPLAY-ATTACK!,
wrong_password_input,123456789:-1,WRONG_FORMAT,
wrong_password_input,123456789:abc,WRONG_FORMAT,
wrong_password_input,123456789:,WRONG_FORMAT,
wrong_password_input,123456789:5000000000,WRONG_FORMAT,
wrong_password_input,987654321:20,REJECTED,
wrong_password_input,123456789,WRONG_FORMAT,
wrong_password_input,12345678920,WRONG_FORMAT,
brute_force,987654321:20,REJECTED,
brute_force,987654321:20,REJECTED,
brute_force,987654321:20,REJECTED,
brute_force,987654321:20,REJECTED,
brute_force,987654321:20,REJECTED,
brute_force,987654321:20,REJECTED,
brute_force,987654321:20,REJECTED,
brute_force,987654321:20,REJECTED,
brute_force,987654321:20,REJECTED,
brute_force,987654321:20,LOCKEDOUT,
brute_force,987654321:20,LOCKEDOUT,





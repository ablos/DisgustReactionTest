import serial
import serial.tools.list_ports
import csv
from datetime import datetime
import sounddevice as sd
import soundfile as sf
import threading

# Load all sounds at startup
countdown_data, countdown_sr = sf.read('countdown.wav')
sounds = {
    'countdown': sf.read('countdown.wav'),
    'final_result': sf.read('final_result.wav'),
    'wrong': sf.read('wrong.wav'),
    'success': sf.read('success.wav')
}

def play_sound(name):
    data, sr = sounds[name]
    threading.Thread(target=lambda: sd.play(data, sr), daemon=True).start()

# List available ports
ports = serial.tools.list_ports.comports()
print("Available ports:")
for i, port in enumerate(ports):
    print(f"{i}: {port.device} - {port.description}")
    
# User port selection
port_index = int(input("Select port number: "))
selected_port = ports[port_index].device

# Connect
ser = serial.Serial(selected_port, 115200, timeout=1)
print(f"Connected to {selected_port}")
print(f"Connect ESP32 now or press the reset button to start capture!")

# Data storage
trial_data = []
final_results = None

# Read lines
try:
    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8').strip()
            parts = line.split(',')
            
            if parts[0] == "ready":
                print()
                print(f"Test ready for {parts[1]} trials!")
                
            elif parts[0] == "start":
                print()
                print("Starting test in 3 seconds!")
                play_sound('countdown')
                
            elif parts[0] == "end":
                play_sound('final_result')
                print()
                print("Test finished!")
                print()
                print("Results:")
                print(f"Normal average: {float(parts[1]) / 1000.0} ms")
                print(f"Disgust average: {float(parts[2]) / 1000.0} ms")
                print(f"Total average: {float(parts[3]) / 1000.0} ms")
                print()
                print(f"Earlies on normal: {parts[4]}")
                print(f"Earlies on disgust: {parts[5]}")
                print()
                print(f"Wrongs on normal: {parts[6]}")
                print(f"Wrongs on disgust: {parts[7]}")
                
                final_results = {
                    'normal_avg': int(parts[1]),
                    'disgust_avg': int(parts[2]),
                    'total_avg': int(parts[3]),
                    'normal_early': int(parts[4]),
                    'disgust_early': int(parts[5]),
                    'normal_wrong': int(parts[6]),
                    'disgust_wrong': int(parts[7])
                }
                
                # Prompt to save results
                save = input("\nSave results? (y/n): ")
                if save.lower() == 'y':
                    participant_id = input("Enter participant ID: ")
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    
                    # Save trial data
                    with open(f"{participant_id}_{timestamp}_trials.csv", 'w', newline='') as f:
                        writer = csv.DictWriter(f, fieldnames=['trial', 'condition', 'result', 'reaction_time'])
                        writer.writeheader()
                        writer.writerows(trial_data)
                    
                    # Save summary
                    with open(f"{participant_id}_{timestamp}_summary.csv", 'w', newline='') as f:
                        writer = csv.DictWriter(f, fieldnames=final_results.keys())
                        writer.writeheader()
                        writer.writerow(final_results)
                    
                    print(f"\nSaved to {participant_id}_{timestamp}_trials.csv and {participant_id}_{timestamp}_summary.csv")
                        
                # Clear data for next test
                trial_data = []
                final_results = None
                print("\nReady for next participant. Press reset on ESP32 to start.")
                        
            elif parts[0] == "early":
                print()
                print("Whoops, that was too early! Resetting trial...")
                play_sound('wrong')
                
            elif parts[0] == "test":
                trial_data.append({
                    'trial': int(parts[1]),
                    'condition': parts[2],
                    'result': parts[3],
                    'reaction_time': int(parts[4])
                })
                
                reactionInMs = float(parts[4]) / 1000.0
                
                if parts[3] == "wrong":
                    print()
                    print(f"Whoops! Participant hit the wrong fruit in {reactionInMs} ms")
                    play_sound('wrong')
                    
                else:
                    print()
                    print(f"Results of trial {parts[1]}:")
                    print(f"Category: {parts[2]}")
                    print(f"Reaction time: {reactionInMs} ms")
                    play_sound('success')
            
except KeyboardInterrupt:
    print()
    print("\nClosing connection...")
    ser.close()
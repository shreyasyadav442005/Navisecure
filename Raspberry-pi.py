#!/usr/bin/env python3
"""
NAVISECURE: Raspberry Pi Face Verification System
Description: Performs automated face verification inside the vehicle cabin.
             Upon verification, it signals authorization status to the 
             Arduino Digital Controller via USB Serial.
Dependencies: pip install opencv-python face_recognition pyserial
Hardware: Raspberry Pi 4/5, USB Camera (facing driver's seat)
Install Opencv Library and related.
"""

import cv2
import face_recognition
import serial
import time
import os

# --- HARDWARE CONFIGURATION ---
SERIAL_PORT = '/dev/ttyACM0'  # Default Arduino USB Serial Port on Pi
BAUD_RATE = 9600
CAMERA_INDEX = 0             # Core cabin camera index
KNOWN_DRIVER_IMG = "driver_template.jpg"  # Path to registered driver template

# Initialize serial connection with Arduino with auto-retry
def init_serial():
    print("[INFO] Initializing serial link to Arduino Digital Controller...")
    while True:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            time.sleep(2)  # Allow Arduino connection reset
            ser.reset_input_buffer()
            print(f"[SUCCESS] Serial link established on {SERIAL_PORT}")
            return ser
        except serial.SerialException:
            print(f"[ERROR] Arduino not found on {SERIAL_PORT}. Retrying in 3 seconds...")
            time.sleep(3)

def load_known_face():
    if not os.path.exists(KNOWN_DRIVER_IMG):
        print(f"[FATAL] Driver template picture '{KNOWN_DRIVER_IMG}' not found!")
        print("[INSTRUCTION] Please place a portrait image of the driver named 'driver_template.jpg' in this directory.")
        return None
    
    print("[INFO] Encoding registered driver template profile...")
    try:
        driver_image = face_recognition.load_image_file(KNOWN_DRIVER_IMG)
        driver_face_encoding = face_recognition.face_encodings(driver_image)[0]
        print("[SUCCESS] Driver profile encoded successfully.")
        return driver_face_encoding
    except IndexError:
        print("[FATAL] Could not locate or decode a face in the template image!")
        return None

def main():
    driver_encoding = load_known_face()
    if driver_encoding is None:
        return

    arduino_serial = init_serial()
    video_capture = cv2.VideoCapture(CAMERA_INDEX)
    
    if not video_capture.isOpened():
        print("[FATAL] Video capture device could not be opened.")
        return

    print("[INFO] Navisecure cabin authentication system is running and idle...")
    
    last_trigger_check = 0
    verification_active = False

    try:
        while True:
            # Check for standard trigger commands from Arduino controller
            if arduino_serial.in_waiting > 0:
                raw_command = arduino_serial.readline().decode('utf-8', errors='ignore').strip()
                if raw_command == "TRIGGER_AUTH":
                    print("[EVENT] Received AUTH request from Digital Controller.")
                    verification_active = True
                    last_trigger_check = time.time()

            if verification_active:
                # Limit verification window to 30 seconds to prevent battery drain
                if time.time() - last_trigger_check > 30:
                    print("[TIMEOUT] Authentication attempt timed out.")
                    arduino_serial.write(b"AUTH_FAILED\n")
                    verification_active = False
                    continue

                ret, frame = video_capture.read()
                if not ret:
                    continue

                # Resize frame to 1/4 for fast processing
                small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)
                # Convert BGR (OpenCV standard) to RGB (Face_recognition standard)
                rgb_small_frame = cv2.cvtColor(small_frame, cv2.COLOR_BGR2RGB)

                face_locations = face_recognition.face_locations(rgb_small_frame)
                face_encodings = face_recognition.face_encodings(rgb_small_frame, face_locations)

                for current_encoding in face_encodings:
                    # Check if face is a match with a strict tolerance rating of 0.5 (low FAR)
                    match = face_recognition.compare_faces([driver_encoding], current_encoding, tolerance=0.5)
                    
                    if match[0]:
                        print("[SUCCESS] Driver identity successfully matched!")
                        arduino_serial.write(b"AUTH_SUCCESS\n")
                        verification_active = False
                        break
                
                # Render monitoring output (only if desktop display is attached)
                cv2.imshow('Navisecure Verification Camera Feed', frame)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            else:
                # System is idle; sleep to conserve CPU overhead
                time.sleep(0.5)
                cv2.destroyAllWindows()

    except KeyboardInterrupt:
        print("\n[INFO] Terminating system execution.")
    finally:
        video_capture.release()
        cv2.destroyAllWindows()
        arduino_serial.close()

if __name__ == "__main__":
    main()

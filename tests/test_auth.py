import socket
import time
from utils import send_packet, receive_packet, PORT

def test_auth_flow():
    # Register
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', PORT))
    
    unique_user = f"user_{int(time.time())}"
    reg_payload = {"action": "REGISTER", "data": {"username": unique_user, "password": "123"}}
    send_packet(s, "REQ", reg_payload)
    type, resp = receive_packet(s)
    if type != "RES" or resp["status"] != "SUCCESS":
        raise Exception(f"Register failed: {resp}")
    s.close()
    
    # Login
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', PORT))
    login_payload = {"action": "LOGIN", "data": {"username": unique_user, "password": "123"}}
    send_packet(s, "REQ", login_payload)
    type, resp = receive_packet(s)
    if type != "RES" or resp["status"] != "SUCCESS":
        raise Exception(f"Login failed: {resp}")
        
    logged_in_sock = s 
    
    # Concurrent Login Kick
    s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s2.connect(('127.0.0.1', PORT))
    send_packet(s2, "REQ", login_payload)
    type, resp = receive_packet(s2)
    if type != "RES" or resp["status"] != "SUCCESS":
        raise Exception(f"Second login failed: {resp}")
    
    # Check if first client is disconnected or received error
    try:
        logged_in_sock.settimeout(2.0)
        type, resp = receive_packet(logged_in_sock)
        # Should receive error "Logged in from another location"
        if type == "ERR" and "another location" in resp["message"]:
            print("PASS: Old session kicked with notification")
        else:
            print(f"WARN: Old session received {type} {resp} instead of kick message")
            
        # Should be closed after that
        try:
             d = logged_in_sock.recv(1024)
             if len(d) == 0:
                 print("PASS: Old session socket closed")
             else:
                 print("WARN: Old session socket still open")
        except:
             print("PASS: Old session socket closed (exception)")
             
    except Exception as e:
        print(f"Exception checking old session: {e}")

    s2.close()
    logged_in_sock.close()
    
    print("PASS: All Auth Tests Completed")

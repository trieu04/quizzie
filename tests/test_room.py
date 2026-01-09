import socket
import time
import json
from utils import send_packet, receive_packet, PORT

def test_room_flow():
    # 1. Admin Login
    print("--- Testing Admin Login ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', PORT))
    
    # Assuming 'admin' user needs to be created first or we register one with role 'admin'
    # But register only supports 'participant' by default in handle_register unless we hacked it?
    # Wait, handle_register calls storage_add_user(..., NULL). storage_add_user defaults to "participant".
    # So we need to manually create an admin user in the file or modify storage to allow us to create one.
    # For test purpose, I'll append an admin user directly to data/users.txt if not exists.
    
    try:
        with open("data/users.txt", "a") as f:
            f.write("admin:admin:admin\n")
    except:
        pass

    login_payload = {"action": "LOGIN", "data": {"username": "admin", "password": "admin"}}
    send_packet(s, "REQ", login_payload)
    type, resp = receive_packet(s)
    
    if type != "RES" or resp["status"] != "SUCCESS":
        raise Exception(f"Admin login failed: {resp}")
    
    role = resp["data"].get("role")
    if role != "admin":
        raise Exception(f"Expected role 'admin', got '{role}'")
    
    print("PASS: Admin login successful")

    # 2. Import Questions
    print("--- Testing Import Questions ---")
    questions = [
        {
            "question": "1+1=?",
            "options": ["1", "2", "3", "4"],
            "correct_index": 1
        }
    ]
    import_payload = {
        "action": "IMPORT_QUESTIONS",
        "data": {
            "bank_name": "math_101",
            "questions": questions
        }
    }
    
    send_packet(s, "REQ", import_payload)
    type, resp = receive_packet(s)
    if type != "RES" or resp["status"] != "SUCCESS":
        raise Exception(f"Import questions failed: {resp}")
    print("PASS: Import questions successful")

    # 3. List Question Banks
    print("--- Testing List Question Banks ---")
    send_packet(s, "REQ", {"action": "LIST_QUESTION_BANKS"})
    type, resp = receive_packet(s)
    if type != "RES" or resp["status"] != "SUCCESS":
        raise Exception(f"List banks failed: {resp}")
    if "math_101" not in resp["data"]:
        raise Exception(f"math_101 not in bank list: {resp['data']}")
    print("PASS: List question banks successful")

    # 4. Get Question Bank
    print("--- Testing Get Question Bank ---")
    get_bank_payload = {
        "action": "GET_QUESTION_BANK",
        "data": {"bank_id": "math_101"}
    }
    send_packet(s, "REQ", get_bank_payload)
    type, resp = receive_packet(s)
    if type != "RES" or resp["status"] != "SUCCESS":
        raise Exception(f"Get bank failed: {resp}")
    if len(resp["data"]) != 1 or resp["data"][0]["question"] != "1+1=?":
        raise Exception(f"Unexpected bank data: {resp['data']}")
    print("PASS: Get question bank successful")

    # 5. Update Question Bank
    print("--- Testing Update Question Bank ---")
    new_questions = [
        {
            "question": "2+2=?",
            "options": ["3", "4", "5", "6"],
            "correct_index": 1
        }
    ]
    update_payload = {
        "action": "UPDATE_QUESTION_BANK",
        "data": {
            "bank_id": "math_101",
            "questions": new_questions
        }
    }
    send_packet(s, "REQ", update_payload)
    type, resp = receive_packet(s)
    # Expecting success message
    if type == "ERR": 
         raise Exception(f"Update bank failed: {resp}")
    # Note: server.c sends SUCCESS packet via send_success which uses MSG_TYPE_RES
    if type != "RES": # or check message content
         raise Exception(f"Update bank unexpected response: {type} {resp}")
    print("PASS: Update question bank successful")

    # 6. Delete Question Bank
    print("--- Testing Delete Question Bank ---")
    # First creating a dummy bank to delete so we don't break subsequent tests that might use math_101 (though here we use it for room)
    # Actually, let's delete math_101 AFTER creating the room, or create a temporary one.
    # Let's create 'temp_del' bank.
    
    questions_del = [{"question": "Q?", "options": ["A","B"], "correct_index": 0}]
    import_payload = {
        "action": "IMPORT_QUESTIONS",
        "data": {"bank_name": "temp_del", "questions": questions_del}
    }
    send_packet(s, "REQ", import_payload)
    receive_packet(s) # Consume response

    req = {
        "action": "DELETE_QUESTION_BANK",
        "data": {"bank_id": "temp_del"}
    }
    send_packet(s, "REQ", req)
    rtype, rdata = receive_packet(s)
    if rtype == "RES" and rdata["status"] == "SUCCESS":
        print("PASS: Delete question bank successful")
    else:
        print(f"FAIL: Delete question bank failed: {rdata.get('message')}")
        sys.exit(1)

    # 7. Create Enhanced Room
    print("--- Testing Create Enhanced Room ---")
    create_payload = {
        "action": "CREATE_ROOM",
        "data": {
            "room_name": "Advanced Math",
            "question_bank_id": "math_101",
            "start_time": int(time.time()),
            "end_time": int(time.time()) + 3600,
            "num_questions": 5,
            "allowed_attempts": 3
        }
    }
    send_packet(s, "REQ", create_payload)
    type, resp = receive_packet(s)
    if type != "RES" or resp["status"] != "SUCCESS":
        raise Exception(f"Create room failed: {resp}")
    print("PASS: Create enhanced room successful")

    # 7. List Rooms and Verify Fields
    print("--- Testing List Rooms ---")
    list_payload = {"action": "LIST_ROOMS"}
    send_packet(s, "REQ", list_payload)
    type, resp = receive_packet(s)
    if type != "RES" or resp["status"] != "SUCCESS":
        raise Exception(f"List rooms failed: {resp}")
    
    rooms = resp["data"]
    room_id = None
    found = False
    for r in rooms:
        if r["name"] == "Advanced Math":
            found = True
            room_id = r["id"]
            if r.get("num_questions") != 5 or r.get("allowed_attempts") != 3:
                 raise Exception(f"Room fields mismatch: {r}")
            break
    
    if not found or not room_id:
        raise Exception("Created room not found in list")
        
    print(f"PASS: List rooms successful, found enhanced room ID: {room_id}")
    
    # --- Test Get Room Stats ---
    print("--- Testing Get Room Stats ---")
    send_packet(s, "REQ", {
        "action": "GET_ROOM_STATS",
        "data": {
            "room_id": room_id
        }
    })
    type, resp = receive_packet(s)
    if type == "RES" and resp.get("status") == "SUCCESS":
        data = resp.get("data")
        # Structure is now data: { stats: { ... }, results: [ ... ] }
        if data and "stats" in data and "results" in data:
            stats = data["stats"]
            if "total_attempts" in stats and "average_score" in stats:
                print("PASS: Get room stats successful")
            else:
                print("FAIL: Stats missing fields")
            
            if "room" in data:
                r = data["room"]
                if r.get("id") == room_id and r.get("name") == "Advanced Math":
                    print("PASS: Room info returned correctly")
                else:
                    print(f"FAIL: Room info incorrect: {r}")
            else:
                print("FAIL: Room info missing from response")

        else:
            print("FAIL: Get room stats missing stats or results object")
    else:
        print(f"FAIL: Get room stats failed: {resp}")

    # Note: CLOSE_ROOM is removed (auto status).

    # --- Test Delete Room ---
    print("--- Testing Delete Room ---")
    send_packet(s, "REQ", {
        "action": "DELETE_ROOM",
        "data": {
            "room_id": room_id
        }
    })
    type, resp = receive_packet(s)
    if type == "RES" and resp.get("status") == "SUCCESS":
        print("PASS: Delete room successful")
    else:
        print(f"FAIL: Delete room failed: {resp}")

    # Verify Deletion by Listing
    send_packet(s, "REQ", {"action": "LIST_ROOMS"})
    type, resp = receive_packet(s)
    if type == "RES" and resp.get("status") == "SUCCESS":
        rooms = resp.get("data")
        found = False
        for r in rooms:
            if r["id"] == room_id:
                found = True
                break
        if not found:
            print("PASS: Room successfully deleted from list")
        else:
            print("FAIL: Room still in list after deletion")

    print("PASS: All Room Tests Completed")
    s.close()

if __name__ == "__main__":
    try:
        test_room_flow()
    except Exception as e:
        print(f"FAIL: {e}")
        exit(1)

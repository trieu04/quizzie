import subprocess
import time
import sys
import os
from utils import SERVER_BIN, PORT
from test_auth import test_auth_flow

def run_test_case(name, func):
    print(f"Running {name}...", end=" ")
    try:
        func()
        print("PASS")
        return True
    except Exception as e:
        print(f"FAIL: {e}")
        return False

def main():
    if not os.path.exists(SERVER_BIN):
        print(f"Error: {SERVER_BIN} not found. Run 'make' first.")
        sys.exit(1)

    print("Starting server...")
    server_process = subprocess.Popen([SERVER_BIN, str(PORT)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(1)

    if server_process.poll() is not None:
        print(f"Server failed to start. Return code: {server_process.returncode}")
        print("Output:")
        out, err = server_process.communicate()
        print("STDOUT:", out.decode('utf-8') if out else "")
        print("STDERR:", err.decode('utf-8') if err else "")
        sys.exit(1)

    success = False
    try:
        if run_test_case("Auth Flow", test_auth_flow):
            success = True
    finally:
        print("Stopping server...")
        server_process.terminate()
        server_process.wait()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()

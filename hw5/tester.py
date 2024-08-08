import subprocess
import os
import signal
import time
import threading

SERVER_EXECUTABLE = "./pcc_server"
CLIENT_EXECUTABLE = "./pcc_client"
SERVER_PORT = "8080"
SERVER_IP = "127.0.0.1"
TEST_FILES_DIR = "test_files"

def start_server():
    server_process = subprocess.Popen([SERVER_EXECUTABLE, SERVER_PORT], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return server_process

def stop_server(server_process):
    if server_process:
        server_process.terminate()
        server_process.wait()

def run_client(file_path):
    client_process = subprocess.Popen([CLIENT_EXECUTABLE, SERVER_IP, SERVER_PORT, file_path],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = client_process.communicate()
    return client_process.returncode, stdout.decode(), stderr.decode()

def create_test_files():
    os.makedirs(TEST_FILES_DIR, exist_ok=True)
    
    # Small file with only printable characters
    with open(os.path.join(TEST_FILES_DIR, "small_printable.txt"), "w") as f:
        f.write("This is a small file with printable characters 12345!")

    # File with mixed printable and non-printable characters
    with open(os.path.join(TEST_FILES_DIR, "mixed_chars.txt"), "wb") as f:
        f.write(b"This is a mixed file with printable and non-printable characters.\x00\x01\x02\x03")

    # Large file approaching 1MB
    with open(os.path.join(TEST_FILES_DIR, "large_file.txt"), "wb") as f:
        f.write(b"A" * (1024 * 1024 - 1))  # 1MB - 1 byte

    # Empty file
    open(os.path.join(TEST_FILES_DIR, "empty_file.txt"), "w").close()

    # File generated from /dev/urandom
    with open(os.path.join(TEST_FILES_DIR, "urandom_file.bin"), "wb") as f:
        subprocess.run(["head", "-c", "1M", "/dev/urandom"], stdout=f)

def test_basic_functionality(server_process):
    test_cases = ["small_printable.txt", "mixed_chars.txt", "large_file.txt"]

    for test_case in test_cases:
        file_path = os.path.join(TEST_FILES_DIR, test_case)
        return_code, stdout, stderr = run_client(file_path)
        print(f"Test {test_case}:")
        print(f"Return code: {return_code}")
        print(f"Output: {stdout}")
        print(f"Error: {stderr}\n")

def test_error_handling(server_process):
    test_cases = ["empty_file.txt", "non_existent_file.txt"]

    for test_case in test_cases:
        file_path = os.path.join(TEST_FILES_DIR, test_case)
        return_code, stdout, stderr = run_client(file_path)
        print(f"Test {test_case}:")
        print(f"Return code: {return_code}")
        print(f"Output: {stdout}")
        print(f"Error: {stderr}\n")

def test_signal_handling(server_process):
    file_path = os.path.join(TEST_FILES_DIR, "large_file.txt")

    def send_sigint():
        time.sleep(2)  # Wait for the client to start sending data
        os.kill(server_process.pid, signal.SIGINT)

    sigint_thread = threading.Thread(target=send_sigint)
    sigint_thread.start()

    return_code, stdout, stderr = run_client(file_path)
    sigint_thread.join()

    print(f"Signal Handling Test (SIGINT):")
    print(f"Return code: {return_code}")
    print(f"Output: {stdout}")
    print(f"Error: {stderr}\n")

def test_stress(server_process):
    file_path = os.path.join(TEST_FILES_DIR, "small_printable.txt")
    client_threads = []

    def run_client_thread():
        run_client(file_path)

    # Start multiple clients simultaneously
    for _ in range(10):
        client_thread = threading.Thread(target=run_client_thread)
        client_threads.append(client_thread)
        client_thread.start()

    for client_thread in client_threads:
        client_thread.join()

    print(f"Stress Test: 10 clients simultaneously\n")

def main():
    create_test_files()

    print("Starting server...")
    server_process = start_server()
    time.sleep(1)  # Give the server time to start

    try:
        print("\nRunning basic functionality tests...")
        test_basic_functionality(server_process)

        print("\nRunning error handling tests...")
        test_error_handling(server_process)

        print("\nRunning signal handling test...")
        test_signal_handling(server_process)

        print("\nRunning stress test...")
        test_stress(server_process)

    finally:
        print("\nStopping server...")
        stop_server(server_process)

if __name__ == "__main__":
    main()

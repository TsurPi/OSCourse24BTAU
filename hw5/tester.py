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

def run_client(file_path, delay=0):
    def client_thread():
        client_process = subprocess.Popen([CLIENT_EXECUTABLE, SERVER_IP, SERVER_PORT, file_path],
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = client_process.communicate()
        return client_process.returncode, stdout.decode(), stderr.decode()

    if delay > 0:
        time.sleep(delay)  # Introduce a delay to simulate slow clients

    return client_thread()

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

    # Very large file exceeding 1MB
    with open(os.path.join(TEST_FILES_DIR, "very_large_file.txt"), "wb") as f:
        f.write(b"A" * (1024 * 1024 * 2))  # 2MB

    # Empty file
    open(os.path.join(TEST_FILES_DIR, "empty_file.txt"), "w").close()

    # File generated from /dev/urandom
    with open(os.path.join(TEST_FILES_DIR, "urandom_file.bin"), "wb") as f:
        subprocess.run(["head", "-c", "1M", "/dev/urandom"], stdout=f)

def test_additional_stress(server_process):
    test_cases = [
        "small_printable.txt",
        "large_file.txt",
        "very_large_file.txt"
    ]

    client_threads = []

    def run_client_thread(file_path):
        run_client(file_path)

    # Start multiple clients with varying file sizes
    for test_case in test_cases:
        file_path = os.path.join(TEST_FILES_DIR, test_case)
        for _ in range(5):  # Launch 5 clients per file size
            client_thread = threading.Thread(target=run_client_thread, args=(file_path,))
            client_threads.append(client_thread)
            client_thread.start()

    for client_thread in client_threads:
        client_thread.join()

    print(f"Additional Stress Test: Multiple clients with varying file sizes\n")

def test_timeout_handling(server_process):
    # Run a client with artificial delay to simulate timeout
    file_path = os.path.join(TEST_FILES_DIR, "large_file.txt")
    return_code, stdout, stderr = run_client(file_path, delay=5)
    
    print(f"Timeout Handling Test:")
    print(f"Return code: {return_code}")
    print(f"Output: {stdout}")
    print(f"Error: {stderr}\n")

def test_resource_limits(server_process):
    # Simulate low-memory conditions by running multiple clients
    file_path = os.path.join(TEST_FILES_DIR, "very_large_file.txt")
    client_threads = []

    def run_client_thread(file_path):
        run_client(file_path)

    for _ in range(10):  # Launch 10 clients to stress memory
        client_thread = threading.Thread(target=run_client_thread, args=(file_path,))
        client_threads.append(client_thread)
        client_thread.start()

    for client_thread in client_threads:
        client_thread.join()

    print(f"Resource Limit Test: Simulated low-memory conditions\n")

def main():
    create_test_files()

    print("Starting server...")
    server_process = start_server()
    time.sleep(1)  # Give the server time to start

    try:
        print("\nRunning additional stress tests...")
        test_additional_stress(server_process)

        print("\nRunning timeout handling test...")
        test_timeout_handling(server_process)

        print("\nRunning resource limit test...")
        test_resource_limits(server_process)

    finally:
        print("\nStopping server...")
        stop_server(server_process)

if __name__ == "__main__":
    main()

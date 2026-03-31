import socket
import argparse

TCP_SERVER_PORT = 3999

def send_tcp_message(ip, password, command):
    """Send password + command to TCP server and receive response."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((ip, TCP_SERVER_PORT))
            print(f"Connected to {ip}:{TCP_SERVER_PORT}")

            # Combine password + command (simple protocol)
            full_message = f"{password}:{command}"

            sock.sendall(full_message.encode('utf-8'))
            print(f"Sent: {full_message}")

            response = sock.recv(1024)
            print(f"Server response: {response.decode('utf-8')}")

    except Exception as e:
        print(f"Error communicating with server: {e}")


def main():
    parser = argparse.ArgumentParser(description="TCP Client with authentication")

    parser.add_argument("ip", help="Server IP address (e.g., 192.168.1.10)")
    parser.add_argument("command", help="Command to send to the server")
    parser.add_argument("-p", "--password", required=True, help="Authentication password")

    args = parser.parse_args()

    send_tcp_message(args.ip, args.password, args.command)


if __name__ == "__main__":
    main()

import socket

def open_socket(server_ip, server_port):
    """Create and return a socket object listening on the specified server IP and port."""
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((server_ip, server_port))
    server_socket.listen(1)
    print("Server listening on {}:{}".format(server_ip, server_port))
    return server_socket

def receive_file(conn, output_file):
    """Receive a file from the connected client."""
    with open(output_file, 'wb') as f:
        # print("Receiving data...")
        total_received = 0
        while True:
            data = conn.recv(1024)
            if not data:
                break
            f.write(data)
            total_received += len(data)

    # Only print the message if total_received is greater than 0
    if total_received > 0:
        print("File received and saved as {}. Total bytes received: {}".format(output_file, total_received))
    # else:
    #     print("No file saved.")

    return total_received

def send_command(conn, command):
    """Send a command to the client."""
    conn.sendall(command.encode())
    print(f"Command '{command}' sent to the client.")

def receive_completion_status(conn):
    """Receive completion status from the client."""
    status = conn.recv(1024).decode()
    if status:
        print(f"Status received ssfrom client: '{status}'")

def close_socket(server_socket):
    """Close the server socket."""
    server_socket.close()
    print("Server socket closed.")

if __name__ == "__main__":
    server_ip = "10.138.74.186"  # Replace with the server's IP address
    # 192.168.0.180
    server_port = 55029
    server_socket = open_socket(server_ip, server_port)
    conn, addr = server_socket.accept()  # Accept the connection to send command later
    try:
        command = 'exptime'
        send_command(conn, command)
        receive_completion_status(conn)
        receive_file(conn, f'{command}_file.raw')  # Save the received file with this name

    finally:
        # Close the connection and socket
        conn.close()
        close_socket(server_socket)

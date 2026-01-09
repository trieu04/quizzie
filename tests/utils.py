import json
import struct
import socket

SERVER_BIN = "./bin/server"
PORT = 8081
HEADER_SIZE = 7

def send_packet(sock, msg_type, payload):
    json_str = json.dumps(payload)
    payload_len = len(json_str)
    total_len = HEADER_SIZE + payload_len
    header = struct.pack('!I3s', total_len, msg_type.encode('utf-8'))
    sock.sendall(header)
    sock.sendall(json_str.encode('utf-8'))

def receive_packet(sock):
    header_data = sock.recv(HEADER_SIZE)
    if len(header_data) != HEADER_SIZE:
        return None, None
    total_len, msg_type = struct.unpack('!I3s', header_data)
    msg_type = msg_type.decode('utf-8')
    payload_len = total_len - HEADER_SIZE
    payload_data = sock.recv(payload_len)
    payload = json.loads(payload_data.decode('utf-8'))
    return msg_type, payload

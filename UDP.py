import socket
import soundfile as sf
import time

UDP_IP="192.168.1.100"
UDP_PORT = 50005
Ms = 0
Ns = 430
Ns_index = 0
Ms_index = 0

print ("UDP target IP:", UDP_IP)
print ("UDP target port:", UDP_PORT)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # UDP
buffer, fs = sf.read('sv2.wav', Ns, Ms_index, None, 'int16')

while (len(buffer)) == Ns:

    Ms_index = Ms_index + Ns
    sock.sendto(buffer, (UDP_IP.encode(), UDP_PORT))
    time.sleep(0.01)
    buffer, fs = sf.read('sv2.wav', Ns, Ms_index, None, 'int16')

import socket
import soundfile as sf

UDP_IP="192.168.1.100"
UDP_PORT = 5005
Ms = 0
Ns = 4
Ns_index = 0
Ms_index = 0

print ("UDP target IP:", UDP_IP)
print ("UDP target port:", UDP_PORT)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # UDP


buffer, fs = sf.read('spring_HiFi.wav', Ns, Ms_index, None, 'int16')


while (len(buffer)) == Ns:

    for Ns_index in range (0, Ns) :
        buffer[Ns_index] = (buffer[Ns_index] + 32675) * (273/4369)
    print(buffer)
    Ms_index = Ms_index + Ns
    sock.sendto(buffer, (UDP_IP.encode(), UDP_PORT))
    buffer, fs = sf.read('spring_HiFi.wav', Ns, Ms_index, None, 'int16')

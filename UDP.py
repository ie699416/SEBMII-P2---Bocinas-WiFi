import socket
import soundfile as sf
import time

UDP_IP="192.168.1.100"
UDP_PORT = 50005
Ms = 0
Ns = 2000
Ns_index = 0
Ms_index = 0

print ("UDP target IP:", UDP_IP)
print ("UDP target port:", UDP_PORT)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # UDP
buffer, fs = sf.read('september.wav', Ns, Ms_index, None, 'int16')
bufferAux, fs = sf.read('september.wav', 500, Ms_index, None, 'int16')


while (len(buffer)) == Ns:


    for Ns_index in range (0, Ns) :
        buffer[Ns_index] = (buffer[Ns_index] + 32675) * (273/4369)


    for Ns_index in range (0, int(Ns/4)):
        bufferAux[Ns_index] = buffer[(Ns_index * 4)]


    Ms_index = Ms_index + Ns
    start = time.time()
    sock.sendto(bufferAux, (UDP_IP.encode(), UDP_PORT))
    #time.sleep(0.01)
    print(bufferAux)
    end = time.time()
    process = end - start

    buffer, fs = sf.read('september.wav', Ns, Ms_index, None, 'int16')

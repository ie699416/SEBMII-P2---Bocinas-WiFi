import socket
import soundfile as sf
import time

UDP_IP="192.168.1.100"
UDP_PORT = 50005
Ms = 0
Ns = 1740
Ns_index = 0
Ms_index = 0

print ("UDP target IP:", UDP_IP)
print ("UDP target port:", UDP_PORT)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # UDP
buffer, fs = sf.read('spring_HiFi.wav', Ns, Ms_index, None, 'int16')
bufferAux, fs = sf.read('spring_HiFi.wav', 1700, Ms_index, None, 'int16')

start = time.time()


while (len(buffer)) == Ns:


    for Ns_index in range (0, Ns) :
        buffer[Ns_index] = (buffer[Ns_index] + 32675) * (273/4369)


    #for Ns_index in range (0, int(Ns/2)):
    #    bufferAux[Ns_index] = buffer[(Ns_index * 2)]


    Ms_index = Ms_index + Ns

    #print(bufferAux)
    sock.sendto(buffer, (UDP_IP.encode(), UDP_PORT))
    end = time.time()
    process = end - start
    start = time.time()
    if process != 0:
        print(process)
    # time.sleep(0.01)

    buffer, fs = sf.read('spring_HiFi.wav', Ns, Ms_index, None, 'int16')

import socket

UDP_IP="192.168.1.100"
UDP_PORT = 5005
msg = chr(5)
MESSAGE = "Hello, World!"
Ns = 60


binary = '{0:08b}'.format(6)
print("binary: ", binary)

print ("UDP target IP:", UDP_IP)
print ("UDP target port:", UDP_PORT)
print ("message:", MESSAGE)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # UDP
import wave, struct
waveFile = wave.open('spring_HiFi.wav', 'r')

2281
length = waveFile.getnframes()
Ms = int(length/Ns)
Packet = ""
print(Ms)

for i in range(0,Ms):
    for j in range (0, Ns) :
        for k in range(0,1):
             data = struct.unpack("<h",  waveFile.readframes(1))
        Hexa = '{0:04d}'.format(int((data[0] + 32675) * (273/4369)))
        Packet = Packet + Hexa

    sock.sendto(Packet.encode(), (UDP_IP.encode(), UDP_PORT))
    print("binary: ", Packet)
    Packet = ""

import socket
import codecs

def int2bytes(i):
    hex_value = '{0:x}'.format(i)
    # make length of hex_value a multiple of two
    print(hex_value)
    hex_value = '0' * (len(hex_value) % 2) + hex_value
    print(hex_value)
    return codecs.decode(hex_value, 'hex_codec')


print(int2bytes(255))

DATA_L_MASK = 0x00FF
DATA_H_MASK = 0xFF00

UDP_IP="192.168.1.100"
UDP_PORT = 5005
msg = chr(5)
MESSAGE = "Hello, World!"
Ns = 220
print ("UDP target IP:", UDP_IP)
print ("UDP target port:", UDP_PORT)
print ("message:", MESSAGE)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # UDP
import wave, struct
waveFile = wave.open('spring_HiFi.wav', 'r')

length = waveFile.getnframes()


decimal = 0

if decimal == 1:

    Ms = int(length / Ns)
    Packet = ""

    for i in range(0, Ms):
        for j in range (0, Ns) :
            data = struct.unpack("<h", waveFile.readframes(1))
            decimal = '{0:04d}'.format(int((data[0] + 32675) * (273/4369)))
            Packet = Packet + decimal

        sock.sendto(Packet.encode(), (UDP_IP.encode(), UDP_PORT))
        print("binary: ", Packet)
        Packet = ""

else:

    Ms = int(length / Ns)
    Packet = ""
    for i in range(0, Ms):
        for j in range(0, Ns):
            data = struct.unpack("<h", waveFile.readframes(1))
            Hexa_H = chr((data[0] & DATA_H_MASK) >> 8 )
            print(Hexa_H)
            data = struct.unpack("<h", waveFile.readframes(1))
            Hexa_L = chr(data[0] & DATA_L_MASK)
            print(Hexa_L)
            Packet = Packet + str(Hexa_H) + str(Hexa_L)

        sock.sendto(Packet.encode(), (UDP_IP.encode(), UDP_PORT))
        print("binary: ", Packet)
        print("encode: ", Packet.encode())
        Packet = ""

import socket, time, sys
sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
while True:
    sock.sendto(b"8 bytes\0",('127.0.0.1' if len(sys.argv)<2 else sys.argv[1],3956 if len(sys.argv)<3 else int(sys.argv[2])))
    sys.stderr.write('.'); sys.stderr.flush()
    time.sleep(.2)

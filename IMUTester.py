import socket, traceback
# Note that the IP Address and Port in this script and the script on the Mobile Phone should match.
host = "10.192.50.36"   #IP Address of this Machine
port = 3400
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
s.bind((host, port))
while 1:
    try:
        message, address = s.recvfrom(4096)
        print message
    except (KeyboardInterrupt, SystemExit):
        raise
    except:
        traceback.print_exc()

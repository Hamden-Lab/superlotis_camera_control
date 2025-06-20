import socket

def send_command(command, host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((host, port))
        s.sendall(command)  
        response = s.recv(4096)  
        print 'Response from server:', response
    finally:
        s.close()

send_command('expose', 'localhost', 6972)


#define time variables
#set exptime=1000
#verify exptime
#get temp
#get analog gain
#(or maybe generate a config file, which sets temp, gain?)
#take 5 test images - run 'expose' 5 times (changing names of each image?)


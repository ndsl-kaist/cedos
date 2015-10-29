
# Name of the main program
CLIENT = client
CLIENT_ARM = client-arm
SERVER = server

CLIENT_TCP = client-tcp
CLIENT_TCP_ARM = client-tcp-arm
SERVER_TCP = server-tcp

all: $(CLIENT) $(CLIENT_ARM) $(SERVER) $(CLIENT_TCP) $(CLIENT_TCP_ARM) $(SERVER_TCP)
	adb push $(CLIENT_ARM) /sdcard/$(CLIENT_ARM)
	adb shell "su -c cp /sdcard/$(CLIENT_ARM) /data/$(CLIENT_ARM)"
	adb shell "su -c chmod 755 /data/$(CLIENT_ARM)"
	adb push $(CLIENT_TCP_ARM) /sdcard/$(CLIENT_TCP_ARM)
	adb shell "su -c cp /sdcard/$(CLIENT_TCP_ARM) /data/$(CLIENT_TCP_ARM)"
	adb shell "su -c chmod 755 /data/$(CLIENT_TCP_ARM)"

# Compilation and link
$(CLIENT): client.c
	gcc client.c -g -ggdb -L../../arch/host/obj -ldtp -levent -lcrypto -lssl -o $(CLIENT)

$(CLIENT_ARM): client.c
	arm-linux-androideabi-gcc client.c -L../../arch/arm/obj -ldtp -o $(CLIENT_ARM) -llog

$(SERVER): server.c
	gcc server.c -g -ggdb -L../../arch/host/obj -ldtp -levent -lcrypto -lssl -o $(SERVER)

$(CLIENT_TCP): client-tcp.c
	gcc client-tcp.c -g -ggdb -o $(CLIENT_TCP)

$(CLIENT_TCP_ARM): client-tcp.c
	arm-linux-androideabi-gcc client-tcp.c -o $(CLIENT_TCP_ARM) -llog

$(SERVER_TCP): server-tcp.c
	gcc server-tcp.c -g -ggdb -o $(SERVER_TCP)

clean:
	-rm -f $(CLIENT) $(CLIENT_ARM) $(SERVER) $(CLIENT_TCP) $(CLIENT_TCP_ARM) $(SERVER_TCP) 
	-rm -f *.o


CC = g++
COMFLAG = -g -fPIC
LDFLAG = -g -fPIC -shared
INCLUDE = -I ./
LIBDIR = 
LIB = 
BIN = ./
TARGET = libnetlib.so
SRCS = Accept.cpp Common_Func.cpp Config.cpp Connect.cpp Connector.cpp \
		Date_Time.cpp Epoll_Watcher.cpp HotUpdate.cpp Lib_Log.cpp Log_Connector.cpp \
		Log.cpp Misc.cpp Mysql_Conn.cpp Pack.cpp Receive.cpp Send.cpp Server.cpp Svc.cpp \
		Time_Value.cpp Base64.cpp Sha1.cpp Svc_Tcp.cpp Svc_WebSocket.cpp

$(TARGET):$(SRCS:.cpp=.o)
	$(CC) $(LDFLAG) $(LIBDIR) $(LIB) -o $@ $^
	#-rm -f *.o *.d
	-mv $(TARGET) /usr/local/lib64

%.o:%.cpp
	$(CC) $(COMFLAG) $(INCLUDE) -c -o $@ $<

clean:
	-rm -f *.o *.d

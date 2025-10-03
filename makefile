CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

# Source files
CLIENT_SRC = clientmain.cpp
SERVER_SRC = servermain.cpp

# Executables
CLIENT = client
SERVER = server

all: $(CLIENT) $(SERVER)

$(CLIENT): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $(CLIENT) $(CLIENT_SRC)

$(SERVER): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(SERVER) $(SERVER_SRC)

clean:
	rm -f $(CLIENT) $(SERVER) *.o

.PHONY: all clean


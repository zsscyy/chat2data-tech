all: cal.pb.cc cal.pb.h client server
cal.pb.cc cal.pb.h: cal.proto
	protoc --cpp_out=. $<
client : client.cc cal.pb.cc
	g++ $^ -o $@ -std=c++17 -lbite_scaffold -lbrpc -lprotobuf  -lgflags -lfmt -lcrypto -lssl -lleveldb
server : server.cc cal.pb.cc
	g++ $^ -o $@ -std=c++17 -lbite_scaffold -lbrpc -lprotobuf  -lgflags -lfmt -lcrypto -lssl -lleveldb
clean:
	rm -rf client server cal.pb.cc cal.pb.h
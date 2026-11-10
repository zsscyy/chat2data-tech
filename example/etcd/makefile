all: userService fileService aiService gateway
userService : userService.cc
	g++ $^ -o $@ -std=c++17 -lbite_scaffold -letcd-cpp-api -lcpprest -lfmt -ljsoncpp
fileService : fileService.cc
	g++ $^ -o $@ -std=c++17 -lbite_scaffold -letcd-cpp-api -lcpprest -lfmt -ljsoncpp
aiService : aiService.cc
	g++ $^ -o $@ -std=c++17 -lbite_scaffold -letcd-cpp-api -lcpprest -lfmt -ljsoncpp
gateway : gateway.cc
	g++ $^ -o $@ -std=c++17 -lbite_scaffold -letcd-cpp-api -lcpprest -lfmt -ljsoncpp
clean:
	rm -rf userService fileService aiService gateway

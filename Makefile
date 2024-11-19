all: twmailer-client twmailer-server
twmailer-client: twmailer-client.cpp
	g++ -std=c++17 -Wall -Werror -o twmailer-client twmailer-client.cpp
twmailer-server: twmailer-server.cpp
	g++ -std=c++17 -Wall -Werror -o twmailer-server twmailer-server.cpp
clean:
	rm -f twmailer-client
	rm -f twmailer-server
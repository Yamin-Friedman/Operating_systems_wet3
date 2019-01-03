


#include <sys/types.h>
#include <sys/socket.h>
#include <winsock.h>
#include <cstdio>

#define TEMP_PORT 69 // To be replaced
#define WAIT_FOR_PACKET_TIMEOUT 3
#define NUMBER_OF_FAILURES  7

int open_socket(int port){
	int descriptor;
	int res;

	descriptor = socket(AF_INET,SOCK_DGRAM,0);
	if (descriptor == -1) {
		perror("TTFTP_ERROR:");
		exit(-1);
	}

	struct sockaddr_in Addr = {0};
	Addr.sin_family = AF_INET;
	Addr.sin_addr.s_addr = INADDR_ANY;
	Addr.sin_port = htons(port);

	res = bind(descriptor,(struct sockaddr *)&Addr,sizeof(Addr));
	if (res == -1) {
		perror("TTFTP_ERROR:");
		exit(-1);
	}

	res = listen(descriptor,1);

	if (res == -1) {
		perror("TTFTP_ERROR:");
		exit(-1);
	}
}

int main(){


	int descriptor,clientfd;
	int res;
	struct sockaddr_in client_addr = {0};
	int client_addr_len = sizeof(client_addr);

	descriptor = open_socket(TEMP_PORT);

	//listen forever
	while(1) {

		clientfd = accept(descriptor,(struct sockaddr *)&client_addr,&client_addr_len);
		if (clientfd == -1) {
			perror("TTFTP_ERROR:");
			exit(-1);
		}


	}




}

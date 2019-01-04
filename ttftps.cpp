


#include <sys/types.h>
#include <sys/socket.h>
#include <winsock.h>
#include <cstdio>
#include <unistd.h>
#include <vector>

using namespace std;

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

		char *buffer = new(516);

		res = recv(clientfd,buffer,516,NULL);
		if (res == -1) {
			perror("TTFTP_ERROR:");
			delete buffer;
			exit(-1);
		}

		int size_read  = res;

		res = check_WRQ_packet(buffer,size_read);
		if (res == -1) {
			//error with WRQ
		}

		send_ACK_packet(clientfd,0);

		do
		{
			do
			{

				do
				{
					// TODO: Wait WAIT_FOR_PACKET_TIMEOUT to see if something appears
					// for us at the socket (we are waiting for DATA)

					if ()// TODO: if there was something at the socket and
						// we are here not because of a timeout
					{
						// TODO: Read the DATA packet from the socket (at
						// least we hope this is a DATA packet)
					}
					if (...) // TODO: Time out expired while waiting for data
					// to appear at the socket
					{
//TODO: Send another ACK for the last packet
						timeoutExpiredCount++;
					}

					if (timeoutExpiredCount>= NUMBER_OF_FAILURES)
					{
						// FATAL ERROR BAIL OUT
					}

				}while (...) // TODO: Continue while some socket was ready
				// but recvfrom somehow failed to read the data

				if (...) // TODO: We got something else but DATA
				{
					// FATAL ERROR BAIL OUT
				}
				if (...) // TODO: The incoming block number is not what we have
				// expected, i.e. this is a DATA pkt but the block number
				// in DATA was wrong (not last ACK’s block number + 1)
				{
					// FATAL ERROR BAIL OUT
				}
			}while (FALSE);
			timeoutExpiredCount = 0;
			lastWriteSize = fwrite(...); // write next bulk of data
// TODO: send ACK packet to the client
		}while (...); // Have blocks left to be read from client (not end of transmission)
	}




}

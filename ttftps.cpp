#include <sys/types.h>
#include <sys/socket.h>
//#include <winsock.h>
#include <cstdio>
#include <unistd.h>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>


using namespace std;

#define TEMP_PORT 2001 // To be replaced
#define WAIT_FOR_PACKET_TIMEOUT 3
#define NUMBER_OF_FAILURES  7
#define ACK_OPCODE  4
#define WRQ_OPCODE  2
#define DATA_PACK_SIZE 512


struct ackPack {
	uint16_t opcode;
	uint16_t blockNum;
}__attribute__((packed));


int open_socket(int port) {
	int descriptor;
	int res;

	descriptor = socket(AF_INET, SOCK_DGRAM , 0);
	if (descriptor == -1) {
		perror("TTFTP_ERROR:");
		exit(-1);
	}

	struct sockaddr_in Addr = { 0 };
	Addr.sin_family = AF_INET;
	Addr.sin_addr.s_addr = INADDR_ANY;
	Addr.sin_port = htons(port);

	res = bind(descriptor, (struct sockaddr *)&Addr, sizeof(Addr));
	if (res == -1) {
		perror("TTFTP_ERROR:");
		exit(-1);
	}

	//res = listen(descriptor, 1);
	if (res == -1) {
		perror("TTFTP_ERROR:");
		exit(-1);
	}

	return descriptor;
}

int parse_WQE_packet(char *buffer, char *filename, char *trans_mode, int *opcode){

	if(buffer == NULL)
		return -1;

	char *ptr = buffer + 2;

	*opcode = *(buffer + 1);
	for(int i = 0; *ptr != 0; i++) {
		filename[i] = *ptr;
		ptr++;
	}
	ptr++;
	for(int i = 0; *ptr != 0; i++) {
		trans_mode[i] = *ptr;
		ptr++;
	}

	return 0;
}

void send_ACK_packet(int descriptor, int blockNum, struct sockaddr_in *sock, int len) {
	struct ackPack ackpack;
	int res;
	ackpack.blockNum = htons(blockNum);
	ackpack.opcode = htons(ACK_OPCODE);
	// not sure what to type in the buf parameter:
	res = sendto(descriptor, (void*)&ackpack, sizeof(ackpack), 0, (struct sockaddr*)sock, sizeof(*sock));
	if (res == -1) {
		perror("Ack sending ERROR\n");
	}
	printf("OUT:ACK,%d\n", blockNum);
}

int check_WRQ_packet(int opcode, char *mode) {


	if (opcode == WRQ_OPCODE && strcmp(mode,"octet\0") == 0) {
		return 0;
	}
	return -1;
}

int check_data_packet(dataPack *packet,int block_num, int packet_size) {
	return 0;
}

int main() {

	int descriptor, clientfd;
	int res;
	struct sockaddr_in client_addr = { 0 };
	socklen_t client_addr_len = sizeof(client_addr);
	struct timeval tv;
	bool packet_ready = false;
	int timeoutExpiredCount = 0;
	int block_num = 0;
	bool last_data_packet = false;
	uint16_t data_size;
	int lastWriteSize;
	char *data = NULL;
	int data_pack_size;

	descriptor = open_socket(TEMP_PORT);

	//listen forever
	while (true) {
		//clientfd = accept(descriptor, (struct sockaddr *)&client_addr, &client_addr_len);
		//if (clientfd == -1) {
		//	perror("TTFTP_ERROR:");
		//	exit(-1);
		//}

		char *buffer = new char[516];
		//res = recv(clientfd, buffer, 516, NULL);
		res = recvfrom(descriptor, buffer, 516, 0, (struct sockaddr *) &client_addr, &client_addr_len);
		if (res == -1) {
			perror("TTFTP_ERROR:");
			delete buffer;
			exit(-1);
		}

		int size_read = res;

		char filename[516] = {0};
		char trans_mode[6] = {0};
		int opcode = 0;

		res = parse_WQE_packet(buffer,filename,trans_mode,&opcode);
		if (res == -1) {
			perror("TTFTP_ERROR:");
			delete buffer;
			exit(-1);
		}

		res = check_WRQ_packet(opcode, trans_mode);
		if (res == -1) {
			cerr << "FLOWERROR: Unsupported WRQ format" << endl;
			delete buffer;
			exit(-1);
		}

		printf("IN:WRQ, %s, %s\n", filename, trans_mode);

		FILE *file = fopen(filename, "w");
		send_ACK_packet(descriptor, block_num, &client_addr, client_addr_len);
		last_data_packet = false;

		do
		{
			do
			{

				do
				{

					// TODO: Wait WAIT_FOR_PACKET_TIMEOUT to see if something appears
					// for us at the socket (we are waiting for DATA)
					tv.tv_sec = WAIT_FOR_PACKET_TIMEOUT;
					tv.tv_usec = 0;
					fd_set rfds;
					FD_ZERO(&rfds);
					FD_SET(descriptor, &rfds);
					res = select(descriptor + 1,&rfds,NULL,NULL,&tv);

					if (res == -1) {
						perror("TTFTP_ERROR:");
						delete buffer;
						exit(-1);
					} else if (res){
						packet_ready = true;
					} else {
						packet_ready = false;
					}
					// TODO: if there was something at the socket and
					// we are here not because of a timeout
					if (packet_ready)
					{
						// TODO: Read the DATA packet from the socket (at
						// least we hope this is a DATA packet)
						memset(buffer,0,DATA_PACK_SIZE + 2);
						res = recv(descriptor, buffer, 516, 0);
						if (res == -1) {
							perror("TTFTP_ERROR:");
							delete buffer;
							exit(-1);
						}
						printf("recieved data packet\n");

					}
					if (!packet_ready) //Time out expired while waiting for data to appear at the socket
					{
						send_ACK_packet(descriptor,block_num, &client_addr, client_addr_len);
						timeoutExpiredCount++;
					}

					if (timeoutExpiredCount >= NUMBER_OF_FAILURES)
					{
						//Error message
						delete buffer;
						exit(-1);
					}

				} while (res == -1); //Continue while some socket was ready but recvfrom somehow failed to read the data

				data_size = *(buffer + 1);
				data = buffer + 2;

				block_num = block_num + 1;
				if (data_size < DATA_PACK_SIZE) {
					printf("test\n");
					last_data_packet = true;
				}
				if (res == -1) // TODO: We got something else but DATA
				{
					// FATAL ERROR BAIL OUT
				}
				if (res != block_num + 1)
				{
					// FATAL ERROR BAIL OUT
				}
				//block_num = res + 1; // Update block number to ACK
			} while (false);
			timeoutExpiredCount = 0;
			lastWriteSize = fwrite(data,data_size,1,file); // write next bulk of data
			send_ACK_packet(descriptor,block_num, &client_addr, client_addr_len);
		} while (!last_data_packet); // Have blocks left to be read from client (not end of transmission).
		res = fclose(file);
		if ( res == EOF) {
			perror("TTFTP_ERROR:");
			delete buffer;
			exit(-1);
		}
	}




}

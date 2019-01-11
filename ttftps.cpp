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
#define DATA_PACK_SIZE 516


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

int parse_data_packet(char *buffer, int buffer_size, uint16_t *data_size, int *data_block_num, char** data) {

	if (buffer_size < 4) {
		return  -1;
	}

	uint16_t  *two_byte_ptr = (uint16_t *)buffer;
	int opcode = ntohs(*two_byte_ptr);

	if (opcode != 3) {
		return -1;
	}
	*data_size = buffer_size - 4;
	two_byte_ptr = (uint16_t *)(buffer + 2);
	*data_block_num = ntohs(*two_byte_ptr);
	*data = buffer + 4;

	return 0;
}

void send_ACK_packet(int descriptor, int blockNum, struct sockaddr_in *sock, int len) {
	struct ackPack ackpack;
	int res;
	ackpack.blockNum = htons(blockNum);
	ackpack.opcode = htons(ACK_OPCODE);

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


int main() {

	int descriptor, clientfd;
	int res;
	struct sockaddr_in client_addr = { 0 };
	socklen_t client_addr_len = sizeof(client_addr);
	struct timeval tv;
	bool packet_ready = false;
	int timeoutExpiredCount = 0;
	bool last_data_packet = false;
	uint16_t data_size;
	int lastWriteSize;
	char *data = NULL;
	int data_pack_size;
	uint16_t *two_byte_ptr = NULL;
	bool HW_error = false;

	descriptor = open_socket(TEMP_PORT);

	char *buffer = new char[DATA_PACK_SIZE];

	//listen forever
	while (true) {
		int block_num = 0;
		int data_block_num = 0;
		HW_error = false;

		memset(buffer,0,DATA_PACK_SIZE);
		res = recvfrom(descriptor, buffer, DATA_PACK_SIZE, 0, (struct sockaddr *) &client_addr, &client_addr_len);
		if (res <= 0) {
			perror("TTFTP_ERROR:");
			delete buffer;
			exit(-1);
		}

		int size_read = res;

		char filename[DATA_PACK_SIZE] = {0};
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

					// Wait WAIT_FOR_PACKET_TIMEOUT to see if something appears
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
					// if there was something at the socket and
					// we are here not because of a timeout
					if (packet_ready)
					{
						// Read the DATA packet from the socket (at
						// least we hope this is a DATA packet)
						memset(buffer,0,DATA_PACK_SIZE );
						res = recv(descriptor, buffer, DATA_PACK_SIZE, 0);
						if (res == -1) {
							perror("TTFTP_ERROR:");
							delete buffer;
							exit(-1);
						}
					}

					if (timeoutExpiredCount >= NUMBER_OF_FAILURES)
					{
						HW_error = true;
						break;
					}

					if (!packet_ready) //Time out expired while waiting for data to appear at the socket
					{
						send_ACK_packet(descriptor,block_num, &client_addr, client_addr_len);
						timeoutExpiredCount++;
					}

				} while (res == -1); //Continue while some socket was ready but recvfrom somehow failed to read the data

				// If the were too many timeouts
				if (HW_error) {
					last_data_packet = true;
					break;
				}

				res = parse_data_packet(buffer,res,&data_size,&data_block_num,&data);

				if (res == -1) // We got something else but DATA
				{
					HW_error = true;
					last_data_packet = true;
					break;
				}
				if (data_block_num != block_num + 1)
				{
					HW_error = true;
					last_data_packet = true;
					break;
				}
				if (data_size < DATA_PACK_SIZE - 4) {
					last_data_packet = true;
				}
				block_num = data_block_num; // Update block number to ACK
				printf("IN:DATA, %d, %d\n",data_block_num,data_size + 4);

				send_ACK_packet(descriptor,block_num, &client_addr, client_addr_len);
			} while (false);
			timeoutExpiredCount = 0;
			if (!HW_error) {
				lastWriteSize = fwrite(data, 1, data_size, file);
				printf("WRITING:%d\n",data_size);
			}
		} while (!last_data_packet); // Have blocks left to be read from client (not end of transmission).
		res = fclose(file);
		if ( res == EOF) {
			perror("TTFTP_ERROR:");
			delete buffer;
			exit(-1);
		}
		if(HW_error)
			printf("RECVFAIL\n");
		else
			printf("RECVOK\n");
	}

}

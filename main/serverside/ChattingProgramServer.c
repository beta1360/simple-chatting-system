#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#define BUFSIZE 100
#define MAX_CLIENT_NUM 20 // 최대 20명 접속
void ErrorHandling(char *message);

typedef struct peerInfo {
	SOCKADDR_IN Socket;
	SOCKET fd;
	char *nickname;
}peerInfo;

int main(int argc, char **argv) {
	WSADATA wsaData;
	SOCKET hServSock, hClntSock;
	SOCKADDR_IN servAddr, clntAddr;
	peerInfo peerArray[MAX_CLIENT_NUM];
	fd_set reads, temps;

	int clntLen, strLen;
	int CheckPeerNumber = 0;
	char* quitPeer_Nickname;
	char message[BUFSIZE];
	
	TIMEVAL timeout; // the same as struct timeval timeout;

	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) /* Load Winsock 2.2 DLL */
		ErrorHandling("WSAStartup() error!");

	hServSock = socket(PF_INET, SOCK_STREAM, 0);
	
	if (hServSock == INVALID_SOCKET)
		ErrorHandling("socket() error");

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(atoi(argv[1]));
	
	if (bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		ErrorHandling("bind() error");
	if (listen(hServSock, 5) == SOCKET_ERROR)
		ErrorHandling("listen() error");

	FD_ZERO(&reads);
	FD_SET(hServSock, &reads);

	while (1) // 무한루프 (서버측)
	{
		temps = reads;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		for (int i = 0; i < BUFSIZE; i++)
			message[i] = 0; // 메세지 초기화

		if (select(0, &temps, 0, 0, &timeout) == SOCKET_ERROR)
			ErrorHandling("select() error");

		for (int arrIndex = 0; arrIndex<reads.fd_count; arrIndex++) { // 무한루프를 돌고 있을 때 각 피어들을 순회하면서 메세지를 받아옴.
			if (FD_ISSET(reads.fd_array[arrIndex], &temps)) { 
				if (reads.fd_array[arrIndex] == hServSock) { // 연결요청을 받은 경우
					clntLen = sizeof(clntAddr);
					hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &clntLen);

					FD_SET(hClntSock, &reads);
					recv(hClntSock, message, BUFSIZE - 1, 0);

					size_t nickSize = strcspn(message, "]");
					char* nick = (char*)malloc(sizeof((int)nickSize));

					memcpy(nick, message + 1, nickSize - 1);
					peerArray[CheckPeerNumber].nickname = (char*)malloc(sizeof((int)nickSize));
					peerArray[CheckPeerNumber].fd = hClntSock;
					peerArray[CheckPeerNumber].Socket = clntAddr;
					strcpy(peerArray[CheckPeerNumber].nickname, nick);
					peerArray[CheckPeerNumber].nickname[nickSize - 1] = '\0';
					// 연결 요청 받은 피어의 정보를 배열에 입력해준다.

					CheckPeerNumber++;
					// 배열을 가리키는 플래그의 수를 1 증가.

					printf("Connection to client : socket handle %d \n", hClntSock);

					send(hClntSock, "Connection Complete!\n", strlen("Connection Complete!\n"), 0);
					for (int i = 1; i < reads.fd_count - 1; i++){
						send(reads.fd_array[i], peerArray[CheckPeerNumber - 1].nickname, strlen(peerArray[CheckPeerNumber - 1].nickname), 0);
						send(reads.fd_array[i], "님이 채팅방에 입장하셨습니다.\n", strlen("님이 채팅방에 입장하셨습니다.\n"), 0);
					} // 그 후에 다른 피어들에게 연결되었음을 알려주기 위해서 send() 함수를 호출시켜준다.
				}
				else { // 만일 피어들에게 메세지를 받을 경우
					strLen = recv(reads.fd_array[arrIndex], message, BUFSIZE - 1, 0);
					if ((strLen == 0) || (strLen == SOCKET_ERROR)) { // 피어한테서 채팅프로그램 종료 요청을 받을 경우
						closesocket(reads.fd_array[arrIndex]);
						printf("Close Client : Socket Handle %d \n", reads.fd_array[arrIndex]);    
						FD_CLR(reads.fd_array[arrIndex], &reads);

						quitPeer_Nickname = (char *)malloc(sizeof(strlen(peerArray[arrIndex - 1].nickname)));
						strcpy(quitPeer_Nickname, peerArray[arrIndex - 1].nickname);

						for (int i = arrIndex; i < CheckPeerNumber; i++) {
							peerArray[i - 1].fd = peerArray[i].fd;
							peerArray[i - 1].Socket = peerArray[i].Socket;
							strcpy(peerArray[i - 1].nickname, peerArray[i].nickname);
						} // 피어들을 저장한 배열을 줄여버린다.

						CheckPeerNumber--;
						// 배열을 가리키는 플래그의 수를 1 감소.

						for (int i = 1; i < CheckPeerNumber + 1; i++)
						{
							send(reads.fd_array[i], quitPeer_Nickname, strlen(quitPeer_Nickname), 0);
							send(reads.fd_array[i], "님이 퇴장하셨습니다.\n", strlen("님이 퇴장하셨습니다.\n"), 0);
						} // 다른 피어들에게 종료된 피어가 있음을 알려줌.
					}
					else { // 다른 메세지를 전달 받을 경우 (종료 이외의)
						if (strstr(message, "/l\n")) { // /l 옵션을 사용하고자 할 때 ('/l\n' 이라는 문자열이 검색되었을 때)
							send(reads.fd_array[arrIndex], "--------------접속자 리스트--------------\n", strlen("--------------접속자 리스트--------------\n"), 0);
							for (int i = 0; i < CheckPeerNumber; i++) {
								send(reads.fd_array[arrIndex], peerArray[i].nickname, strlen(peerArray[i].nickname), 0);
								send(reads.fd_array[arrIndex], " : ", strlen(" : "), 0);
								send(reads.fd_array[arrIndex], inet_ntoa(peerArray[i].Socket.sin_addr), strlen(inet_ntoa(peerArray[i].Socket.sin_addr)), 0);
								send(reads.fd_array[arrIndex], "\n", strlen("\n"), 0);
							} // 피어들의 정보가 저장되어 있는 배열을 순회하면서 해당 옵션을 요청한 피어에게 메세지 전송해줌.
							send(reads.fd_array[arrIndex], "------------------------------------------\n", strlen("------------------------------------------\n"), 0);
						}
						else if (strstr(message, "/h ")) { // '/h' 옵션 요청을 받았을 경우 ('/h ' 이라는 문자열이 검색되었을 때)
							int hCount = 0;
							char message_temp[BUFSIZE]; // 임시로 사용할 메세지의 크기.

							for (int i = 0; i < CheckPeerNumber; i++) {
								for (int j = 0; j < strlen(message) - (strlen(peerArray[arrIndex - 1].nickname) + 5); j++)
									strcpy(message_temp + j, message + j + strlen(peerArray[arrIndex - 1].nickname) + 5);
								// 메세지 크기 재 조정 
								// [Han] /h Kim Hello World!!  => Kim Hello World!

								if (strstr(message_temp, peerArray[i].nickname) != NULL) {
									// 위의 문자열(message_temp)이 만약 피어의 이름을 포함하고 있을 때
									char message_h[BUFSIZE];
									for (int k = 0; k < strlen(message_temp) - (strlen(peerArray[i].nickname) + 2); k++)
										strcpy(message_h, message_temp + (strlen(peerArray[i].nickname) + 2));
									// 문자열을 다시 재조정
									// Kim Hello World! => Hello World!

									if (reads.fd_array[i+1] != hServSock) {
										send(reads.fd_array[i + 1], peerArray[arrIndex - 1].nickname, strlen(peerArray[arrIndex - 1].nickname), 0);
										send(reads.fd_array[i + 1], "->", strlen("->"), 0);
										send(reads.fd_array[i + 1], peerArray[i].nickname, strlen(peerArray[i].nickname), 0);
										send(reads.fd_array[i + 1], "] ", strlen("] "), 0);
										send(reads.fd_array[i + 1], message_h, strlen(message_h), 0);
									} // /h 옵션을 요청한 피어의 메세지를 전달하고자 하는 피어에게 귓속말 메세지를 전송.
									break;
								}
								hCount++;
								// 피어의 갯수만큼 체크
							}
							if (hCount >= CheckPeerNumber) {
								char failMessage[BUFSIZE] = "귓속말을 할 대상을 찾을 수가 없습니다.\n";
								send(reads.fd_array[arrIndex], failMessage, strlen(failMessage), 0);
							} // hCount >= CheckPeerNumber라는 말은 피어의 배열만큼 순회해도 해당 조건을 만족하는 피어의 이름이 없다는 뜻
						}
						else { // 가장 일반적인 메세지 전송 (채팅)
							for (int i = 0; i < reads.fd_count; i++)
								if (reads.fd_array[i] != hServSock)
									send(reads.fd_array[i], message, strLen, 0);
						}
					}
				} 
			} 
		} 
	} 

	WSACleanup();

	return 0;
}
void ErrorHandling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

#include<iostream>
#include<string>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<thread>
#include<map>
using namespace std;
#pragma comment(lib,"ws2_32.lib")
char* ipaddress = new char[100];
int port;
int flag = 1;
char* name = new char[100];
DWORD WINAPI receive(LPVOID P);
DWORD WINAPI my_send(LPVOID P);
int main() {
	// Declare and initialize variables
	WSADATA wsaData;
	struct hostent* remoteHost = NULL;
	char serverHostName[128] = {};

	// Initialize Winsock
	int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (r != 0)
	{
		printf("WSAStartup failed: %d\n", r);
		return 0;
	}
	printf("WSAStartup succeed!\n");
	//gethostname
	r = gethostname(serverHostName, sizeof(serverHostName));
	if (r == 0)
	{
		printf("gethostname succeed: %s\n", serverHostName);
	}
	else
	{
		printf("gethostname failed(errcode %d)\n", ::GetLastError());
	}
	cout << "---------Initializing,please wait...-----------" << endl;
	//socket
	SOCKET s;
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		cout << "Create socket failed." << endl;
		WSACleanup();
		return 0;
	}
	else {
		cout << "Create socket succeed." << endl;
	}

	sockaddr_in sa;
	ipaddress = "127.0.0.1";
	port = 8000;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr(ipaddress);
	sa.sin_port = htons(port);
	cout << "Server IP: " << ipaddress << ":" << port << endl;
	r = connect(s, (SOCKADDR*)&sa, sizeof(SOCKADDR));
	if (r == 0) {
		cout << "Connection succeed." << endl;
	}
	else {
		cout << "Connection failed." << endl;
		closesocket(s);
		WSACleanup();
		return 0;
	}

	recv(s, name, 100, 0);
	cout << "----------Chat begins.Here's your username: " << name <<"----------" << endl;
	cout << "Here are some tips:" << endl;
	cout << "Input 'change name to <new name>' to change your name" << endl;
	cout << "Input 'send to <username> <message>' to send message to someone privately." << endl;
	cout << "Input 'quit' to quit this chat room.";
	cout << "Input message directly to send message to all people in this chat room." << endl;
	cout << "That's all! Have a good day!" << endl;
	HANDLE t[2];
	t[0] = CreateThread(NULL, 0, receive, (LPVOID)&s, 0, NULL);
	t[1] = CreateThread(NULL, 0, my_send, (LPVOID)&s, 0, NULL);
	WaitForMultipleObjects(2, t, TRUE, INFINITE);
	CloseHandle(t[1]); CloseHandle(t[0]);
	closesocket(s);
	WSACleanup();
}
void outTime() {
	SYSTEMTIME t;
	GetLocalTime(&t);
	cout << t.wYear << "/" << t.wMonth << "/" << t.wDay << " " << t.wHour << ":" << t.wMinute << ":" << t.wSecond << " ";
}

DWORD WINAPI receive(LPVOID p) {
	int r;
	SOCKET* s = (SOCKET*)p;
	char* recvBuff = new char[100];
	while (true) {
		r = recv(*s, recvBuff, 100, 0);
		if (flag && r > 0) {
			outTime();
			cout << recvBuff << endl;
		}
		else {
			closesocket(*s);
			return 0;
		}
	}
}

DWORD WINAPI my_send(LPVOID p) {
	int r;
	SOCKET* s = (SOCKET*)p;
	char* sendBuff = new char[100];
	while (true) {
		cin.getline(sendBuff, 100);
		if (string(sendBuff) == "quit") {
			r = send(*s, sendBuff, 100, 0);
			flag = 0;
			closesocket(*s);
			cout << "You quit! Goodbye!" << endl;
			return 1;
		}
		r = send(*s, sendBuff, 100, 0);
		if (r == SOCKET_ERROR) {
			cout << "Sending messages failed." << endl;
			closesocket(*s);
			WSACleanup();
			return 0;
		}
		else {
			outTime();
			cout << "Sending messages succeed." << endl;
		}
	}
}
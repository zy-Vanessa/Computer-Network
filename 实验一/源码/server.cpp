#include<iostream>
#include<string>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<thread>
#include<map>
#include<vector>
#include<conio.h>
using namespace std;
#pragma comment(lib,"ws2_32.lib")
char* ipaddress = new char[100]; //ip地址
int port;//端口号
char message[100];
int number = 0; //人数
DWORD WINAPI process(LPVOID p);//线程执行函数
map<SOCKET, int>store_info;
map<string, string>name_change;
int main() {
	// Declare and initialize variables
	WSADATA wsaData;
	struct hostent* remoteHost = NULL;
	char serverHostName[128] = {};

	// Initialize Winsock
	//进行一次系统初始化
	int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (r != 0)
	{
		printf("WSAStartup failed: %d\n", r);
		return 0;
	}
	printf("WSAStartup succeed!\n");
	//gethostname依赖wind socket
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
	//启动一个socket
	SOCKET s;
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//初始化 IPV-4，下层协议选择数据流，根据要求选择TCP协议
	if (s == INVALID_SOCKET) {
		cout << "Create socket failed." << endl;
		WSACleanup();//释放dll资源
		return 0;
	}
	else {
		cout << "Create socket succeed." << endl;
	}
	//绑定套接字，将一个指定地址绑定到指定的Socket
	sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = PF_INET;
	ipaddress = "127.0.0.1";
	port = 8000; //超过范围 默认8000
	sa.sin_addr.s_addr = inet_addr(ipaddress);  //对S_un.S_addr进行赋值
	sa.sin_port = htons(port);  //端口
	cout << "server IP: " << ipaddress << ":" << port << endl;

	//绑定
	r = bind(s, (SOCKADDR*)&sa, sizeof(SOCKADDR));//执行绑定
	if (r == 0) {
		cout << "bind succeeed." << endl;
	}
	else {
		closesocket(s);//释放socket资源
		WSACleanup();//释放dll资源
		cout << "bind failed." << endl;
		return 0;
	}

	//进入监听状态
	if (listen(s, 10) == 0) {//最大队列为10
		cout << "----------Initialized chat room successfully with up to 10 people----------" << endl;
		cout << "listen secceed." << endl;
	}
	else {
		cout << "listen failed." << endl;
		closesocket(s);//释放socket资源
		WSACleanup();//释放dll资源
		return 0;
	}
	cout << "----------Waiting for Client to Join----------" << endl;

	//处理成功接受的客户端，为他们分配资源
	while (1) {
		int size = sizeof(sockaddr_in);
		sockaddr_in clientaddr;
		SOCKET cs;
		cs = accept(s, (SOCKADDR*)&clientaddr, &size);
		if (cs == INVALID_SOCKET) {
			cout << "Connection failed." << endl;
			closesocket(s);//释放socket资源
			WSACleanup();//释放dll资源
			return 0;
		}
		else{
			//HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, 
			//LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
			//线程属性、线程堆栈大小、线程执行函数、传入线程参数、创建线程参数、新线程ID
			//LPVOID是一个没有类型的指针，可以将任意类型的指针赋值给LPVOID类型的变量（一般作为参数传递），然后在使用的时候再转换回来
			HANDLE cthread = CreateThread(NULL, 0, process, (LPVOID)cs, 0, NULL);
			CloseHandle(cthread);
		}
	}
	closesocket(s);//释放socket资源
	WSACleanup();//释放dll资源
	cout << "----------Chat Room Ended, See You Next Time.----------" << endl;
}

bool if_change_name(char* c) {
	char s[14];
	for(int i = 0 ; i < 14 ; i++){
		s[i] = c[i];
	}
	s[14] = '\0';
	if (strcmp(s, "change name to") == 0) {
		return true;
	}return false;
}

bool if_private(char*c) {
	char s[7];
	for(int i = 0 ; i < 7 ; i++){
		s[i] = c[i];
	}
	s[7] = '\0';
	if (strcmp(s, "send to") == 0) {
		return true;
	}return false;
}

string getNewName(char*c) {
	int len = strlen(c);
	string s = "";
	for (int i = 15; i < len ; i++) {
		s += c[i];
	}
	return s;
}
string send_to(char* c) {
	int len = strlen(c);
	string s = "";
	for (int i = 8; i < len; i++) {
		if(c[i] == ' ') return s;
		else s += c[i];
	}
}
char* getMessage(char*c) {
	int len = strlen(c);
	memset(message, 0, 100);
	int j = 0;
	int count = 0;
	for (int i = 0; i <= len; i++) {
		if (count >= 3) {
			message[j++] = c[i];
		}
		if (c[i] == '\0' || c[i] == '/t') {
			return message;
		}
		else {
			if (c[i] == ' ') { count++; }
		}
	}
}

void outTime() {
	SYSTEMTIME t;
	GetLocalTime(&t);
	cout  << t.wYear << "/" << t.wMonth << "/" << t.wDay << " " << t.wHour << ":" << t.wMinute << ":" << t.wSecond << " ";
}

DWORD WINAPI process(LPVOID p) {
	SOCKET cs = (SOCKET)p;
	//当前聊天室没有该用户，用户信息存储到容器中
	if (store_info.find(cs) == store_info.end()) {
		store_info.insert(pair<SOCKET, int>(cs, 1));
	}
	else {
		store_info[cs] = 1;
	}
	char* name = new char[100];
	strcpy(name, to_string(cs).data());
	send(cs, (const char*)name, 100, 0);
	outTime();
	cout << endl;
	cout << "----------New user " << to_string(cs).data() << " join in the chat successfully!----------" << endl;
	cout << "----------Total number : " << ++number << "----------" << endl;

	int r;
	int flag = 1;
	char* sendBuff = new char[100];
	char* recvBuff = new char[100];
	//需要server处也打印一下
	do {
		r = recv(cs, recvBuff, 100, 0);
		if (strcmp(recvBuff, "quit") == 0) {
			store_info[cs] = 0;
			if (name_change.find(to_string(cs).data()) == name_change.end()) {
				outTime();
				cout << endl;
				cout << "----------User " << to_string(cs).data() << " quits.-----------" << endl;
				cout << "----------Total number : " << --number << "----------" << endl;
				break;
			}else {
				outTime();
				cout << endl;
				cout << "----------User " << name_change[to_string(cs).data()] << " quits.----------" << endl;
				cout << "----------Total number : " << --number << "----------" << endl;
				break;
			}
		}
		if (if_change_name(recvBuff)) {
			string newName = getNewName(recvBuff);
			if(strcmp(newName.data(), to_string(cs).data()) == 0){
				cout << "New name is same as old name.Not change." << endl;
			}
			else{
				name_change[to_string(cs).data()] = newName;
				outTime();
				cout << endl;
				cout << "----------User " << to_string(cs).data() << " change his/her nickname to: " << newName<<"------------" << endl;
				continue;
			}
		}
		if (if_private(recvBuff)) {
			bool stflag = false;
			string s = send_to(recvBuff);
			for (auto it : store_info) {
				//查找更改名字的容器
				if (name_change[to_string(it.first).data()] == s) {
					strcpy(sendBuff, "Private message from User ");
					if (name_change.find(to_string(cs).data()) == name_change.end()) {
						strcat(sendBuff, to_string(cs).data());
					}
					else {
						strcat(sendBuff, name_change[to_string(cs).data()].c_str());
					}
					strcat(sendBuff, " : ");
					const char* m = getMessage(recvBuff);
					strcat(sendBuff, m);
					outTime();
					if (name_change.find(to_string(cs).data()) == name_change.end()) {
						cout << "User " << to_string(cs).data() << " : " << recvBuff << endl;
						cout << "A message has been sent privately by User " << to_string(cs).data() << " to User " << name_change[to_string(it.first).data()].c_str() << " : " << m << endl;
					}
					else {
						cout << "User " << name_change[to_string(cs).data()] << " : " << recvBuff << endl;
						cout << "A message has been sent privately by User " << name_change[to_string(cs).data()] << " to User " << name_change[to_string(it.first).data()].c_str() << " : " << m << endl;
					}
					send(it.first, sendBuff, 100, 0);
					stflag = true;
					break;
				}
				else if (to_string(it.first).data() == s) {
					strcpy(sendBuff, "Private message from User ");
					if (name_change.find(to_string(cs).data()) == name_change.end()) {
						strcat(sendBuff, to_string(cs).data());
					}
					else {
						strcat(sendBuff, name_change[to_string(cs).data()].c_str());
					}
					strcat(sendBuff, " : ");
					const char *m = getMessage(recvBuff);
					strcat(sendBuff, m);
					outTime();
					if (name_change.find(to_string(cs).data()) == name_change.end()) {
						cout << "User " << to_string(cs).data() << " : " << recvBuff << endl;
						cout << "A message has been sent privately by User " << to_string(cs).data() << " to User " << to_string(it.first).data() << " : " << m << endl;
					}
					else {
						cout << "User " << name_change[to_string(cs).data()] << " : " << recvBuff << endl;
						cout << "A message has been sent privately by User " << name_change[to_string(cs).data()] << " to User " << to_string(it.first).data() << " : " << m << endl;
					}
					send(it.first, sendBuff, 100, 0);
					stflag = true;
					break;
				}
			}
			if (stflag == false) {
				strcpy(sendBuff, "Failed");
				send(cs, sendBuff, 100, 0);
				cout<<"A private chat failed to send."<<endl;
			}
			continue;
		}
		if (r > 0) {
			strcpy(sendBuff, "User ");
			if (name_change.find(to_string(cs).data()) == name_change.end()) {
				strcat(sendBuff, to_string(cs).data());
			}
			else {
				strcat(sendBuff, name_change[to_string(cs).data()].c_str());
			}
			strcat(sendBuff," : ");
			strcat(sendBuff, (const char*)recvBuff);
			outTime();
			if (name_change.find(to_string(cs).data()) == name_change.end()) {
				cout << "User " << to_string(cs).data() << " : " << recvBuff << endl;
			}
			else {
				cout << "User " << name_change[to_string(cs).data()] << " : " << recvBuff << endl;
			}
			//socket类型的数组
			vector<SOCKET> temp;
			//遍历store_info中的元素，即所有用户
			for (auto it : store_info) {
				//找到不是当前客户端的用户并且运行状态为1
				if (it.first != cs && it.second == 1) {
					int a = send(it.first, sendBuff, 100, 0);
					if (a == SOCKET_ERROR) {
						cout << "Send to " << it.first << "failed." << endl;
						cout << "Move User " << it.first << " away." << endl;
						temp.push_back(cs);
					}
				}
			}
			for (int i = 0; i < temp.size(); i++) {
				store_info.erase(temp[i]);
			}
		}
		else{
			flag = 0;
		}
	} while (r != SOCKET_ERROR && flag != 0);
	//store_info[cs] = 0;
	return 0;
}

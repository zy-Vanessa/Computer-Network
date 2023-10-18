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
char* ipaddress = new char[100]; //ip��ַ
int port;//�˿ں�
char message[100];
int number = 0; //����
DWORD WINAPI process(LPVOID p);//�߳�ִ�к���
map<SOCKET, int>store_info;
map<string, string>name_change;
int main() {
	// Declare and initialize variables
	WSADATA wsaData;
	struct hostent* remoteHost = NULL;
	char serverHostName[128] = {};

	// Initialize Winsock
	//����һ��ϵͳ��ʼ��
	int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (r != 0)
	{
		printf("WSAStartup failed: %d\n", r);
		return 0;
	}
	printf("WSAStartup succeed!\n");
	//gethostname����wind socket
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
	//����һ��socket
	SOCKET s;
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//��ʼ�� IPV-4���²�Э��ѡ��������������Ҫ��ѡ��TCPЭ��
	if (s == INVALID_SOCKET) {
		cout << "Create socket failed." << endl;
		WSACleanup();//�ͷ�dll��Դ
		return 0;
	}
	else {
		cout << "Create socket succeed." << endl;
	}
	//���׽��֣���һ��ָ����ַ�󶨵�ָ����Socket
	sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = PF_INET;
	ipaddress = "127.0.0.1";
	port = 8000; //������Χ Ĭ��8000
	sa.sin_addr.s_addr = inet_addr(ipaddress);  //��S_un.S_addr���и�ֵ
	sa.sin_port = htons(port);  //�˿�
	cout << "server IP: " << ipaddress << ":" << port << endl;

	//��
	r = bind(s, (SOCKADDR*)&sa, sizeof(SOCKADDR));//ִ�а�
	if (r == 0) {
		cout << "bind succeeed." << endl;
	}
	else {
		closesocket(s);//�ͷ�socket��Դ
		WSACleanup();//�ͷ�dll��Դ
		cout << "bind failed." << endl;
		return 0;
	}

	//�������״̬
	if (listen(s, 10) == 0) {//������Ϊ10
		cout << "----------Initialized chat room successfully with up to 10 people----------" << endl;
		cout << "listen secceed." << endl;
	}
	else {
		cout << "listen failed." << endl;
		closesocket(s);//�ͷ�socket��Դ
		WSACleanup();//�ͷ�dll��Դ
		return 0;
	}
	cout << "----------Waiting for Client to Join----------" << endl;

	//����ɹ����ܵĿͻ��ˣ�Ϊ���Ƿ�����Դ
	while (1) {
		int size = sizeof(sockaddr_in);
		sockaddr_in clientaddr;
		SOCKET cs;
		cs = accept(s, (SOCKADDR*)&clientaddr, &size);
		if (cs == INVALID_SOCKET) {
			cout << "Connection failed." << endl;
			closesocket(s);//�ͷ�socket��Դ
			WSACleanup();//�ͷ�dll��Դ
			return 0;
		}
		else{
			//HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, 
			//LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
			//�߳����ԡ��̶߳�ջ��С���߳�ִ�к����������̲߳����������̲߳��������߳�ID
			//LPVOID��һ��û�����͵�ָ�룬���Խ��������͵�ָ�븳ֵ��LPVOID���͵ı�����һ����Ϊ�������ݣ���Ȼ����ʹ�õ�ʱ����ת������
			HANDLE cthread = CreateThread(NULL, 0, process, (LPVOID)cs, 0, NULL);
			CloseHandle(cthread);
		}
	}
	closesocket(s);//�ͷ�socket��Դ
	WSACleanup();//�ͷ�dll��Դ
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
	//��ǰ������û�и��û����û���Ϣ�洢��������
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
	//��Ҫserver��Ҳ��ӡһ��
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
				//���Ҹ������ֵ�����
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
			//socket���͵�����
			vector<SOCKET> temp;
			//����store_info�е�Ԫ�أ��������û�
			for (auto it : store_info) {
				//�ҵ����ǵ�ǰ�ͻ��˵��û���������״̬Ϊ1
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

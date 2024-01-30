#include <iostream>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fstream>
#pragma comment (lib, "ws2_32.lib")
using namespace std;
//设置参数
#define MAX_WAIT_TIME  5000 //等待超时时限 5s
#define MAX_SEND_TIMES  1000 //发送超时时限 
#define MaxFileSize 200000000 //最大文件大小
#define MaxMsgSize 10000 //最大报文数据大小
const unsigned short SYN = 0x1;
const unsigned short ACK = 0x2;
const unsigned short FIN = 0x4;
const unsigned short RST = 0x8;
#pragma pack(1)//让编译器将结构体数据强制连续排列，禁止优化存储结构进行对齐
struct Message
{
	//校验和
	unsigned short checkNum;//2字节
	//序号 Seq num
	unsigned int SeqNum;//4字节
	//确认号 Ack num
	unsigned int AckNum;//4字节
	//标志
	unsigned short flag;//2字节
	//数据大小
	unsigned int size;//4字节
	//源IP、目的IP
	unsigned int SrcIP, DestIP;//4字节、4字节
	//源端口号、目的端口号
	unsigned short SrcPort, DestPort;//2字节、2字节
	
	BYTE msgData[MaxMsgSize];
	Message();
	bool checksum();
	void setchecksum();
};

#pragma pack()

Message::Message()
{
	SrcIP = 0;
	DestIP = 0;
	SeqNum = 0;
	AckNum = 0;
	size = 0;
	flag = 0;
	memset(&msgData, 0, sizeof(msgData));
}

void Message::setchecksum()
{
	this->checkNum = 0;
	int sum = 0;
	unsigned short* msgStream = (unsigned short*)this; //按16位字（两个字节）进行迭代
	for (int i = 0; i < sizeof(*this) / 2; i++)
	{
		sum += *msgStream++;
		if (sum & 0xFFFF0000) //如果 sum 的高16位不为0，则将 sum 的高16位清零，低16位加1
		{
			sum &= 0xFFFF;
			sum++;
		}
	}
	this->checkNum = ~(sum & 0xFFFF); ///将 checkNum 设置为 sum 取反后的低16位。
}

bool Message::checksum()
{
	unsigned int sum = 0;
	unsigned short* msgStream = (unsigned short*)this; //按16位字（两个字节）进行迭代
	for (int i = 0; i < sizeof(*this) / 2; i++)
	{
		sum += *msgStream++;//将当前16位字的值加到 sum 中
		if (sum & 0xFFFF0000) //如果 sum 的高16位不为0，则将 sum 的高16位清零，低16位加1
		{
			sum &= 0xFFFF;
			sum++;
		}
	}
	if ((sum & 0xFFFF) == 0xFFFF) //如果 sum 的低16位全为1，则返回true，表示校验通过
	{
		return true;
	}
	return false;
}
// 定义颜色代码
enum ConsoleColor
{
	Red = FOREGROUND_RED,
	Green = FOREGROUND_GREEN,
	Blue = FOREGROUND_BLUE,
	Yellow = FOREGROUND_RED | FOREGROUND_GREEN,
	White = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
};
int ServerPORT; //server端口号
int RouterPORT; //路由器端口号
int seq = 0;//全局变量序列号
// 函数用于设置控制台文本颜色
void SetTextColor(ConsoleColor color)
{
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(consoleHandle, static_cast<WORD>(color));
}
//实现server（接收端）的三次握手
bool Connect(SOCKET serverSocket, SOCKADDR_IN routerAddr)
{
	int AddrLen = sizeof(routerAddr);
	Message buffer1;
	Message buffer2;
	Message buffer3;
	while (1)
	{
		//接收第一次握手的消息（SYN=1，seq=x）
		int r = recvfrom(serverSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "连接失败,关闭连接！" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//判断SYN、检验和
			if (!(buffer1.flag && SYN) || !buffer1.checksum() || !(buffer1.SeqNum == seq + 1))
			{
				SetTextColor(Red);
				cout << "连接发生错误！" << endl;
				SetTextColor(White);
				return false;
			}
			seq++;
			SetTextColor(Blue);
			cout << "[1]:服务器端已收到第一次握手的消息！" << endl;
			SetTextColor(White);
			//发送第二次握手的消息（SYN=1，ACK=1，ack=x）
			buffer2.SrcPort = ServerPORT;
			buffer2.DestPort = RouterPORT;
			buffer2.AckNum = buffer1.SeqNum;//服务器回复的ack=客户端发来的seq
			buffer2.flag += SYN;
			buffer2.flag += ACK;
			buffer2.setchecksum();//设置校验和
			int s = sendto(serverSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, AddrLen);
			clock_t buffer2start = clock();
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "连接失败,关闭连接！" << endl;
				SetTextColor(White);
				return false;
			}
			SetTextColor(Blue);
			cout << "[2]:服务器端已发送第二次握手的消息！" << endl;
			SetTextColor(White);
			//接收第三次握手的消息（ACK=1，seq=x+1）
			while (1)
			{
				int r = recvfrom(serverSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, &AddrLen);
				if (r == 0)
				{
					SetTextColor(Red);
					cout << "连接失败,关闭连接！" << endl;
					SetTextColor(White);
					return false;
				}
				else if (r > 0)
				{
					//成功收到消息，检查校验和、seq
					if ((buffer3.flag && ACK) && buffer3.checksum() && (buffer3.SeqNum == seq + 1))
					{
						seq++;
						SetTextColor(Blue);
						cout << "[3]:服务器端已收到第三次握手的消息！" << endl;
						cout << "服务器端连接成功！" << endl;
						SetTextColor(White);
						return true;
					}
					else
					{
						SetTextColor(Red);
						cout << "连接发生错误！" << endl;
						SetTextColor(White);
						return false;
					}
				}
				//buffer2超时，重新发送并重新计时
				if (clock() - buffer2start > MAX_WAIT_TIME)
				{
					SetTextColor(Red);
					cout << "第二次握手超时，正在重传......" << endl;
					SetTextColor(White);
					int s = sendto(serverSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, AddrLen);
					buffer2start = clock(); //重新设置buffer2的时间
					if (s == 0)
					{
						SetTextColor(Red);
						cout << "连接失败,关闭连接！" << endl;
						SetTextColor(White);
						return false;
					}
				}
			}
		}
	}
	return false;
}
//实现server（接收端）的四次挥手
bool Disconnect(SOCKET serverSocket, SOCKADDR_IN routerAddr)
{
	int AddrLen = sizeof(routerAddr);
	Message buffer1;
	Message buffer2;
	Message buffer3;
	Message buffer4;
	while (1)
	{
		//接收第一次挥手的消息（FIN=1，seq=y）
		int r = recvfrom(serverSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "连接失败,关闭连接！" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//判断SYN、检验和
			if (!(buffer1.flag && FIN) || !buffer1.checksum() || !(buffer1.SeqNum == seq + 1))
			{
				SetTextColor(Red);
				cout << "连接发生错误！" << endl;
				SetTextColor(White);
				return false;
			}
			seq++;
			SetTextColor(Blue);
			cout << "[1]:服务端已收到第一次挥手的消息！" << endl;
			SetTextColor(White);
			//发送第二次挥手的消息（ACK=1，ack=y）
			buffer2.SrcPort = ServerPORT;
			buffer2.DestPort = RouterPORT;
			buffer2.AckNum = buffer1.SeqNum;
			buffer2.flag += ACK;
			buffer2.setchecksum();//设置校验和
			int s = sendto(serverSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, AddrLen);
			clock_t buffer2start = clock();
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "连接失败......关闭连接！" << endl;
				SetTextColor(White);
				return false;
			}
			SetTextColor(Blue);
			cout << "[2]:服务器端已发送第二次挥手的消息！" << endl;
			SetTextColor(White);
			break;
		}
	}
	//发送第三次挥手的消息（FIN=1，ACK=1，seq=z）
	buffer3.SrcPort = ServerPORT;
	buffer3.DestPort = RouterPORT;
	buffer3.flag += FIN;//设置FIN
	buffer3.flag += ACK;//设置ACK
	buffer3.SeqNum = seq++;//设置序号seq
	buffer3.setchecksum();//设置校验和
	int s = sendto(serverSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, AddrLen);
	clock_t buffer3start = clock();
	if (s == 0)
	{
		SetTextColor(Red);
		cout << "连接失败,关闭连接！" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout << "[3]:服务器端已发送第三次挥手的消息！" << endl;
	SetTextColor(White);

	//接收第四次挥手的消息（ACK=1，ack=z）
	while (1)
	{
		int r = recvfrom(serverSocket, (char*)&buffer4, sizeof(buffer4), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "关闭连接错误！" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//成功收到消息，检查校验和、ACK、ack
			if ((buffer4.flag && ACK) && buffer4.checksum() && (buffer4.AckNum == buffer3.SeqNum))
			{
				SetTextColor(Blue);
				cout << "[4]:服务器端已收到第四次挥手的消息！" << endl;
				SetTextColor(White);
				break;
			}
			else
			{
				SetTextColor(Red);
				cout << "连接发生错误！" << endl;
				SetTextColor(White);
				return false;
			}
		}
		//buffer3超时，重新发送并重新计时
		if (clock() - buffer3start > MAX_WAIT_TIME)
		{
			SetTextColor(Red);
			cout << "第三次挥手超时，正在重传......" << endl;
			SetTextColor(White);
			int s = sendto(serverSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, AddrLen);
			buffer3start = clock();
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "关闭连接失败！" << endl;
				SetTextColor(White);
				return false;
			}
		}
	}
	SetTextColor(Blue);
	cout << "关闭连接成功！" << endl;
	SetTextColor(White);
	return true;
}
//接收报文段
bool recvMessage(Message& recvMsg, SOCKET serverSocket, SOCKADDR_IN routerAddr, int& expectedSeq)
{
	int AddrLen = sizeof(routerAddr);
	while (1)
	{
		int r = recvfrom(serverSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r > 0)
		{
			//成功收到消息检查校验和、序列号
			if (recvMsg.checksum() && (recvMsg.SeqNum == expectedSeq))
			{
				//回复ACK
				Message reply;
				reply.SrcPort = ServerPORT;//原端口
				reply.DestPort = RouterPORT;//目的端口
				reply.flag += ACK;//设置ACK=1
				reply.AckNum = recvMsg.SeqNum;//ack=seq
				reply.setchecksum();//设置校验和
				sendto(serverSocket, (char*)&reply, sizeof(reply), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
				cout << "[recv]: " << "Seq = " << recvMsg.SeqNum << "，checksum =" << recvMsg.checkNum <<  endl << "[send]: Ack = " << reply.AckNum << endl;
				expectedSeq++;
				return true;
			}
			//如果seq！= 期待值，则返回累计确认的ack（expectedSeq-1）
			else if (recvMsg.checksum() && (recvMsg.SeqNum != expectedSeq))
			{
				//回复ACK
				Message reply;
				reply.SrcPort = ServerPORT;
				reply.DestPort = RouterPORT;
				reply.flag += ACK;
				reply.AckNum = expectedSeq - 1;//ack=期待值减一
				reply.setchecksum();
				sendto(serverSocket, (char*)&reply, sizeof(reply), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
				cout << "[GBN recv]: Seq = " << recvMsg.SeqNum << endl << "[GBN send]: Ack = " << reply.AckNum << endl;
			}
		}
		else if (r == 0)
		{
			return false;
		}
	}
}
//接收文件
void serverRecvFile(SOCKET serverSocket, SOCKADDR_IN routerAddr)
{
	int expectedSeq = 0;
	int AddrLen = sizeof(routerAddr);
	//接收文件名和文件大小
	Message nameMessage;
	unsigned int fileSize;//文件大小
	char fileName[50] = { 0 };//文件名
	while (1)
	{
		int r = recvfrom(serverSocket, (char*)&nameMessage, sizeof(nameMessage), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r > 0)
		{
			//成功收到消息且为expectedSeq
			if (nameMessage.checksum() && (nameMessage.SeqNum == expectedSeq))
			{
				fileSize = nameMessage.size;//获取文件大小
				for (int i = 0; nameMessage.msgData[i]; i++)//获取文件名
					fileName[i] = nameMessage.msgData[i];
				SetTextColor(Blue);
				cout << "接收文件名为：" << fileName << "，大小为：" << fileSize << endl << endl;
				SetTextColor(White);
				//回复ACK
				Message reply;
				reply.SrcPort = ServerPORT;
				reply.DestPort = RouterPORT;
				reply.flag += ACK;
				reply.AckNum = nameMessage.SeqNum;//确认expectedSeq
				reply.setchecksum();
				sendto(serverSocket, (char*)&reply, sizeof(reply), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
				cout << "[recv]: Seq = " << nameMessage.SeqNum << endl << "[send]: Ack = " << reply.AckNum << endl;
				expectedSeq++;
				break;
			}
			//如果seq！= 期待值，则返回累计确认的ack（expectedSeq-1）
			else if (nameMessage.checksum() && (nameMessage.SeqNum != expectedSeq))
			{
				//回复ACK
				Message reply;
				reply.SrcPort = ServerPORT;
				reply.DestPort = RouterPORT;
				reply.flag += ACK;
				reply.AckNum = expectedSeq - 1;//累计确认
				reply.setchecksum();
				sendto(serverSocket, (char*)&reply, sizeof(reply), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
				cout << "[GBN recv]: Seq = " << nameMessage.SeqNum << endl << "[send]: Ack = " << reply.AckNum << endl;
			}
		}
	}

	//接收数据段
	int batchNum = fileSize / MaxMsgSize;//全装满的报文个数
	int leftSize = fileSize % MaxMsgSize;//不能装满的剩余报文大小
	BYTE* fileBuffer = new BYTE[fileSize];
	SetTextColor(Blue);
	cout << "开始接收数据段，共 " << batchNum << " 个最大装载报文段" << endl;
	SetTextColor(White);
	for (int i = 0; i < batchNum; i++)
	{
		Message dataMsg;
		if (recvMessage(dataMsg, serverSocket, routerAddr, expectedSeq))
		{
			cout << "数据报" << dataMsg.SeqNum << "接收成功" << endl;
		}
		else
		{
			SetTextColor(Red);
			cout << "数据接收失败！" << endl;
			SetTextColor(White);
			return;
		}
		//读取数据部分
		for (int j = 0; j < MaxMsgSize; j++)
		{
			fileBuffer[i * MaxMsgSize + j] = dataMsg.msgData[j];
		}
	}
	//剩余部分
	if (leftSize > 0)
	{
		Message dataMsg;
		if (recvMessage(dataMsg, serverSocket, routerAddr, expectedSeq))
		{
			cout << "数据报" << dataMsg.SeqNum << "接收成功" << endl;
		}
		else
		{
			cout << "数据接收失败！" << endl;
			return;
		}
		for (int j = 0; j < leftSize; j++)
		{
			fileBuffer[batchNum * MaxMsgSize + j] = dataMsg.msgData[j];
		}
	}

	cout << "文件传输成功，正在写入文件......" << endl;
	SetTextColor(White);
	//写入文件
	FILE* outFile;
	outFile = fopen(fileName, "wb");
	if (fileBuffer != 0)
	{
		fwrite(fileBuffer, fileSize, 1, outFile);
		fclose(outFile);
		SetTextColor(Blue);
		cout << "文件写入成功！" << endl;
		SetTextColor(White);
	}
	return;
}


int main()
{
	//初始化socket 
	WSAData wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata); //MAKEWORD(主版本号, 副版本号)
	string ip1, ip2;
	const char* IP1=0, *IP2=0;
	string addr = "127.0.0.1";
	cout << "请输入服务端IP地址（输入0默认本机地址）:";
	cin >> ip1;
	if(ip1 == "0")
		IP1 = addr.c_str();
	else
		IP1 = ip1.c_str();
	cout << "请输入服务端端口号：";
	cin >> ServerPORT;
	cout << "请输入路由器端IP地址（输入0默认本机地址）:";
	cin >> ip2;
	if(ip2 == "0")
		IP2 = addr.c_str();
	else
		IP2 = ip2.c_str();
	cout << "请输入路由器端端口号：";
	cin >> RouterPORT;
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2)
	{
		SetTextColor(Red);
		cout << "初始化Socket失败！" << endl;
		SetTextColor(White);
		return -1;
	}
	SetTextColor(Blue);
	cout << "初始化Socket成功!" << endl;
	SetTextColor(White);
	//创建socket
	SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
	unsigned long on = 1;
	ioctlsocket(serverSocket, FIONBIO, &on);//设置非阻塞
	if (serverSocket == INVALID_SOCKET)
	{
		SetTextColor(Red);
		cout << "创建socket失败！" << endl;
		SetTextColor(White);
		return -1;
	}
	SetTextColor(Blue);
	cout << "创建socket成功！" << endl;
	SetTextColor(White);

	//初始化服务器地址
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET; //地址类型
	serverAddr.sin_addr.S_un.S_addr = inet_addr(IP1); //地址
	serverAddr.sin_port = htons(ServerPORT); //端口号
	//bind
	int tem = bind(serverSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr));
	if (tem == SOCKET_ERROR)
	{
		SetTextColor(Red);
		cout << "连接失败！" << endl;
		SetTextColor(White);
		return -1;
	}
	else
	{
		SetTextColor(Blue);
		cout << "连接到端口 " << ServerPORT << "：成功！" << endl;
		SetTextColor(White);
	}
	SetTextColor(Blue);
	cout << "服务器端就绪，等待客户端连接……" << endl << endl;
	SetTextColor(White);

	//初始化路由器地址
	SOCKADDR_IN routerAddr;
	routerAddr.sin_family = AF_INET; //地址类型
	routerAddr.sin_addr.S_un.S_addr = inet_addr(IP2); //地址
	routerAddr.sin_port = htons(RouterPORT); //端口号
	//建立连接
	bool isConn = Connect(serverSocket, routerAddr);
	if (isConn == 0)
		return -1;
	//传输文件
	serverRecvFile(serverSocket, routerAddr);
	//关闭连接
	Disconnect(serverSocket, routerAddr);
	closesocket(serverSocket); //关闭socket
	WSACleanup();
	system("pause");
	return 0;
}

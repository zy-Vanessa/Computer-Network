#include <iostream>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <time.h>
#include <fstream>
#include <windows.h>
#include <queue>
#include <mutex>
#pragma comment (lib, "ws2_32.lib")
using namespace std;
//设置参数
#define MAX_WAIT_TIME  5000 //超时时限 5s
#define MAX_SEND_TIMES  1000 //超时时限 
#define MaxFileSize 200000000 //最大文件大小
#define MaxMsgSize 10000 //最大报文数据大小
const unsigned short SYN = 0x1;
const unsigned short ACK = 0x2;
const unsigned short FIN = 0x4;
const unsigned short RST = 0x8;
#pragma pack(1)//让编译器将结构体数据强制连续排列，禁止优化存储结构进行对齐
struct sendwindowstruct//发送窗口
{
	int size;//窗口大小=7
	int base;//窗口起始序列号
	int nextseqnum;//下一个将发送的序列号
	sendwindowstruct();
};
sendwindowstruct::sendwindowstruct()
{
	size = 30;
	base = 0;
	nextseqnum = 0;
}
//发送报文段
struct parameters {
	SOCKADDR_IN routerAddr;
	SOCKET clientSocket;
	int msgSum;//消息数量
};
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
#define Stream std::basic_ostream<T1, T2>&
std::mutex outputMutex;
//全局变量window
sendwindowstruct sendwindow;
int RouterPORT; //路由器端口号
int ClientPORT; //客户端端口号
int seq = 0;//全局变量 数据包序列号
int msgStart;
bool over = 0;
bool sendAgain = 0;
// 函数用于设置控制台文本颜色
void SetTextColor(ConsoleColor color)
{
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(consoleHandle, static_cast<WORD>(color));
}
//实现client（发送端）的三次握手
bool Connect(SOCKET clientSocket, SOCKADDR_IN routerAddr)
{
	int AddrLen = sizeof(routerAddr);
	Message buffer1;
	Message buffer2;
	Message buffer3;
	//发送第一次握手的消息（SYN=1，seq=x）
	buffer1.SrcPort = ClientPORT;//原端口客户端端口
	buffer1.DestPort = RouterPORT;//目的端口为路由器端口
	buffer1.flag += SYN;//设置SYN=1
	buffer1.SeqNum = ++seq;//设置序号seq
	buffer1.setchecksum();//设置校验和
	int s = sendto(clientSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, AddrLen);
	//发送第一次握手消息
	clock_t buffer1start = clock();//开始计时
	if (s == 0)//发送失败
	{
		SetTextColor(Red);
		cout << "连接失败，关闭连接！" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout <<"[1]:客户端已发送第一次握手的消息！" << endl;
	SetTextColor(White);
	//接收第二次握手的消息（SYN=1，ACK=1，ack=x）
	while (1)
	{
		int r = recvfrom(clientSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "连接失败，关闭连接！" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//成功收到消息，检查校验和、ACK、SYN、ack
			if ((buffer2.flag && ACK) && (buffer2.flag && SYN) && buffer2.checksum() && (buffer2.AckNum == buffer1.SeqNum))
			{
				SetTextColor(Blue);
				cout << "[2]:客户端已收到第二次握手的消息！" << endl;
				SetTextColor(White);
				break;//只有接收到正确的第二次握手消息才break退出循环
			}
			else
			{
				SetTextColor(Red);
				cout << "连接发生错误！" << endl;
				SetTextColor(White);
				return false;
			}
		}
		//buffer1超时，重新发送并重新计时
		if (clock() - buffer1start > MAX_WAIT_TIME)
		{
			SetTextColor(Red);
			cout << "第一次握手超时，正在重传......" << endl;
			SetTextColor(White);
			int s = sendto(clientSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, AddrLen);
			buffer1start = clock();//重新计时
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "连接失败，关闭连接！" << endl;
				SetTextColor(White);
				return false;
			}
		}//重传后仍在while循环中，继续等待接收
	}
	//发送第三次握手的消息（ACK=1，seq=x+1）
	buffer3.SrcPort = ClientPORT;//原端口
	buffer3.DestPort = RouterPORT;//目的端口
	buffer3.flag += ACK;//设置ACK
	buffer3.SeqNum = ++seq;//设置序号seq=x+1
	buffer3.setchecksum();//设置校验和
	s = sendto(clientSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, AddrLen);
	if (s == 0)
	{
		SetTextColor(Red);
		cout << "连接失败，关闭连接！" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout << "[3]:客户端已发送第三次握手的消息！" << endl;
	cout << "客户端连接成功！" << endl;
	SetTextColor(White);
}

//实现client（发送端）的四次挥手
bool Disconnect(SOCKET clientSocket, SOCKADDR_IN routerAddr)
{
	int AddrLen = sizeof(routerAddr);
	Message buffer1;
	Message buffer2;
	Message buffer3;
	Message buffer4;

	//发送第一次挥手的消息（FIN=1，seq=y）
	buffer1.SrcPort = ClientPORT;
	buffer1.DestPort = RouterPORT;
	buffer1.flag += FIN;//设置FIN
	buffer1.SeqNum = ++seq;//设置序号seq
	buffer1.setchecksum();//设置校验和
	int s = sendto(clientSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, AddrLen);
	clock_t buffer1start = clock();
	if (s == 0)
	{
		SetTextColor(Red);
		cout << "连接失败，关闭连接！" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout << "[1]:客户端已发送第一次挥手的消息！" << endl;
	SetTextColor(White);

	//接收第二次挥手的消息（ACK=1，ack=y）
	while (1)
	{
		int r = recvfrom(clientSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "关闭连接失败！" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//成功收到消息，检查校验和、ACK、ack
			if ((buffer2.flag && ACK) && buffer2.checksum() && (buffer2.AckNum == buffer1.SeqNum))
			{
				SetTextColor(Blue);
				cout << "[2]:客户端已收到第二次挥手的消息！" << endl;
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
		//buffer1超时，重新发送并重新计时
		if (clock() - buffer1start > MAX_WAIT_TIME)
		{
			SetTextColor(Red);
			cout << "第一次挥手超时，正在重传......" << endl;
			SetTextColor(White);
			int s = sendto(clientSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, AddrLen);
			buffer1start = clock();
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "关闭连接失败！" << endl;
				SetTextColor(White);
				return false;
			}
		}
	}
	//接收第三次挥手的消息（FIN=1，ACK=1，seq=z）
	while (1)
	{
		int r = recvfrom(clientSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "关闭连接失败！" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//成功收到消息，检查校验和、ACK、ack
			if ((buffer3.flag && ACK) && (buffer3.flag && FIN) && buffer3.checksum())
			{
				SetTextColor(Blue);
				cout << "[3]:客户端已收到第三次挥手的消息！" << endl;
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
	}
	//发送第四次挥手的消息（ACK=1，ack=z）
	buffer4.SrcPort = ClientPORT;
	buffer4.DestPort = RouterPORT;
	buffer4.flag += ACK;//设置ACK
	buffer4.AckNum = buffer3.SeqNum;//设置序号seq
	buffer4.setchecksum();//设置校验和
	s = sendto(clientSocket, (char*)&buffer4, sizeof(buffer4), 0, (sockaddr*)&routerAddr, AddrLen);
	if (s == 0)
	{
		SetTextColor(Red);
		cout <<"关闭连接失败！" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout << "[4]:客户端已发送第四次挥手的消息！" << endl;
	SetTextColor(White);
	//第四次挥手之后还需等待2MSL，防止最后一个ACK丢失，处于半关闭
	int tempclock = clock();
	SetTextColor(Red);
	cout << "client端2MSL等待..." << endl;
	SetTextColor(White);
	Message tmp;
	while (clock() - tempclock < 2 * MAX_WAIT_TIME)
	{
		int r = recvfrom(clientSocket, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout <<"关闭连接错误！" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			s = sendto(clientSocket, (char*)&buffer4, sizeof(buffer4), 0, (sockaddr*)&routerAddr, AddrLen);
			cout << "回复" << endl;
		}
	}
	SetTextColor(Blue);
	cout << "关闭连接成功！" << endl;
	SetTextColor(White);
}
//接收ack的线程
DWORD WINAPI recvThread(PVOID pParam)
{
	parameters* para = (parameters*)pParam;//将pParam转换为parameters结构体指针
	SOCKADDR_IN routerAddr = para->routerAddr;
	SOCKET clientSocket = para->clientSocket;
	int msgSum = para->msgSum; //消息的数量
	int AddrLen = sizeof(routerAddr);

	int wrongACK = -1;//失序的ACK变量
	int wrongCount = 0;//错误计数器
	while (1)//等待接收
	{
		//rdt_rcv
		Message recvMsg;
		int r = recvfrom(clientSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r > 0)
		{
			if (recvMsg.checksum())//检查校验和
			{
				if (recvMsg.AckNum >= sendwindow.base)//确认号ack大于等于窗口左侧
					sendwindow.base = recvMsg.AckNum + 1;//窗口右移，base指向等待确认的数据包
				msgStart = clock();//开始计时
				if (sendwindow.base != sendwindow.nextseqnum)//base与nextseqnum不重合
				{
					std::lock_guard<std::mutex>lock(outputMutex);
					cout << "[recv]: Ack = " << recvMsg.AckNum << endl;
				}
				//打印窗口情况
				{
					std::lock_guard<std::mutex>lock(outputMutex);
					SetTextColor(Red);
					cout << "[当前窗口情况]： 窗口大小：" << sendwindow.size << "，已发送但未收到ACK：" << sendwindow.nextseqnum - sendwindow.base
						<< "，尚未发送：" << sendwindow.size - (sendwindow.nextseqnum - sendwindow.base) << endl;
					SetTextColor(White);
				}
				//判断结束的情况
				if (recvMsg.AckNum == msgSum - 1)//最后一个数据包的ack已接收到
				{
					SetTextColor(Blue);
					cout << "已接收到最后一个分组的ACK,传输完成！" << endl;
					SetTextColor(White);
					over = 1;
					return 0;
				}
				//快速重传
				if (wrongACK != recvMsg.AckNum)
				{//wrongACK等于上一次接收到的累积确认的ack,那么这次收到的ACK一定大于wrongACK,
					//不可能小于，如果等于则说明数据包丢失
					wrongCount = 0;
					wrongACK = recvMsg.AckNum;
				}
				else
				{
					wrongCount++;
				}
				if (wrongCount == 3)
				{
					//重发
					sendAgain = 1;
				}
			}
			//若校验失败或ack不对，则忽略，继续等待
		}
	}
	return 0;
}

void sendThread(string filename, SOCKADDR_IN routerAddr, SOCKET clientSocket)
{
	int startTime = clock();
	//截取文件名
	string realname = "";
	for (int i = filename.size() - 1; i >= 0; i--)
	{
		if (filename[i] == '/' || filename[i] == '\\')
			break;//去掉文件名开头的空格或换行符
		realname += filename[i];
	}
	realname = string(realname.rbegin(), realname.rend());//将倒序的文件变回正序
   // 打开文件，读成字节流 
	ifstream fin(filename.c_str(), ifstream::binary);
	if (!fin) {
		printf("无法打开文件！\n");
		return;
	}
	//文件读取到fileBuffer
	// 创建fileBuffer的BYTE数组，用于存储文件内容，数组大小为 MaxFileSize
	BYTE* fileBuffer = new BYTE[MaxFileSize];
	unsigned int fileSize = 0;
	// 从输入流 fin 中获取一个字节，并存储到变量 byte 中
	BYTE byte = fin.get();
	// 循环读取文件内容，直到文件流 fin 的状态变为 false（文件结束）
	while (fin) {
		// 将读取到的字节存储到 fileBuffer 数组中，并更新文件大小 fileSize
		fileBuffer[fileSize++] = byte;
		// 继续从输入流 fin 中获取下一个字节
		byte = fin.get();
	}
	fin.close();
	int batchNum = fileSize / MaxMsgSize;//全装满的报文个数
	int leftSize = fileSize % MaxMsgSize;//不能装满的剩余报文大小
	// 创建接受消息线程
	int msgSum = leftSize > 0 ? batchNum + 2 : batchNum + 1;
	//判断消息总数，除了整个文件外，另加一个说明文件名和文件大小的数据包
	parameters param;
	param.routerAddr = routerAddr;
	param.clientSocket = clientSocket;
	param.msgSum = msgSum;
	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recvThread, &param, 0, 0); //创建线程，用于接收ack

	while (1)
	{
		//rdt_send(data)
		if (sendwindow.nextseqnum < sendwindow.base + sendwindow.size && sendwindow.nextseqnum < msgSum)//将要发送的数据号在窗口内，并且不是最后一个序列号
		{
			//make_pkt
			Message sendMsg;
			if (sendwindow.nextseqnum == 0)//第一个分组，说明文件名和文件大小的消息
			{
				sendMsg.SrcPort = ClientPORT;//原端口
				sendMsg.DestPort = RouterPORT;//目的端口
				sendMsg.size = fileSize;//文件大小
				sendMsg.flag += RST;//文件名
				sendMsg.SeqNum = sendwindow.nextseqnum;
				for (int i = 0; i < realname.size(); i++)//填充报文数据段
					sendMsg.msgData[i] = realname[i];//文件名写入Message的数据部分
				sendMsg.msgData[realname.size()] = '\0';//字符串结尾补\0
				sendMsg.setchecksum();//设置校验和
			}
			else if (sendwindow.nextseqnum == batchNum + 1 && leftSize > 0)//发送最后一个包
			{
				sendMsg.SrcPort = ClientPORT;//源端口
				sendMsg.DestPort = RouterPORT;//目的端口
				sendMsg.SeqNum = sendwindow.nextseqnum;//发送序列号
				for (int j = 0; j < leftSize; j++)
				{
					sendMsg.msgData[j] = fileBuffer[batchNum * MaxMsgSize + j];//写入文件数据
				}
				sendMsg.setchecksum();//设置校验和
			}
			else//最大装载的数据包
			{
				sendMsg.SrcPort = ClientPORT;//原端口
				sendMsg.DestPort = RouterPORT;//目的端口
				sendMsg.SeqNum = sendwindow.nextseqnum;//发送序列号
				for (int j = 0; j < MaxMsgSize; j++)
				{//写入文件数据
					sendMsg.msgData[j] = fileBuffer[(sendwindow.nextseqnum - 1) * MaxMsgSize + j];
				}
				sendMsg.setchecksum();//设置校验和
			}
			sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
			{
				std::lock_guard<std::mutex>lock(outputMutex);
				cout << "[send]: Seq = " << sendMsg.SeqNum << "，checksum =" << sendMsg.checkNum << endl;
			}

			if (sendwindow.base == sendwindow.nextseqnum)
			{
				msgStart = clock();
			}
			sendwindow.nextseqnum++;
			//发送报文段，没有接受ACK窗口不滑动
		}
		//超时
		if (clock() - msgStart > MAX_WAIT_TIME || sendAgain)//超时或ack重复3次
		{
			if (sendAgain) {
				{
					std::lock_guard<std::mutex>lock(outputMutex);
					SetTextColor(Red);
					cout <<  "连续收到三次重复ACK，快速重传......" << endl;
					SetTextColor(White);
				}
			}
			//重发当前缓冲区的message
			Message sendMsg;
			for (int i = 0; i < sendwindow.nextseqnum - sendwindow.base; i++)
			{
				int sendnum = sendwindow.base + i;
				if (sendnum == 0)
				{
					sendMsg.SrcPort = ClientPORT;
					sendMsg.DestPort = RouterPORT;
					sendMsg.size = fileSize;
					sendMsg.flag += RST;
					sendMsg.SeqNum = sendnum;
					for (int i = 0; i < realname.size(); i++)//填充报文数据段
						sendMsg.msgData[i] = realname[i];
					sendMsg.msgData[realname.size()] = '\0';//字符串结尾补\0
					sendMsg.setchecksum();
				}
				else if (sendnum == batchNum + 1 && leftSize > 0)
				{
					sendMsg.SrcPort = ClientPORT;
					sendMsg.DestPort = RouterPORT;
					sendMsg.SeqNum = sendnum;
					for (int j = 0; j < leftSize; j++)
					{
						sendMsg.msgData[j] = fileBuffer[batchNum * MaxMsgSize + j];
					}
					sendMsg.setchecksum();
				}
				else
				{
					sendMsg.SrcPort = ClientPORT;
					sendMsg.DestPort = RouterPORT;
					sendMsg.SeqNum = sendnum;
					for (int j = 0; j < MaxMsgSize; j++)
					{
						sendMsg.msgData[j] = fileBuffer[(sendnum - 1) * MaxMsgSize + j];
					}
					sendMsg.setchecksum();
				}

				sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
				{
					std::lock_guard<std::mutex>lock(outputMutex);
					SetTextColor(Red);
					cout << "Seq = " << sendMsg.SeqNum << "的报文段已超时，正在重传......" << endl;
					SetTextColor(White);
				}

			}
			msgStart = clock();
			sendAgain = 0;
		}
		if (over == 1)//已收到所有ack
		{
			break;
		}
	}
	CloseHandle(hThread);
	SetTextColor(Blue);
	cout << "已发送并确认所有报文，文件传输成功！" << endl;
	SetTextColor(White);
	//计算传输时间和吞吐率
	cout << "============================输出日志================================" << endl;
	int endTime = clock();
	cout << "总体传输时间为: " << (endTime - startTime) / CLOCKS_PER_SEC << " s" << endl;
	cout << "吞吐率: " << ((float)fileSize) / ((endTime - startTime) / CLOCKS_PER_SEC) << " byte/s" << endl << endl;
}

int main()
{
	//初始化socket
	WSAData wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata); //MAKEWORD(主版本号, 副版本号)
	string ip1, ip2;
	const char* IP1=0, *IP2=0;
	string addr = "127.0.0.1";
	cout << "请输入客户端IP地址（输入0默认本机地址）:";
	cin >> ip1;
	if(ip1 == "0")
		IP1 = addr.c_str();
	else
		IP1 = ip1.c_str();
	cout << "请输入客户端端口号：";
	cin >> ClientPORT;
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
		cout << "初始化Socket失败!" << endl;
		SetTextColor(White);
		return -1;
	}
	SetTextColor(Blue);
	cout << "初始化Socket成功!" << endl;
	SetTextColor(White);
	//创建socket
	SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
	unsigned long on = 1;
	ioctlsocket(clientSocket, FIONBIO, &on);//设置非阻塞
	if (clientSocket == INVALID_SOCKET)
	{
		SetTextColor(Red);
		cout << "创建socket失败！" << endl;
		SetTextColor(White);
		return -1;
	}
	SetTextColor(Blue);
	cout << "创建socket成功！" << endl;
	SetTextColor(White);
	//初始化服务器/路由器地址
	SOCKADDR_IN routerAddr;
	routerAddr.sin_family = AF_INET; //地址类型
	routerAddr.sin_addr.S_un.S_addr = inet_addr(IP1); //地址
	routerAddr.sin_port = htons(RouterPORT); //端口号
	//初始化客户端地址
	SOCKADDR_IN clientAddr;
	clientAddr.sin_family = AF_INET; //地址类型
	clientAddr.sin_addr.S_un.S_addr = inet_addr(IP2); //地址
	clientAddr.sin_port = htons(ClientPORT); //端口号
	bind(clientSocket, (LPSOCKADDR)&clientAddr, sizeof(clientAddr));
	//建立连接
	bool c = Connect(clientSocket, routerAddr);
	if (c == 0)
		return -1;
	while (c)
	{
		int choice;
		cout << "输入0断开连接，输入1传输文件:" << endl;
		cin >> choice;
		if (choice == 1)
		{
			string filename;
			cout << "请输入文件路径：" << endl;
			cin >> filename;
			sendThread(filename, routerAddr, clientSocket);
		}
		else
		{
			c = false;//退出循环
		}
	}
	cout << "关闭连接..." << endl;
	Disconnect(clientSocket, routerAddr);
	system("pause");
	return 0;
}
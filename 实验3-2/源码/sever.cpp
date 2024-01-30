#include <iostream>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fstream>
#pragma comment (lib, "ws2_32.lib")
using namespace std;
//���ò���
#define MAX_WAIT_TIME  5000 //�ȴ���ʱʱ�� 5s
#define MAX_SEND_TIMES  1000 //���ͳ�ʱʱ�� 
#define MaxFileSize 200000000 //����ļ���С
#define MaxMsgSize 10000 //��������ݴ�С
const unsigned short SYN = 0x1;
const unsigned short ACK = 0x2;
const unsigned short FIN = 0x4;
const unsigned short RST = 0x8;
#pragma pack(1)//�ñ��������ṹ������ǿ���������У���ֹ�Ż��洢�ṹ���ж���
struct Message
{
	//У���
	unsigned short checkNum;//2�ֽ�
	//��� Seq num
	unsigned int SeqNum;//4�ֽ�
	//ȷ�Ϻ� Ack num
	unsigned int AckNum;//4�ֽ�
	//��־
	unsigned short flag;//2�ֽ�
	//���ݴ�С
	unsigned int size;//4�ֽ�
	//ԴIP��Ŀ��IP
	unsigned int SrcIP, DestIP;//4�ֽڡ�4�ֽ�
	//Դ�˿ںš�Ŀ�Ķ˿ں�
	unsigned short SrcPort, DestPort;//2�ֽڡ�2�ֽ�
	
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
	unsigned short* msgStream = (unsigned short*)this; //��16λ�֣������ֽڣ����е���
	for (int i = 0; i < sizeof(*this) / 2; i++)
	{
		sum += *msgStream++;
		if (sum & 0xFFFF0000) //��� sum �ĸ�16λ��Ϊ0���� sum �ĸ�16λ���㣬��16λ��1
		{
			sum &= 0xFFFF;
			sum++;
		}
	}
	this->checkNum = ~(sum & 0xFFFF); ///�� checkNum ����Ϊ sum ȡ����ĵ�16λ��
}

bool Message::checksum()
{
	unsigned int sum = 0;
	unsigned short* msgStream = (unsigned short*)this; //��16λ�֣������ֽڣ����е���
	for (int i = 0; i < sizeof(*this) / 2; i++)
	{
		sum += *msgStream++;//����ǰ16λ�ֵ�ֵ�ӵ� sum ��
		if (sum & 0xFFFF0000) //��� sum �ĸ�16λ��Ϊ0���� sum �ĸ�16λ���㣬��16λ��1
		{
			sum &= 0xFFFF;
			sum++;
		}
	}
	if ((sum & 0xFFFF) == 0xFFFF) //��� sum �ĵ�16λȫΪ1���򷵻�true����ʾУ��ͨ��
	{
		return true;
	}
	return false;
}
// ������ɫ����
enum ConsoleColor
{
	Red = FOREGROUND_RED,
	Green = FOREGROUND_GREEN,
	Blue = FOREGROUND_BLUE,
	Yellow = FOREGROUND_RED | FOREGROUND_GREEN,
	White = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
};
int ServerPORT; //server�˿ں�
int RouterPORT; //·�����˿ں�
int seq = 0;//ȫ�ֱ������к�
// �����������ÿ���̨�ı���ɫ
void SetTextColor(ConsoleColor color)
{
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(consoleHandle, static_cast<WORD>(color));
}
//ʵ��server�����նˣ�����������
bool Connect(SOCKET serverSocket, SOCKADDR_IN routerAddr)
{
	int AddrLen = sizeof(routerAddr);
	Message buffer1;
	Message buffer2;
	Message buffer3;
	while (1)
	{
		//���յ�һ�����ֵ���Ϣ��SYN=1��seq=x��
		int r = recvfrom(serverSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "����ʧ��,�ر����ӣ�" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//�ж�SYN�������
			if (!(buffer1.flag && SYN) || !buffer1.checksum() || !(buffer1.SeqNum == seq + 1))
			{
				SetTextColor(Red);
				cout << "���ӷ�������" << endl;
				SetTextColor(White);
				return false;
			}
			seq++;
			SetTextColor(Blue);
			cout << "[1]:�����������յ���һ�����ֵ���Ϣ��" << endl;
			SetTextColor(White);
			//���͵ڶ������ֵ���Ϣ��SYN=1��ACK=1��ack=x��
			buffer2.SrcPort = ServerPORT;
			buffer2.DestPort = RouterPORT;
			buffer2.AckNum = buffer1.SeqNum;//�������ظ���ack=�ͻ��˷�����seq
			buffer2.flag += SYN;
			buffer2.flag += ACK;
			buffer2.setchecksum();//����У���
			int s = sendto(serverSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, AddrLen);
			clock_t buffer2start = clock();
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "����ʧ��,�ر����ӣ�" << endl;
				SetTextColor(White);
				return false;
			}
			SetTextColor(Blue);
			cout << "[2]:���������ѷ��͵ڶ������ֵ���Ϣ��" << endl;
			SetTextColor(White);
			//���յ��������ֵ���Ϣ��ACK=1��seq=x+1��
			while (1)
			{
				int r = recvfrom(serverSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, &AddrLen);
				if (r == 0)
				{
					SetTextColor(Red);
					cout << "����ʧ��,�ر����ӣ�" << endl;
					SetTextColor(White);
					return false;
				}
				else if (r > 0)
				{
					//�ɹ��յ���Ϣ�����У��͡�seq
					if ((buffer3.flag && ACK) && buffer3.checksum() && (buffer3.SeqNum == seq + 1))
					{
						seq++;
						SetTextColor(Blue);
						cout << "[3]:�����������յ����������ֵ���Ϣ��" << endl;
						cout << "�����������ӳɹ���" << endl;
						SetTextColor(White);
						return true;
					}
					else
					{
						SetTextColor(Red);
						cout << "���ӷ�������" << endl;
						SetTextColor(White);
						return false;
					}
				}
				//buffer2��ʱ�����·��Ͳ����¼�ʱ
				if (clock() - buffer2start > MAX_WAIT_TIME)
				{
					SetTextColor(Red);
					cout << "�ڶ������ֳ�ʱ�������ش�......" << endl;
					SetTextColor(White);
					int s = sendto(serverSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, AddrLen);
					buffer2start = clock(); //��������buffer2��ʱ��
					if (s == 0)
					{
						SetTextColor(Red);
						cout << "����ʧ��,�ر����ӣ�" << endl;
						SetTextColor(White);
						return false;
					}
				}
			}
		}
	}
	return false;
}
//ʵ��server�����նˣ����Ĵλ���
bool Disconnect(SOCKET serverSocket, SOCKADDR_IN routerAddr)
{
	int AddrLen = sizeof(routerAddr);
	Message buffer1;
	Message buffer2;
	Message buffer3;
	Message buffer4;
	while (1)
	{
		//���յ�һ�λ��ֵ���Ϣ��FIN=1��seq=y��
		int r = recvfrom(serverSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "����ʧ��,�ر����ӣ�" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//�ж�SYN�������
			if (!(buffer1.flag && FIN) || !buffer1.checksum() || !(buffer1.SeqNum == seq + 1))
			{
				SetTextColor(Red);
				cout << "���ӷ�������" << endl;
				SetTextColor(White);
				return false;
			}
			seq++;
			SetTextColor(Blue);
			cout << "[1]:��������յ���һ�λ��ֵ���Ϣ��" << endl;
			SetTextColor(White);
			//���͵ڶ��λ��ֵ���Ϣ��ACK=1��ack=y��
			buffer2.SrcPort = ServerPORT;
			buffer2.DestPort = RouterPORT;
			buffer2.AckNum = buffer1.SeqNum;
			buffer2.flag += ACK;
			buffer2.setchecksum();//����У���
			int s = sendto(serverSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, AddrLen);
			clock_t buffer2start = clock();
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "����ʧ��......�ر����ӣ�" << endl;
				SetTextColor(White);
				return false;
			}
			SetTextColor(Blue);
			cout << "[2]:���������ѷ��͵ڶ��λ��ֵ���Ϣ��" << endl;
			SetTextColor(White);
			break;
		}
	}
	//���͵����λ��ֵ���Ϣ��FIN=1��ACK=1��seq=z��
	buffer3.SrcPort = ServerPORT;
	buffer3.DestPort = RouterPORT;
	buffer3.flag += FIN;//����FIN
	buffer3.flag += ACK;//����ACK
	buffer3.SeqNum = seq++;//�������seq
	buffer3.setchecksum();//����У���
	int s = sendto(serverSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, AddrLen);
	clock_t buffer3start = clock();
	if (s == 0)
	{
		SetTextColor(Red);
		cout << "����ʧ��,�ر����ӣ�" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout << "[3]:���������ѷ��͵����λ��ֵ���Ϣ��" << endl;
	SetTextColor(White);

	//���յ��Ĵλ��ֵ���Ϣ��ACK=1��ack=z��
	while (1)
	{
		int r = recvfrom(serverSocket, (char*)&buffer4, sizeof(buffer4), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "�ر����Ӵ���" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//�ɹ��յ���Ϣ�����У��͡�ACK��ack
			if ((buffer4.flag && ACK) && buffer4.checksum() && (buffer4.AckNum == buffer3.SeqNum))
			{
				SetTextColor(Blue);
				cout << "[4]:�����������յ����Ĵλ��ֵ���Ϣ��" << endl;
				SetTextColor(White);
				break;
			}
			else
			{
				SetTextColor(Red);
				cout << "���ӷ�������" << endl;
				SetTextColor(White);
				return false;
			}
		}
		//buffer3��ʱ�����·��Ͳ����¼�ʱ
		if (clock() - buffer3start > MAX_WAIT_TIME)
		{
			SetTextColor(Red);
			cout << "�����λ��ֳ�ʱ�������ش�......" << endl;
			SetTextColor(White);
			int s = sendto(serverSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, AddrLen);
			buffer3start = clock();
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "�ر�����ʧ�ܣ�" << endl;
				SetTextColor(White);
				return false;
			}
		}
	}
	SetTextColor(Blue);
	cout << "�ر����ӳɹ���" << endl;
	SetTextColor(White);
	return true;
}
//���ձ��Ķ�
bool recvMessage(Message& recvMsg, SOCKET serverSocket, SOCKADDR_IN routerAddr, int& expectedSeq)
{
	int AddrLen = sizeof(routerAddr);
	while (1)
	{
		int r = recvfrom(serverSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r > 0)
		{
			//�ɹ��յ���Ϣ���У��͡����к�
			if (recvMsg.checksum() && (recvMsg.SeqNum == expectedSeq))
			{
				//�ظ�ACK
				Message reply;
				reply.SrcPort = ServerPORT;//ԭ�˿�
				reply.DestPort = RouterPORT;//Ŀ�Ķ˿�
				reply.flag += ACK;//����ACK=1
				reply.AckNum = recvMsg.SeqNum;//ack=seq
				reply.setchecksum();//����У���
				sendto(serverSocket, (char*)&reply, sizeof(reply), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
				cout << "[recv]: " << "Seq = " << recvMsg.SeqNum << "��checksum =" << recvMsg.checkNum <<  endl << "[send]: Ack = " << reply.AckNum << endl;
				expectedSeq++;
				return true;
			}
			//���seq��= �ڴ�ֵ���򷵻��ۼ�ȷ�ϵ�ack��expectedSeq-1��
			else if (recvMsg.checksum() && (recvMsg.SeqNum != expectedSeq))
			{
				//�ظ�ACK
				Message reply;
				reply.SrcPort = ServerPORT;
				reply.DestPort = RouterPORT;
				reply.flag += ACK;
				reply.AckNum = expectedSeq - 1;//ack=�ڴ�ֵ��һ
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
//�����ļ�
void serverRecvFile(SOCKET serverSocket, SOCKADDR_IN routerAddr)
{
	int expectedSeq = 0;
	int AddrLen = sizeof(routerAddr);
	//�����ļ������ļ���С
	Message nameMessage;
	unsigned int fileSize;//�ļ���С
	char fileName[50] = { 0 };//�ļ���
	while (1)
	{
		int r = recvfrom(serverSocket, (char*)&nameMessage, sizeof(nameMessage), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r > 0)
		{
			//�ɹ��յ���Ϣ��ΪexpectedSeq
			if (nameMessage.checksum() && (nameMessage.SeqNum == expectedSeq))
			{
				fileSize = nameMessage.size;//��ȡ�ļ���С
				for (int i = 0; nameMessage.msgData[i]; i++)//��ȡ�ļ���
					fileName[i] = nameMessage.msgData[i];
				SetTextColor(Blue);
				cout << "�����ļ���Ϊ��" << fileName << "����СΪ��" << fileSize << endl << endl;
				SetTextColor(White);
				//�ظ�ACK
				Message reply;
				reply.SrcPort = ServerPORT;
				reply.DestPort = RouterPORT;
				reply.flag += ACK;
				reply.AckNum = nameMessage.SeqNum;//ȷ��expectedSeq
				reply.setchecksum();
				sendto(serverSocket, (char*)&reply, sizeof(reply), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
				cout << "[recv]: Seq = " << nameMessage.SeqNum << endl << "[send]: Ack = " << reply.AckNum << endl;
				expectedSeq++;
				break;
			}
			//���seq��= �ڴ�ֵ���򷵻��ۼ�ȷ�ϵ�ack��expectedSeq-1��
			else if (nameMessage.checksum() && (nameMessage.SeqNum != expectedSeq))
			{
				//�ظ�ACK
				Message reply;
				reply.SrcPort = ServerPORT;
				reply.DestPort = RouterPORT;
				reply.flag += ACK;
				reply.AckNum = expectedSeq - 1;//�ۼ�ȷ��
				reply.setchecksum();
				sendto(serverSocket, (char*)&reply, sizeof(reply), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
				cout << "[GBN recv]: Seq = " << nameMessage.SeqNum << endl << "[send]: Ack = " << reply.AckNum << endl;
			}
		}
	}

	//�������ݶ�
	int batchNum = fileSize / MaxMsgSize;//ȫװ���ı��ĸ���
	int leftSize = fileSize % MaxMsgSize;//����װ����ʣ�౨�Ĵ�С
	BYTE* fileBuffer = new BYTE[fileSize];
	SetTextColor(Blue);
	cout << "��ʼ�������ݶΣ��� " << batchNum << " �����װ�ر��Ķ�" << endl;
	SetTextColor(White);
	for (int i = 0; i < batchNum; i++)
	{
		Message dataMsg;
		if (recvMessage(dataMsg, serverSocket, routerAddr, expectedSeq))
		{
			cout << "���ݱ�" << dataMsg.SeqNum << "���ճɹ�" << endl;
		}
		else
		{
			SetTextColor(Red);
			cout << "���ݽ���ʧ�ܣ�" << endl;
			SetTextColor(White);
			return;
		}
		//��ȡ���ݲ���
		for (int j = 0; j < MaxMsgSize; j++)
		{
			fileBuffer[i * MaxMsgSize + j] = dataMsg.msgData[j];
		}
	}
	//ʣ�ಿ��
	if (leftSize > 0)
	{
		Message dataMsg;
		if (recvMessage(dataMsg, serverSocket, routerAddr, expectedSeq))
		{
			cout << "���ݱ�" << dataMsg.SeqNum << "���ճɹ�" << endl;
		}
		else
		{
			cout << "���ݽ���ʧ�ܣ�" << endl;
			return;
		}
		for (int j = 0; j < leftSize; j++)
		{
			fileBuffer[batchNum * MaxMsgSize + j] = dataMsg.msgData[j];
		}
	}

	cout << "�ļ�����ɹ�������д���ļ�......" << endl;
	SetTextColor(White);
	//д���ļ�
	FILE* outFile;
	outFile = fopen(fileName, "wb");
	if (fileBuffer != 0)
	{
		fwrite(fileBuffer, fileSize, 1, outFile);
		fclose(outFile);
		SetTextColor(Blue);
		cout << "�ļ�д��ɹ���" << endl;
		SetTextColor(White);
	}
	return;
}


int main()
{
	//��ʼ��socket 
	WSAData wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata); //MAKEWORD(���汾��, ���汾��)
	string ip1, ip2;
	const char* IP1=0, *IP2=0;
	string addr = "127.0.0.1";
	cout << "����������IP��ַ������0Ĭ�ϱ�����ַ��:";
	cin >> ip1;
	if(ip1 == "0")
		IP1 = addr.c_str();
	else
		IP1 = ip1.c_str();
	cout << "���������˶˿ںţ�";
	cin >> ServerPORT;
	cout << "������·������IP��ַ������0Ĭ�ϱ�����ַ��:";
	cin >> ip2;
	if(ip2 == "0")
		IP2 = addr.c_str();
	else
		IP2 = ip2.c_str();
	cout << "������·�����˶˿ںţ�";
	cin >> RouterPORT;
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2)
	{
		SetTextColor(Red);
		cout << "��ʼ��Socketʧ�ܣ�" << endl;
		SetTextColor(White);
		return -1;
	}
	SetTextColor(Blue);
	cout << "��ʼ��Socket�ɹ�!" << endl;
	SetTextColor(White);
	//����socket
	SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
	unsigned long on = 1;
	ioctlsocket(serverSocket, FIONBIO, &on);//���÷�����
	if (serverSocket == INVALID_SOCKET)
	{
		SetTextColor(Red);
		cout << "����socketʧ�ܣ�" << endl;
		SetTextColor(White);
		return -1;
	}
	SetTextColor(Blue);
	cout << "����socket�ɹ���" << endl;
	SetTextColor(White);

	//��ʼ����������ַ
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET; //��ַ����
	serverAddr.sin_addr.S_un.S_addr = inet_addr(IP1); //��ַ
	serverAddr.sin_port = htons(ServerPORT); //�˿ں�
	//bind
	int tem = bind(serverSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr));
	if (tem == SOCKET_ERROR)
	{
		SetTextColor(Red);
		cout << "����ʧ�ܣ�" << endl;
		SetTextColor(White);
		return -1;
	}
	else
	{
		SetTextColor(Blue);
		cout << "���ӵ��˿� " << ServerPORT << "���ɹ���" << endl;
		SetTextColor(White);
	}
	SetTextColor(Blue);
	cout << "�������˾������ȴ��ͻ������ӡ���" << endl << endl;
	SetTextColor(White);

	//��ʼ��·������ַ
	SOCKADDR_IN routerAddr;
	routerAddr.sin_family = AF_INET; //��ַ����
	routerAddr.sin_addr.S_un.S_addr = inet_addr(IP2); //��ַ
	routerAddr.sin_port = htons(RouterPORT); //�˿ں�
	//��������
	bool isConn = Connect(serverSocket, routerAddr);
	if (isConn == 0)
		return -1;
	//�����ļ�
	serverRecvFile(serverSocket, routerAddr);
	//�ر�����
	Disconnect(serverSocket, routerAddr);
	closesocket(serverSocket); //�ر�socket
	WSACleanup();
	system("pause");
	return 0;
}

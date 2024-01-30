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
//���ò���
#define MAX_WAIT_TIME  5000 //��ʱʱ�� 5s
#define MAX_SEND_TIMES  1000 //��ʱʱ�� 
#define MaxFileSize 200000000 //����ļ���С
#define MaxMsgSize 10000 //��������ݴ�С
const unsigned short SYN = 0x1;
const unsigned short ACK = 0x2;
const unsigned short FIN = 0x4;
const unsigned short RST = 0x8;
#pragma pack(1)//�ñ��������ṹ������ǿ���������У���ֹ�Ż��洢�ṹ���ж���
struct sendwindowstruct//���ʹ���
{
	int size;//���ڴ�С=7
	int base;//������ʼ���к�
	int nextseqnum;//��һ�������͵����к�
	sendwindowstruct();
};
sendwindowstruct::sendwindowstruct()
{
	size = 30;
	base = 0;
	nextseqnum = 0;
}
//���ͱ��Ķ�
struct parameters {
	SOCKADDR_IN routerAddr;
	SOCKET clientSocket;
	int msgSum;//��Ϣ����
};
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
#define Stream std::basic_ostream<T1, T2>&
std::mutex outputMutex;
//ȫ�ֱ���window
sendwindowstruct sendwindow;
int RouterPORT; //·�����˿ں�
int ClientPORT; //�ͻ��˶˿ں�
int seq = 0;//ȫ�ֱ��� ���ݰ����к�
int msgStart;
bool over = 0;
bool sendAgain = 0;
// �����������ÿ���̨�ı���ɫ
void SetTextColor(ConsoleColor color)
{
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(consoleHandle, static_cast<WORD>(color));
}
//ʵ��client�����Ͷˣ�����������
bool Connect(SOCKET clientSocket, SOCKADDR_IN routerAddr)
{
	int AddrLen = sizeof(routerAddr);
	Message buffer1;
	Message buffer2;
	Message buffer3;
	//���͵�һ�����ֵ���Ϣ��SYN=1��seq=x��
	buffer1.SrcPort = ClientPORT;//ԭ�˿ڿͻ��˶˿�
	buffer1.DestPort = RouterPORT;//Ŀ�Ķ˿�Ϊ·�����˿�
	buffer1.flag += SYN;//����SYN=1
	buffer1.SeqNum = ++seq;//�������seq
	buffer1.setchecksum();//����У���
	int s = sendto(clientSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, AddrLen);
	//���͵�һ��������Ϣ
	clock_t buffer1start = clock();//��ʼ��ʱ
	if (s == 0)//����ʧ��
	{
		SetTextColor(Red);
		cout << "����ʧ�ܣ��ر����ӣ�" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout <<"[1]:�ͻ����ѷ��͵�һ�����ֵ���Ϣ��" << endl;
	SetTextColor(White);
	//���յڶ������ֵ���Ϣ��SYN=1��ACK=1��ack=x��
	while (1)
	{
		int r = recvfrom(clientSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "����ʧ�ܣ��ر����ӣ�" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//�ɹ��յ���Ϣ�����У��͡�ACK��SYN��ack
			if ((buffer2.flag && ACK) && (buffer2.flag && SYN) && buffer2.checksum() && (buffer2.AckNum == buffer1.SeqNum))
			{
				SetTextColor(Blue);
				cout << "[2]:�ͻ������յ��ڶ������ֵ���Ϣ��" << endl;
				SetTextColor(White);
				break;//ֻ�н��յ���ȷ�ĵڶ���������Ϣ��break�˳�ѭ��
			}
			else
			{
				SetTextColor(Red);
				cout << "���ӷ�������" << endl;
				SetTextColor(White);
				return false;
			}
		}
		//buffer1��ʱ�����·��Ͳ����¼�ʱ
		if (clock() - buffer1start > MAX_WAIT_TIME)
		{
			SetTextColor(Red);
			cout << "��һ�����ֳ�ʱ�������ش�......" << endl;
			SetTextColor(White);
			int s = sendto(clientSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, AddrLen);
			buffer1start = clock();//���¼�ʱ
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "����ʧ�ܣ��ر����ӣ�" << endl;
				SetTextColor(White);
				return false;
			}
		}//�ش�������whileѭ���У������ȴ�����
	}
	//���͵��������ֵ���Ϣ��ACK=1��seq=x+1��
	buffer3.SrcPort = ClientPORT;//ԭ�˿�
	buffer3.DestPort = RouterPORT;//Ŀ�Ķ˿�
	buffer3.flag += ACK;//����ACK
	buffer3.SeqNum = ++seq;//�������seq=x+1
	buffer3.setchecksum();//����У���
	s = sendto(clientSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, AddrLen);
	if (s == 0)
	{
		SetTextColor(Red);
		cout << "����ʧ�ܣ��ر����ӣ�" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout << "[3]:�ͻ����ѷ��͵��������ֵ���Ϣ��" << endl;
	cout << "�ͻ������ӳɹ���" << endl;
	SetTextColor(White);
}

//ʵ��client�����Ͷˣ����Ĵλ���
bool Disconnect(SOCKET clientSocket, SOCKADDR_IN routerAddr)
{
	int AddrLen = sizeof(routerAddr);
	Message buffer1;
	Message buffer2;
	Message buffer3;
	Message buffer4;

	//���͵�һ�λ��ֵ���Ϣ��FIN=1��seq=y��
	buffer1.SrcPort = ClientPORT;
	buffer1.DestPort = RouterPORT;
	buffer1.flag += FIN;//����FIN
	buffer1.SeqNum = ++seq;//�������seq
	buffer1.setchecksum();//����У���
	int s = sendto(clientSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, AddrLen);
	clock_t buffer1start = clock();
	if (s == 0)
	{
		SetTextColor(Red);
		cout << "����ʧ�ܣ��ر����ӣ�" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout << "[1]:�ͻ����ѷ��͵�һ�λ��ֵ���Ϣ��" << endl;
	SetTextColor(White);

	//���յڶ��λ��ֵ���Ϣ��ACK=1��ack=y��
	while (1)
	{
		int r = recvfrom(clientSocket, (char*)&buffer2, sizeof(buffer2), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "�ر�����ʧ�ܣ�" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//�ɹ��յ���Ϣ�����У��͡�ACK��ack
			if ((buffer2.flag && ACK) && buffer2.checksum() && (buffer2.AckNum == buffer1.SeqNum))
			{
				SetTextColor(Blue);
				cout << "[2]:�ͻ������յ��ڶ��λ��ֵ���Ϣ��" << endl;
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
		//buffer1��ʱ�����·��Ͳ����¼�ʱ
		if (clock() - buffer1start > MAX_WAIT_TIME)
		{
			SetTextColor(Red);
			cout << "��һ�λ��ֳ�ʱ�������ش�......" << endl;
			SetTextColor(White);
			int s = sendto(clientSocket, (char*)&buffer1, sizeof(buffer1), 0, (sockaddr*)&routerAddr, AddrLen);
			buffer1start = clock();
			if (s == 0)
			{
				SetTextColor(Red);
				cout << "�ر�����ʧ�ܣ�" << endl;
				SetTextColor(White);
				return false;
			}
		}
	}
	//���յ����λ��ֵ���Ϣ��FIN=1��ACK=1��seq=z��
	while (1)
	{
		int r = recvfrom(clientSocket, (char*)&buffer3, sizeof(buffer3), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout << "�ر�����ʧ�ܣ�" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			//�ɹ��յ���Ϣ�����У��͡�ACK��ack
			if ((buffer3.flag && ACK) && (buffer3.flag && FIN) && buffer3.checksum())
			{
				SetTextColor(Blue);
				cout << "[3]:�ͻ������յ������λ��ֵ���Ϣ��" << endl;
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
	}
	//���͵��Ĵλ��ֵ���Ϣ��ACK=1��ack=z��
	buffer4.SrcPort = ClientPORT;
	buffer4.DestPort = RouterPORT;
	buffer4.flag += ACK;//����ACK
	buffer4.AckNum = buffer3.SeqNum;//�������seq
	buffer4.setchecksum();//����У���
	s = sendto(clientSocket, (char*)&buffer4, sizeof(buffer4), 0, (sockaddr*)&routerAddr, AddrLen);
	if (s == 0)
	{
		SetTextColor(Red);
		cout <<"�ر�����ʧ�ܣ�" << endl;
		SetTextColor(White);
		return false;
	}
	SetTextColor(Blue);
	cout << "[4]:�ͻ����ѷ��͵��Ĵλ��ֵ���Ϣ��" << endl;
	SetTextColor(White);
	//���Ĵλ���֮����ȴ�2MSL����ֹ���һ��ACK��ʧ�����ڰ�ر�
	int tempclock = clock();
	SetTextColor(Red);
	cout << "client��2MSL�ȴ�..." << endl;
	SetTextColor(White);
	Message tmp;
	while (clock() - tempclock < 2 * MAX_WAIT_TIME)
	{
		int r = recvfrom(clientSocket, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r == 0)
		{
			SetTextColor(Red);
			cout <<"�ر����Ӵ���" << endl;
			SetTextColor(White);
			return false;
		}
		else if (r > 0)
		{
			s = sendto(clientSocket, (char*)&buffer4, sizeof(buffer4), 0, (sockaddr*)&routerAddr, AddrLen);
			cout << "�ظ�" << endl;
		}
	}
	SetTextColor(Blue);
	cout << "�ر����ӳɹ���" << endl;
	SetTextColor(White);
}
//����ack���߳�
DWORD WINAPI recvThread(PVOID pParam)
{
	parameters* para = (parameters*)pParam;//��pParamת��Ϊparameters�ṹ��ָ��
	SOCKADDR_IN routerAddr = para->routerAddr;
	SOCKET clientSocket = para->clientSocket;
	int msgSum = para->msgSum; //��Ϣ������
	int AddrLen = sizeof(routerAddr);

	int wrongACK = -1;//ʧ���ACK����
	int wrongCount = 0;//���������
	while (1)//�ȴ�����
	{
		//rdt_rcv
		Message recvMsg;
		int r = recvfrom(clientSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&routerAddr, &AddrLen);
		if (r > 0)
		{
			if (recvMsg.checksum())//���У���
			{
				if (recvMsg.AckNum >= sendwindow.base)//ȷ�Ϻ�ack���ڵ��ڴ������
					sendwindow.base = recvMsg.AckNum + 1;//�������ƣ�baseָ��ȴ�ȷ�ϵ����ݰ�
				msgStart = clock();//��ʼ��ʱ
				if (sendwindow.base != sendwindow.nextseqnum)//base��nextseqnum���غ�
				{
					std::lock_guard<std::mutex>lock(outputMutex);
					cout << "[recv]: Ack = " << recvMsg.AckNum << endl;
				}
				//��ӡ�������
				{
					std::lock_guard<std::mutex>lock(outputMutex);
					SetTextColor(Red);
					cout << "[��ǰ�������]�� ���ڴ�С��" << sendwindow.size << "���ѷ��͵�δ�յ�ACK��" << sendwindow.nextseqnum - sendwindow.base
						<< "����δ���ͣ�" << sendwindow.size - (sendwindow.nextseqnum - sendwindow.base) << endl;
					SetTextColor(White);
				}
				//�жϽ��������
				if (recvMsg.AckNum == msgSum - 1)//���һ�����ݰ���ack�ѽ��յ�
				{
					SetTextColor(Blue);
					cout << "�ѽ��յ����һ�������ACK,������ɣ�" << endl;
					SetTextColor(White);
					over = 1;
					return 0;
				}
				//�����ش�
				if (wrongACK != recvMsg.AckNum)
				{//wrongACK������һ�ν��յ����ۻ�ȷ�ϵ�ack,��ô����յ���ACKһ������wrongACK,
					//������С�ڣ����������˵�����ݰ���ʧ
					wrongCount = 0;
					wrongACK = recvMsg.AckNum;
				}
				else
				{
					wrongCount++;
				}
				if (wrongCount == 3)
				{
					//�ط�
					sendAgain = 1;
				}
			}
			//��У��ʧ�ܻ�ack���ԣ�����ԣ������ȴ�
		}
	}
	return 0;
}

void sendThread(string filename, SOCKADDR_IN routerAddr, SOCKET clientSocket)
{
	int startTime = clock();
	//��ȡ�ļ���
	string realname = "";
	for (int i = filename.size() - 1; i >= 0; i--)
	{
		if (filename[i] == '/' || filename[i] == '\\')
			break;//ȥ���ļ�����ͷ�Ŀո���з�
		realname += filename[i];
	}
	realname = string(realname.rbegin(), realname.rend());//��������ļ��������
   // ���ļ��������ֽ��� 
	ifstream fin(filename.c_str(), ifstream::binary);
	if (!fin) {
		printf("�޷����ļ���\n");
		return;
	}
	//�ļ���ȡ��fileBuffer
	// ����fileBuffer��BYTE���飬���ڴ洢�ļ����ݣ������СΪ MaxFileSize
	BYTE* fileBuffer = new BYTE[MaxFileSize];
	unsigned int fileSize = 0;
	// �������� fin �л�ȡһ���ֽڣ����洢������ byte ��
	BYTE byte = fin.get();
	// ѭ����ȡ�ļ����ݣ�ֱ���ļ��� fin ��״̬��Ϊ false���ļ�������
	while (fin) {
		// ����ȡ�����ֽڴ洢�� fileBuffer �����У��������ļ���С fileSize
		fileBuffer[fileSize++] = byte;
		// ������������ fin �л�ȡ��һ���ֽ�
		byte = fin.get();
	}
	fin.close();
	int batchNum = fileSize / MaxMsgSize;//ȫװ���ı��ĸ���
	int leftSize = fileSize % MaxMsgSize;//����װ����ʣ�౨�Ĵ�С
	// ����������Ϣ�߳�
	int msgSum = leftSize > 0 ? batchNum + 2 : batchNum + 1;
	//�ж���Ϣ���������������ļ��⣬���һ��˵���ļ������ļ���С�����ݰ�
	parameters param;
	param.routerAddr = routerAddr;
	param.clientSocket = clientSocket;
	param.msgSum = msgSum;
	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recvThread, &param, 0, 0); //�����̣߳����ڽ���ack

	while (1)
	{
		//rdt_send(data)
		if (sendwindow.nextseqnum < sendwindow.base + sendwindow.size && sendwindow.nextseqnum < msgSum)//��Ҫ���͵����ݺ��ڴ����ڣ����Ҳ������һ�����к�
		{
			//make_pkt
			Message sendMsg;
			if (sendwindow.nextseqnum == 0)//��һ�����飬˵���ļ������ļ���С����Ϣ
			{
				sendMsg.SrcPort = ClientPORT;//ԭ�˿�
				sendMsg.DestPort = RouterPORT;//Ŀ�Ķ˿�
				sendMsg.size = fileSize;//�ļ���С
				sendMsg.flag += RST;//�ļ���
				sendMsg.SeqNum = sendwindow.nextseqnum;
				for (int i = 0; i < realname.size(); i++)//��䱨�����ݶ�
					sendMsg.msgData[i] = realname[i];//�ļ���д��Message�����ݲ���
				sendMsg.msgData[realname.size()] = '\0';//�ַ�����β��\0
				sendMsg.setchecksum();//����У���
			}
			else if (sendwindow.nextseqnum == batchNum + 1 && leftSize > 0)//�������һ����
			{
				sendMsg.SrcPort = ClientPORT;//Դ�˿�
				sendMsg.DestPort = RouterPORT;//Ŀ�Ķ˿�
				sendMsg.SeqNum = sendwindow.nextseqnum;//�������к�
				for (int j = 0; j < leftSize; j++)
				{
					sendMsg.msgData[j] = fileBuffer[batchNum * MaxMsgSize + j];//д���ļ�����
				}
				sendMsg.setchecksum();//����У���
			}
			else//���װ�ص����ݰ�
			{
				sendMsg.SrcPort = ClientPORT;//ԭ�˿�
				sendMsg.DestPort = RouterPORT;//Ŀ�Ķ˿�
				sendMsg.SeqNum = sendwindow.nextseqnum;//�������к�
				for (int j = 0; j < MaxMsgSize; j++)
				{//д���ļ�����
					sendMsg.msgData[j] = fileBuffer[(sendwindow.nextseqnum - 1) * MaxMsgSize + j];
				}
				sendMsg.setchecksum();//����У���
			}
			sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&routerAddr, sizeof(SOCKADDR_IN));
			{
				std::lock_guard<std::mutex>lock(outputMutex);
				cout << "[send]: Seq = " << sendMsg.SeqNum << "��checksum =" << sendMsg.checkNum << endl;
			}

			if (sendwindow.base == sendwindow.nextseqnum)
			{
				msgStart = clock();
			}
			sendwindow.nextseqnum++;
			//���ͱ��ĶΣ�û�н���ACK���ڲ�����
		}
		//��ʱ
		if (clock() - msgStart > MAX_WAIT_TIME || sendAgain)//��ʱ��ack�ظ�3��
		{
			if (sendAgain) {
				{
					std::lock_guard<std::mutex>lock(outputMutex);
					SetTextColor(Red);
					cout <<  "�����յ������ظ�ACK�������ش�......" << endl;
					SetTextColor(White);
				}
			}
			//�ط���ǰ��������message
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
					for (int i = 0; i < realname.size(); i++)//��䱨�����ݶ�
						sendMsg.msgData[i] = realname[i];
					sendMsg.msgData[realname.size()] = '\0';//�ַ�����β��\0
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
					cout << "Seq = " << sendMsg.SeqNum << "�ı��Ķ��ѳ�ʱ�������ش�......" << endl;
					SetTextColor(White);
				}

			}
			msgStart = clock();
			sendAgain = 0;
		}
		if (over == 1)//���յ�����ack
		{
			break;
		}
	}
	CloseHandle(hThread);
	SetTextColor(Blue);
	cout << "�ѷ��Ͳ�ȷ�����б��ģ��ļ�����ɹ���" << endl;
	SetTextColor(White);
	//���㴫��ʱ���������
	cout << "============================�����־================================" << endl;
	int endTime = clock();
	cout << "���崫��ʱ��Ϊ: " << (endTime - startTime) / CLOCKS_PER_SEC << " s" << endl;
	cout << "������: " << ((float)fileSize) / ((endTime - startTime) / CLOCKS_PER_SEC) << " byte/s" << endl << endl;
}

int main()
{
	//��ʼ��socket
	WSAData wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata); //MAKEWORD(���汾��, ���汾��)
	string ip1, ip2;
	const char* IP1=0, *IP2=0;
	string addr = "127.0.0.1";
	cout << "������ͻ���IP��ַ������0Ĭ�ϱ�����ַ��:";
	cin >> ip1;
	if(ip1 == "0")
		IP1 = addr.c_str();
	else
		IP1 = ip1.c_str();
	cout << "������ͻ��˶˿ںţ�";
	cin >> ClientPORT;
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
		cout << "��ʼ��Socketʧ��!" << endl;
		SetTextColor(White);
		return -1;
	}
	SetTextColor(Blue);
	cout << "��ʼ��Socket�ɹ�!" << endl;
	SetTextColor(White);
	//����socket
	SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
	unsigned long on = 1;
	ioctlsocket(clientSocket, FIONBIO, &on);//���÷�����
	if (clientSocket == INVALID_SOCKET)
	{
		SetTextColor(Red);
		cout << "����socketʧ�ܣ�" << endl;
		SetTextColor(White);
		return -1;
	}
	SetTextColor(Blue);
	cout << "����socket�ɹ���" << endl;
	SetTextColor(White);
	//��ʼ��������/·������ַ
	SOCKADDR_IN routerAddr;
	routerAddr.sin_family = AF_INET; //��ַ����
	routerAddr.sin_addr.S_un.S_addr = inet_addr(IP1); //��ַ
	routerAddr.sin_port = htons(RouterPORT); //�˿ں�
	//��ʼ���ͻ��˵�ַ
	SOCKADDR_IN clientAddr;
	clientAddr.sin_family = AF_INET; //��ַ����
	clientAddr.sin_addr.S_un.S_addr = inet_addr(IP2); //��ַ
	clientAddr.sin_port = htons(ClientPORT); //�˿ں�
	bind(clientSocket, (LPSOCKADDR)&clientAddr, sizeof(clientAddr));
	//��������
	bool c = Connect(clientSocket, routerAddr);
	if (c == 0)
		return -1;
	while (c)
	{
		int choice;
		cout << "����0�Ͽ����ӣ�����1�����ļ�:" << endl;
		cin >> choice;
		if (choice == 1)
		{
			string filename;
			cout << "�������ļ�·����" << endl;
			cin >> filename;
			sendThread(filename, routerAddr, clientSocket);
		}
		else
		{
			c = false;//�˳�ѭ��
		}
	}
	cout << "�ر�����..." << endl;
	Disconnect(clientSocket, routerAddr);
	system("pause");
	return 0;
}
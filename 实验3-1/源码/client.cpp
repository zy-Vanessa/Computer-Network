#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#include<iostream>
#include<WinSock2.h>
#include<time.h>
#include<fstream>
#include<iostream>
#include<string>
using namespace std;
#pragma comment(lib,"ws2_32.lib")

SOCKADDR_IN server_addr;
SOCKADDR_IN router_addr;
SOCKADDR_IN client_addr;
SOCKET client;

char* message = new char[100000000];
char* sbuffer = new char[1000];
char* rbuffer = new char[1000];
unsigned long long int messagelength = 0;// The index of the last message to be sent
unsigned long long int messagepointer = 0;// The next position to send

int slen = sizeof(server_addr);
int rlen = sizeof(router_addr);

u_long unlockmode = 1;
u_long lockmode = 0;
const unsigned int MAX_DATA_LENGTH = 1000;
const u_int SOURCEIP = 0x7f000001;
const u_int DESIP = 0x7f000001;
const u_short SOURCEPORT = 7777;//Դ�˿���9999
const u_short DESPORT = 9999;   //����˶˿ں���7777
const unsigned char SYN = 0x01;          //FC=0,OVER=0,FIN=0,ACK=0,SYN=1
const unsigned char ACK = 0x02;          //FC=0,OVER=0,FIN=0,ACK=1,SYN=0
const unsigned char SYN_ACK = 0x03;      //FC=0,OVER=0,FIN=0,ACK=1,SYN=1
const unsigned char FIN = 0x04;          //FC=0,OVER=0,FIN=1,ACK=0,SYN=0
const unsigned char FIN_ACK = 0x06;      //FC=0,OVER=0,FIN=1,ACK=1,SYN=0
const unsigned char OVER = 0x08;         //FC=0,OVER=1,FIN=0,ACK=0,SYN=0
const unsigned char OVER_ACK = 0xA;      //FC=0,OVER=1,FIN=0,ACK=1,SYN=0
const unsigned char FINAL_CHECK=0x10;    //FC=1,OVER=0,FIN=0,ACK=0,SYN=0
const double MAX_TIME = 0.2*CLOCKS_PER_SEC;
//����ͷ
struct Header {
    u_short checksum;     //У���
    u_short seq;          //���к�,ͣ�����λֻ��0��1����״̬
    u_short ack;          //ack�ţ�ͣ�����λֻ��0��1����״̬
    u_short flag;         //״̬λ
    u_short length;       //����λ
    u_int source_ip;      //Դip��ַ
    u_int des_ip;         //Ŀ��ip��ַ
    u_short source_port;  //Դ�˿ں�
    u_short des_port;     //Ŀ�Ķ˿ں�
    Header() {//���캯��
        checksum = 0;
        source_ip = SOURCEIP;
        des_ip = DESIP;
        source_port = SOURCEPORT;
        des_port = DESPORT;
        seq = 0;
        ack = 0;
        flag = 0;
        length = 0;
    }
};
//ȫ��ʱ������
clock_t linkClock;
clock_t ALLSTART;
clock_t ALLEND;

u_short calcksum(u_short* mes, int size) {
    // ����16λ���ֽ���
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    // ���������ݸ��Ƶ�������
    memcpy(buf, mes, size);
    // ��ʼ��У��ͱ���
    u_long sum = 0;
    buf += 1;
    count -= 1;
    // �����������е�16λ�֣����������
    while (count--) {
        sum += *buf++;
        // ����͵ĸ�16λ��Ϊ�㣬��������ּӵ���16λ
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    // ����У��͵ķ���
    return ~(sum & 0xffff);
}

u_short checksum(u_short* mes, int size) {
    // ����16λ���ֽ���
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    // ���������ݸ��Ƶ�������
    memcpy(buf, mes, size);
    // ��ʼ��У��ͱ���
    u_long sum = 0;
    // �����������е�16λ�֣����������
    while (count--) {
        sum += *buf++;
        // ����͵ĸ�16λ��Ϊ�㣬��������ּӵ���16λ
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    // ����У��͵ķ���
    return ~(sum & 0xffff);
}

int Connect() {
    linkClock = clock();
    Header header;
    char* recvshbuffer = new char[sizeof(header)];
    char* sendshbuffer = new char[sizeof(header)];

    //��һ����������
    header.source_port = SOURCEPORT;
    header.des_port = DESPORT;
    header.flag = SYN;
    header.seq = 0;
    header.ack = 0;
    header.length = 0;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    //cout << checksum((u_short*)&header, sizeof(header)) << endl;
    u_short* test = (u_short*)&header;
    memcpy(sendshbuffer, &header, sizeof(header));
FIRSTSHAKE:
    if (sendto(client, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:��һ������������ʧ��..." << endl;
        return -1;
    }
    cout << "[1]:��һ��������Ϣ���ͳɹ�...." << endl;
    //�����Ƿ�Ϊ������ģʽ
    ioctlsocket(client, FIONBIO, &unlockmode);

    //���ü�ʱ��
    clock_t start = clock();

    //��һ�������ش�
    while (recvfrom(client, recvshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:���ӳ�ʱ,�������Զ��Ͽ�" << endl;
            return -1;
        }
        if (clock() - start > MAX_TIME) {
            if (sendto(client, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:��һ������������ʧ��..." << endl;
                return -1;
            }
            start = clock();
            cout << "[1]:��һ��������Ϣ������ʱ....�������·���" << endl;
            goto FIRSTSHAKE;
        }
    }

    //����ڶ���������Ϣ�Ƿ�׼ȷ
    memcpy(&header, recvshbuffer, sizeof(header));
    if (header.flag == SYN_ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[2]:�ڶ������ֽ����ɹ�..." << endl;
    }
    else {
        cout << "[1]:�����ڴ��ķ�������ݰ�,�����ش���һ���������ݰ�...." << endl;
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:���ӳ�ʱ,�������Զ��Ͽ�" << endl;
            return -1;
        }
        goto FIRSTSHAKE;
    }

    //���͵�����������Ϣ
    header.source_port = SOURCEPORT;
    header.des_port = DESPORT;
    header.flag = ACK;
    header.seq = 1;
    header.ack = 1;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendshbuffer, &header, sizeof(header));
THIRDSHAKE:
    if (sendto(client, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:������������Ϣ����ʧ��...." << endl;
        return -1;
    }
    start = clock();
    while (recvfrom(client, recvshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:���ӳ�ʱ,�������Զ��Ͽ�" << endl;
            return -1;
        }
        if (clock() - start >= 5 * MAX_TIME) {
            cout << "[Failed]:������������Ϣ������ʱ...�������·���" << endl;
            if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
                cout << "[Failed]:���ӳ�ʱ,�������Զ��Ͽ�" << endl;
                return -1;
            }
            goto THIRDSHAKE;
        }
    }
    memcpy(&header, recvshbuffer, sizeof(header));
    if (header.flag == ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[3]:��ȷ���͵�����������Ϣ" << endl;
        cout << "[DONE]:����������ӳɹ���׼����������" << endl;
        return 1;
    }
    else {
        cout << "[ERROR]:ȷ����Ϣ����ʧ��...�ط�����������" << endl;
        goto THIRDSHAKE;
    }
    
}

int Disconnect() {
    Header header;
    char* sendbuffer = new char[sizeof(header)];
    char* recvbuffer = new char[sizeof(header)];
    header.seq = 0;
    header.flag = FIN;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:��һ�λ��ַ���ʧ��" << endl;
        return -1;
    }
WAITWAVE2:
    clock_t start = clock();
    while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > MAX_TIME) {
            if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:��һ�λ��ַ���ʧ��" << endl;
                return -1;
            }
            start = clock();
            cout << "[Failed]:��һ�λ�����Ϣ������ʱ" << endl;
        }
    }
    cout << "[1]:��һ�λ�����Ϣ���ͳɹ�" << endl;
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[2]:�ڶ��λ�����Ϣ���ճɹ�" << endl;
    }
    else {
        cout << "[Failed]:�ڶ��λ�����Ϣ����ʧ��" << endl;
        goto WAITWAVE2;
    }
WAITWAVE3:
    while(recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0){}
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == FIN_ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[3]:�����λ�����Ϣ���ճɹ�" << endl;
    }
    else {
        cout << "[Failed]:�����λ�����Ϣ����ʧ��" << endl;
        goto WAITWAVE3;
    }
    header.seq = 1;
    header.flag = ACK;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
SENDWAVE4:
    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:���Ĵλ��ַ���ʧ��" << endl;
        return -1;
    }
    start = clock();
    ioctlsocket(client, FIONBIO, &unlockmode);
    while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > 2 * MAX_TIME) {
            cout << "[Failed]:���ܷ�����ʱ���ط����Ĵλ���" << endl;
            goto SENDWAVE4;
        }
    }
    cout << "[4]:���Ĵλ��ַ��ͳɹ�" << endl;
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == FINAL_CHECK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[System]:�Ĵλ�����ɣ������Ͽ�����" << endl;
        return 1;
    }
    else {
        cout << "[Failed]:���ݰ�����,׼���ط����Ĵ�����" << endl;
        goto SENDWAVE4;
    }
}

int loadMessage() {
    string filename;
    cout << "[INPUT]:������Ҫ������ļ���" << endl;
    cin >> filename;

    string fileExtension = filename.substr(filename.find_last_of(".") + 1);
    unsigned char fileFormat = '0';  // Default: 0 for txt

    if (fileExtension == "jpg" || fileExtension == "png") {
        fileFormat = '1';
    }

    ifstream fin(filename.c_str(), ifstream::binary);

    message[0] = fileFormat;

    unsigned long long int index = 1;
    unsigned char temp = fin.get();
    while (fin) {
        message[index++] = temp;
        temp = fin.get();
    }

    messagelength = index - 1;
    fin.close();

    cout << "[FINISH]:����ļ����빤��" << endl;
    return 0;
}

int endsend() {
    ALLEND = clock();
    Header header;
    char* sendbuffer = new char[sizeof(header)];
    char* recvbuffer = new char[sizeof(header)];

    header.flag = OVER;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
SEND:
    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:��������źŷ���ʧ��" << endl;
        return -1;
    }
    cout << "[SEND]:��������źŷ��ͳɹ�" << endl;
    clock_t start = clock();
RECV:
    while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > MAX_TIME) {
            if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:��������źŷ���ʧ��" << endl;
                return -1;
            }
            start = clock();
            cout << "[ERROR]:��������źŷ�����ʱ" << endl;
            goto SEND;
        }
    }
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == OVER_ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[FINFISH]:���������Ϣ���ͳɹ�" << endl;
        return 1;
    }
    else {
        cout << "[ERROR]:���ݰ�����" << endl;
        goto RECV;
    }
}

int sendmessage() {
    ALLSTART = clock();
    //�����Ƿ�Ϊ������ģʽ
    ioctlsocket(client, FIONBIO, &unlockmode);

    Header header;
    char* recvbuffer = new char[sizeof(header)];
    char* sendbuffer = new char[sizeof(header) + MAX_DATA_LENGTH];

    while (true) {

        int ml;//�������ݴ��䳤��
        if (messagepointer > messagelength) {//����Ҫ�ٷ��ˣ���������
            if (endsend() == 1) { return 1; }
            return -1;
        }
        if (messagelength - messagepointer >= MAX_DATA_LENGTH) {//���԰�������޶ȷ���
            ml = MAX_DATA_LENGTH;
        }
        else {
            ml = messagelength - messagepointer + 1;//��Ҫ���㷢�͵ĳ���
        }

        header.seq = 0;//��η�����������к�
        header.length = ml;//ʵ�����ݳ���
        memset(sendbuffer, 0, sizeof(header) + MAX_DATA_LENGTH);//sendbufferȫ������
        memcpy(sendbuffer, &header, sizeof(header));//����header����
        memcpy(sendbuffer+sizeof(header), message + messagepointer, ml);//������������
        messagepointer += ml;//��������ָ��
        header.checksum = calcksum((u_short*)sendbuffer, sizeof(header)+MAX_DATA_LENGTH);//����У���
        memcpy(sendbuffer, &header, sizeof(header));//���У���
    SEQ0SEND:
        //����seq=0�����ݰ�
        cout << "[send]:" << endl << "seq:0" << " length:" << ml<<" ";
        cout << "checksum:" << checksum((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;
        if (sendto(client, sendbuffer, (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
            cout << "[Failed]:seq0���ݰ�����ʧ��" << endl;
            return -1;
        }
        clock_t start = clock();
    SEQ0RECV:
        //����յ������˾Ͳ����ˣ�������ʱ�ش�
        //�����˷����������Բ��Ῠס
        while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
            if (clock() - start > MAX_TIME) {
                if (sendto(client, sendbuffer, (sizeof(header)+MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
                    cout << "[Failed]:seq0���ݰ�����ʧ��" << endl;
                    return -1;
                }
                start = clock();
                cout << "[ERROR]:seq0���ݰ�������ʱ" << endl;
            }
        }
        //���ackλ�Ƿ���ȷ�������ȷ��׼������һ�����ݰ�
        memcpy(&header, recvbuffer, sizeof(header));
        cout << "[RECV]:" << endl << "ack:" << header.ack << "checksum:" << checksum((u_short*)&header, sizeof(header)) << endl;
        if (header.ack == 1 && checksum((u_short*)&header, sizeof(header) == 0)) {
            cout << "[CHECKED]:seq0���ճɹ�" << endl;
        }
        else {
            cout << "[ERROR]:�����δ������ȷ�����ݰ�" << endl;
            goto SEQ0SEND;
        }

        //׼����ʼ��SEQ=1�����ݰ�
        if (messagepointer > messagelength) {
            if (endsend() == 1) { return 1; }
            return -1;
        }
        if (messagelength - messagepointer >= MAX_DATA_LENGTH) {
            ml = MAX_DATA_LENGTH;
        }
        else {
            ml = messagelength - messagepointer + 1;
        }

        header.seq = 1;//������к�
        header.length = ml;//ʵ�����ݳ���
        memset(sendbuffer, 0, sizeof(header) + MAX_DATA_LENGTH);//sendbufferȫ������
        memcpy(sendbuffer, &header, sizeof(header));//����header����
        memcpy(sendbuffer + sizeof(header), message + messagepointer, ml);//������������
        messagepointer += ml;//��������ָ��
        header.checksum = calcksum((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH);//����У���
        memcpy(sendbuffer, &header, sizeof(header));//���У���
        //cout << checksum((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;
    SEQ1SEND:
        //����seq=1�����ݰ�
        cout << "[SEND]:" << endl << "seq:1" << " length:" << ml <<" ";
        cout << "checksum:" << checksum((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;
        if (sendto(client, sendbuffer, (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
            cout << "[Failed]:seq1���ݰ�����ʧ��" << endl;
            return -1;
        }
        start = clock();
    SEQ1RECV:
        //����յ������˾Ͳ����ˣ�������ʱ�ش�
        while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
            if (clock() - start > MAX_TIME) {
                if (sendto(client, sendbuffer, (sizeof(header)+MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
                    cout << "[Failed]:seq1���ݰ�����ʧ��" << endl;
                    return -1;
                }
                start = clock();
                cout << "[ERROR]:seq1���ݰ�������ʱ" << endl;
            }
        }
        //���ackλ�Ƿ���ȷ�������ȷ��׼������һ�����ݰ�
        memcpy(&header, recvbuffer, sizeof(header));
        cout << "[RECV]:" << endl << "ack:" << header.ack << " checksum:" << checksum((u_short*)&header, sizeof(header)) << endl;
        if (header.ack == 0 && checksum((u_short*)&header, sizeof(header)) == 0) {
            cout << "[CHECKED]seq1���ճɹ�" << endl;
        }
        else {
            cout << "[ERROR]:�����δ������ȷ�����ݰ�" << endl;
            goto SEQ1SEND;
        }
    }
}

void printlog() {
    cout << "--------------������־--------------" << endl;
    cout << "���α����ܳ���Ϊ " << messagepointer << "�ֽڣ�����Ϊ " << (messagepointer / 256) + 1 << "�����Ķηֱ�ת��" << endl;
    double t = (ALLEND - ALLSTART) / CLOCKS_PER_SEC;
    cout << "���δ������ʱ��Ϊ" <<t <<"��"<< endl;
    t = messagepointer / t;
    cout << "���δ���������Ϊ" << t <<"�ֽ�ÿ��"<< endl;
}

int main() {
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
    int flag = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (flag == 0)
	{
		cout << "[System]:";
		cout << "WSA���سɹ�" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "WSA����ʧ��" << endl;
	}
    //�����
    server_addr.sin_family = AF_INET;//ʹ��IPV4
    server_addr.sin_port = htons(9999);//server�Ķ˿ں�
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//����127.0.0.1

    //·����
    router_addr.sin_family = AF_INET;//ʹ��IPV4
    router_addr.sin_port = htons(8888);//router�Ķ˿ں�
    router_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//����127.0.0.1

    //�ͻ���
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(7777);
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//����127.0.0.1

    client = socket(AF_INET, SOCK_DGRAM, 0);
    flag = bind(client, (SOCKADDR*)&client_addr, sizeof(client_addr));
    if (flag == 0)//�жϰ�
	{
		cout << "[System]:";
		cout << "�󶨳ɹ�" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "��ʧ��" << endl;
		closesocket(client);
		WSACleanup();
		return 0;
	}
    cout << "[PREPARE]��ʼ���������" << endl;
    flag = Connect();
        if (flag != 0)
	{
		cout << "[System]:";
		cout << "�������ӳɹ�" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "��������ʧ��" << endl;
		closesocket(client);
		WSACleanup();
		return 0;
	}
    loadMessage();
    sendmessage();
    flag = Disconnect();
    if (flag != 0)
	{
		cout << "[System]:";
		cout << "�ر����ӳɹ�" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "�ر�����ʧ��" << endl;
		closesocket(client);
		WSACleanup();
		return 0;
	}
    printlog();
    cout<<"�����������...";
    cin.clear();
    cin.sync();
    cin.get();
    return 0;
}
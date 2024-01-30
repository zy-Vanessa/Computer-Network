#include<iostream>
#include<WinSock2.h>
#include<time.h>
#include<fstream>
#include<iostream>
#include<windows.h>
using namespace std;
#pragma comment(lib,"ws2_32.lib")

SOCKET server;  // �������׽���
SOCKADDR_IN server_addr;  // ��������ַ�ṹ
SOCKADDR_IN router_addr;  // ·������ַ�ṹ
SOCKADDR_IN client_addr;  // �ͻ��˵�ַ�ṹ
char* sbuffer = new char[1000];  // ���ͻ�����
char* rbuffer = new char[1000];  // ���ջ�����
char* message = new char[100000000];  // ��Ϣ������
unsigned long long int messagepointer;  // ��Ϣָ�룬ָ����Ϣ�������еĵ�ǰλ��

int clen = sizeof(client_addr);
int rlen = sizeof(router_addr);

u_long blockmode = 0;  // ����ģʽ
u_long unblockmode = 1;  // ������ģʽ

const unsigned int MAX_DATA_LENGTH = 1000;
const u_int SOURCEIP = 0x7f000001;
const u_int DESIP = 0x7f000001;
const u_short SOURCEPORT = 9999;//Դ�˿���9999
const u_short DESPORT = 7777;   //�ͻ��˶˿ں���7777
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
    u_short checksum;    //У���
    u_short seq;         //���кţ�ͣ�����λֻ��0��1����״̬
    u_short ack;         //ack�ţ�ͣ�����λֻ��0��1����״̬
    u_short flag;        //״̬λ
    u_short length;      //����λ
    u_int source_ip;     //Դip��ַ
    u_int des_ip;        //Ŀ��ip��ַ
    u_short source_port; //Դ�˿ں�
    u_short des_port;    //Ŀ�Ķ˿ں�
    Header() {
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
    Header header;//����һ������ͷ
    char* recvshbuffer = new char[sizeof(header)];//����һ��������ͷһ����Ľ��ջ�����
    char* sendshbuffer = new char[sizeof(header)];//����һ��������ͷһ����ķ��ͻ�����
	cout << "[System]:";
    cout << "�ȴ�����..." << endl;
    //�ȴ���һ������
    while (true) {
        // ���׽�������Ϊ������ģʽ
        ioctlsocket(server, FIONBIO, &unblockmode);
        // ���յ�һ�����ֵ�����
        while (recvfrom(server, recvshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen)<=0) {
            // �ж����ӳ�ʱ
            if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
	            cout  << "[Failed]:";
                cout << "���ӳ�ʱ,�������Զ��Ͽ�" << endl;
                return -1;
            }
        }
        // �����յ������ݸ��Ƶ�����ͷ�ṹ����
        memcpy(&header, recvshbuffer, sizeof(header));
        //����ǵ������������������󣬲���У������ȡ��֮�����0
        if (header.flag == SYN && checksum((u_short*)(&header), sizeof(header)) == 0) {
	        cout << "[1]:";
            cout << "��һ�����ֽ����ɹ���" << endl;
            break;
        }
        else {
	        cout << "[Failed]:";
            cout << "��һ������ʧ�ܣ����ڵȴ��ش�..." << endl;
        }
    }
SECONDSHAKE:
    //׼�����͵ڶ������ֵ���Ϣ
    header.source_port = SOURCEPORT;
    header.des_port = DESPORT;
    header.flag = SYN_ACK;
    header.ack = (header.seq + 1) % 2;
    header.seq = 0;
    header.length = 0;
    header.checksum = calcksum((u_short*)(&header), sizeof(header));
    // ������ͷ���Ƶ����ͻ�����
    memcpy(sendshbuffer, &header, sizeof(header));
    // ���͵ڶ���������Ϣ
    if (sendto(server, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:�ڶ���������Ϣ����ʧ��...." << endl;
        return -1;
    }
    cout << "[2]:�ڶ���������Ϣ���ͳɹ�...." << endl;
    // �ж����ӳ�ʱ
    clock_t start = clock();
    if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
        cout << "[Failed]:���ӳ�ʱ,�������Զ��Ͽ�" << endl;
        return -1;
    }

    // �ڶ���������Ϣ�ĳ�ʱ�ش����ش�ʱֱ���ش�sendshbuffer�������
    while (recvfrom(server, recvshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:���ӳ�ʱ,�������Զ��Ͽ�" << endl;
            return -1;
        }
        if (clock() - start > MAX_TIME) {
            // �ش��ڶ���������Ϣ
            if (sendto(server, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:�ڶ���������Ϣ���·���ʧ��..." << endl;
                return -1;
            }
            cout << "[2]:�ڶ���������Ϣ���·��ͳɹ�..." << endl;
            start = clock();
        }
    }

    memcpy(&header, recvshbuffer, sizeof(header));
    if (header.flag == ACK && checksum((u_short*)(&header), sizeof(header)) == 0) {
        cout << "[3]:���������ֽ����ɹ������Կ�ʼ��������..." << endl;
        // ���õ�����������Ϣ����Ϣ
        header.source_port = SOURCEPORT;
        header.des_port = DESPORT;
        header.flag = ACK;
        header.ack = (header.seq + 1) % 2;
        header.seq = 0;
        header.length = 0;
        header.checksum = calcksum((u_short*)(&header), sizeof(header));
        memcpy(sendshbuffer, &header, sizeof(header));
        sendto(server, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen);
        cout << "[DONE]:ȷ����Ϣ����ɹ�...." << endl;
    }
    else {
        cout << "[Failed]:�����ڴ������ݰ��������ش����ȴ��ͻ��˵ȴ��ش�" << endl;
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:���ӳ�ʱ,�������Զ��Ͽ�" << endl;
            return -1;
        }
        goto SECONDSHAKE;
    }
    cout << "[WAITING]:���ڵȴ���������...." << endl;
    return 1;
}

int Disconnect() {
    Header header;
    char* sendbuffer = new char[sizeof(header)];
    char* recvbuffer = new char[sizeof(header)];
RECVWAVE1:
    while(recvfrom(server,recvbuffer,sizeof(header),0,(sockaddr*)&router_addr,&rlen)<=0){}
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == FIN && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[1]:��һ�λ�����Ϣ���ճɹ�" << endl;
    }
    else {
        cout << "[Failed]:��һ�λ�����Ϣ����ʧ��" << endl;
        goto RECVWAVE1;
    }
SENDWAVE2:
    header.seq = 0;
    header.flag = ACK;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:�ڶ��λ�����Ϣ����ʧ��" << endl;
        return -1;
    }
    cout << "[2]:�ڶ��λ�����Ϣ���ͳɹ�" << endl;
    Sleep(80);
    //�����λ���
    header.seq = 1;
    header.flag = FIN_ACK;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:�����λ�����Ϣ����ʧ��" << endl;
        return -1;
    }
    cout << "[3]:�����λ�����Ϣ���ͳɹ�" << endl;
    clock_t start = clock();
    ioctlsocket(server, FIONBIO, &unblockmode);
    while (recvfrom(server, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > MAX_TIME) {
            cout << "[Failed]:���Ĵλ�����Ϣ���ճ�ʱ...׼���ط������λ���" << endl;
            ioctlsocket(server, FIONBIO, &blockmode);
            goto SENDWAVE2;
        }
    }
    cout << "[4]:���Ĵλ�����Ϣ���ճɹ�" << endl;
FINALCHECK:
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        header.seq = 0;
        header.flag = FINAL_CHECK;
        header.checksum = calcksum((u_short*)&header, sizeof(header));
        memcpy(sendbuffer, &header, sizeof(header));
        sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen);
        cout << "[FINAL]:�ɹ�����ȷ�ϱ���" << endl;
    }
    start = clock();
    while (recvfrom(server, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > 10 * MAX_TIME) {
            cout << "[System]:�Ĵλ��ֽ����������Ͽ�����" << endl;
            return 1;
        }
    }
    goto FINALCHECK;
}

int endreceive() {
    Header header;
    char* sendbuffer = new char[sizeof(header)];
    header.flag = OVER_ACK;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header,sizeof(header));
    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) >= 0) return 1;
    cout << "[FINISH]:ȷ����Ϣ���ͳɹ�" << endl;
    return 0;
}

int receivemessage() {
    Header header;
    char* recvbuffer = new char[sizeof(header) + MAX_DATA_LENGTH];
    char* sendbuffer = new char[sizeof(header)];

RECVSEQ0:
    //����seq=0������
    while (true) {
        ioctlsocket(server, FIONBIO, &unblockmode);
        while (recvfrom(server, recvbuffer, sizeof(header) + MAX_DATA_LENGTH, 0, (sockaddr*)&router_addr, &rlen) <= 0) {}
        memcpy(&header, recvbuffer, sizeof(header));
        //�������
        if (header.flag == OVER) {
            //����OVER_ACK
            if (checksum((u_short*)&header, sizeof(header)) == 0) { if (endreceive()) { return 1; }return 0; }
            else { cout << "[ERROR]:���ݰ��������ڵȴ��ش�" << endl; goto RECVSEQ0; }
        }
        cout << "[RECV]:" << endl << "seq:" << header.seq << " checksum:" << checksum((u_short*)recvbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;
        if (header.seq == 0 && checksum((u_short*)recvbuffer, sizeof(header)+MAX_DATA_LENGTH) == 0) {
            cout << "[CHECKED]:�ɹ�����seq0���ݰ�" << endl;
            memcpy(message + messagepointer, recvbuffer + sizeof(header), header.length);
            messagepointer += header.length;
            break;
        }
        else {
            cout << "[ERROR]:���ݰ��������ڵȴ��Է����·���" << endl;
        }
    }
    header.ack = 1;
    header.seq = 0;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
SENDACK1:
    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:ack1����ʧ��" << endl;
        return -1;
    }
    clock_t start = clock();
RECVSEQ1:
    //����Ϊ������ģʽ
    ioctlsocket(server, FIONBIO, &unblockmode);
    while (recvfrom(server, recvbuffer, sizeof(header) + MAX_DATA_LENGTH, 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > MAX_TIME) {
            if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:ack1����ʧ��" << endl;
                return -1;
            }
            start = clock();
            cout << "[ERROR]:ack1��Ϣ������ʱ" << endl;
            goto SENDACK1;
        }
    }
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == OVER) {
        //�������
        if (checksum((u_short*)&header, sizeof(header) == 0)) { if (endreceive()) { return 1; }return 0; }
        else { cout << "[ERROR]:���ݰ��������ڵȴ��ش�" << endl; goto RECVSEQ0; }
    }
    cout << "[RECV]:" << endl << "seq:" << header.seq << " checksum:" << checksum((u_short*)recvbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;

    if (header.seq == 1 && checksum((u_short*)recvbuffer, sizeof(header)+MAX_DATA_LENGTH)==0) {
        cout << "[CHECKED]:�ɹ�����seq1���ݰ�" << endl;
        memcpy(message + messagepointer, recvbuffer + sizeof(header), header.length);
        messagepointer += header.length;
    }
    else {
        cout << "[ERROR]:���ݰ��𻵣����ڵȴ����´���" << endl;
        goto RECVSEQ1;
    }
    header.ack = 0;
    header.seq = 1;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
    sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen);
    goto RECVSEQ0;
}

int loadmessage() {
    if (messagepointer < 1) {
        cout << "[ERROR]:��ϢΪ�գ��޷������ļ�" << endl;
        return -1;
    }

    unsigned char fileFormat = message[0];
    string fileExtension = (fileFormat == '1') ? "png" : "txt";

    string filename = to_string(time(0)) + "." + fileExtension;
    ofstream fout(filename.c_str(), ofstream::binary);
    cout << "[STORE]:�ļ���������Ϊ " << filename << endl;

    for (unsigned long long int i = 1; i < messagepointer; i++) {
        fout << message[i];
    }

    fout.close();

    cout << "[FINISH]:�ļ��ѳɹ����ص�����" << endl;
    return 0;
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

    //�󶨷����
    server = socket(AF_INET, SOCK_DGRAM, 0);
    flag = bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));
    if (flag == 0)//�жϰ�
	{
		cout << "[System]:";
		cout << "�󶨳ɹ�" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "��ʧ��" << endl;
		closesocket(server);
		WSACleanup();
		return 0;
	}

    //�����ַ����
    int clen = sizeof(client_addr);
    int rlen = sizeof(router_addr);
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
		closesocket(server);
		WSACleanup();
		return 0;
	}
    receivemessage();
    loadmessage();
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
		closesocket(server);
		WSACleanup();
		return 0;
	}
    cout<<"�����������...";
    cin.clear();
    cin.sync();
    cin.get();
}

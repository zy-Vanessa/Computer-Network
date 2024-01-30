#include <iostream>
#include <WINSOCK2.h>
#include <ctime>
#include <fstream>
#include <cstdio>
#include <windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include "rdt.h"
#pragma warning( disable : 4996 )
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

using namespace std;

#define PORT 7878
#define ADDRSRV "127.0.0.1"
#define MAX_FILE_SIZE 10000
double MAX_TIME = CLOCKS_PER_SEC / 4;
double MAX_WAIT_TIME = MAX_TIME;
static u_int base_stage = 0;
static int windowSize = 16;
bool receivedPkt[10000] = {false};

char fileBuffer[MAX_FILE_SIZE];
char filetmp[MAX_FILE_SIZE][MAX_DATA_SIZE];

bool acceptClient(SOCKET& socket, SOCKADDR_IN& addr) {
    //��̬����洢�������ݵ��ַ�����
    char* buffer = new char[sizeof(PacketHead)];
    int len = sizeof(addr);//��ȡaddr�ṹ��Ĵ�С
    //�׽����н������ݣ�PacketHead���洢��buffer��
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &len);
    //��bufferǿ��ת��ΪPacketHead�ṹ��ָ��
    //�����յ������ݰ��Ƿ�����������ͨ����λ�����־λ�Ƿ����SYN
    //������ݰ���У����Ƿ�Ϊ��
    if ((((PacketHead*)buffer)->flag & SYN) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead)) == 0))
        cout << "[SYN_RECV]��һ�����ֳɹ�" << endl;
    else
        return false;
    //�����յ������ݰ��е����кŴ洢��base_stage��
    base_stage = ((PacketHead*)buffer)->seq;
    //����һ��ȷ�ϰ�ͷ����Ϣ���͸��ͻ���
    PacketHead head;
    head.flag |= ACK;
    head.flag |= SYN;
    head.windows = windowSize;
    head.checkSum = CheckPacketSum((u_short*)&head, sizeof(PacketHead));
    memcpy(buffer, &head, sizeof(PacketHead));
    if (sendto(socket, buffer, sizeof(PacketHead), 0, (sockaddr*)&addr, len) == -1) {
        return false;
    }
    cout << "[SYN_ACK_SEND]�ڶ������ֳɹ�" << endl;
    //���׽�������Ϊ������ģʽ
    //����recvfrom�����������������ִ��
    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);
    clock_t start = clock(); //��ʼ��ʱ
    //�ڷ�����ģʽ��ѭ���������ݣ���ʱ����һ���ش���ȷ�ϰ�
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr*)&addr, len);
            start = clock();
        }
    }
    if ((((PacketHead*)buffer)->flag & ACK) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead)) == 0)) {
        cout << "[ACK_RECV]���������ֳɹ�" << endl;
    }
    else {
        return false;
    }
    imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//����

    cout << "���û��˳ɹ��������ӣ�׼�������ļ�" << endl;
    return true;
}

Packet makePacket(u_int ack) {
    Packet pkt;
    pkt.head.ack = ack;
    pkt.head.flag |= ACK;
    pkt.head.checkSum = CheckPacketSum((u_short*)&pkt, sizeof(Packet));
    return pkt;
}
//���պ�����fileBuffer���ڴ洢���յ����ļ�����
//��������һ�� u_long ���͵�ֵ��������յ��ļ�����
u_long recvmessage(char* fileBuffer, SOCKET& socket, SOCKADDR_IN& addr) {
    u_long fileLen = 0;//��ʼ������fileLen�����ڸ��ٽ��յ����ļ�����
    //��ȡ��ַ�ṹ��Ĵ�С
    int addrLen = sizeof(addr);
    u_int expectedSeq = base_stage;
    int dataLen;

    char* pkt_buffer = new char[sizeof(Packet)];
    Packet recvPkt, sendPkt = makePacket(base_stage);
    clock_t start;
    bool clockStart = false;
    while (true) {
        memset(pkt_buffer, 0, sizeof(Packet));
        recvfrom(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR*)&addr, &addrLen);
        memcpy(&recvPkt, pkt_buffer, sizeof(Packet));
        //�жϽ��յ��ı���flag��ENDλ�Ƿ���Ϊ1
        //�������˵���ļ��������
        if (recvPkt.head.flag & END && CheckPacketSum((u_short*)&recvPkt, sizeof(PacketHead)) == 0) {
            cout << "�������" << endl;
            PacketHead endPacket;
            endPacket.flag |= ACK;
            endPacket.checkSum = CheckPacketSum((u_short*)&endPacket, sizeof(PacketHead));
            memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
            sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
            //[3-3]�ѻ���İ�������fileBuffer
            if(fileLen % MAX_DATA_SIZE){
                for(int i = 0 ; i < fileLen / MAX_DATA_SIZE ; i++)
                    memcpy(fileBuffer + i * MAX_DATA_SIZE, filetmp[i], MAX_DATA_SIZE);
                memcpy(fileBuffer + fileLen - dataLen, filetmp[fileLen / MAX_DATA_SIZE], fileLen % MAX_DATA_SIZE);
                return fileLen;
            }
            else{
                for(int i = 0 ; i < fileLen / MAX_DATA_SIZE ; i++)
                    memcpy(fileBuffer + i * MAX_DATA_SIZE, filetmp[i], MAX_DATA_SIZE);
                return fileLen;
            }
        }
        //[3-3]�ж��յ���seq�ǲ��ǵ���base������򻬶�����
        if (recvPkt.head.seq == base_stage && CheckPacketSum((u_short*)&recvPkt, sizeof(Packet)) == 0) {
            sendPkt = makePacket(recvPkt.head.seq);
            cout<<"[send]: ack = " << recvPkt.head.seq <<endl;
            if(receivedPkt[recvPkt.head.seq] == false){
                //���汨��
                dataLen = recvPkt.head.bufSize;
                memcpy(filetmp[recvPkt.head.seq], recvPkt.data, dataLen);
                fileLen += dataLen;
                receivedPkt[recvPkt.head.seq] = true;
                while(receivedPkt[base_stage]){
                    base_stage++;
                    expectedSeq++;
                    cout << "[���ڻ���]base:" << base_stage << " expectedSeq:" << expectedSeq<<endl;
                }
            }
            memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR*)&addr, addrLen);
            continue;
        }
        else if(recvPkt.head.seq != base_stage && recvPkt.head.seq < base_stage + windowSize && CheckPacketSum((u_short*)&recvPkt, sizeof(Packet)) == 0){
            sendPkt = makePacket(recvPkt.head.seq);
            cout<<"[send]: ack = " << recvPkt.head.seq <<endl;
            if(receivedPkt[recvPkt.head.seq] == false){
                //���汨��
                dataLen = recvPkt.head.bufSize;
                memcpy(filetmp[recvPkt.head.seq], recvPkt.data, dataLen);
                fileLen += dataLen;
                receivedPkt[recvPkt.head.seq] = true;
            }
            memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR*)&addr, addrLen);
            continue;
        }
        cout << "wait head:" << expectedSeq << endl;
        cout << "recv head:" << recvPkt.head.seq << endl;
        memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
        sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR*)&addr, addrLen);
        cout<<"[send]: ack = " << base_stage <<endl;
    }
}

bool disConnect(SOCKET& socket, SOCKADDR_IN& addr) {
    int addrLen = sizeof(addr);
    char* buffer = new char[sizeof(PacketHead)];
    //�׽��ֽ������ݰ��洢��buffer��
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &addrLen);
    //���������ݰ��еı�־λ��У���ȷ���Ƿ��ǵ�һ�λ�������
    if ((((PacketHead*)buffer)->flag & FIN) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead) == 0))) {
        cout << "[FIN_RECV]��һ�λ�����ɣ��û��˶Ͽ�" << endl;
    }
    else {
        return false;
    }
    //����һ��ȷ�����ݰ�closeHead����־λ����ΪACK�����㲢���У���
    //��ȷ�����ݰ����͸��ͻ��ˣ���ʾ������Ѿ��յ���һ�λ�������
    PacketHead closeHead;
    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = CheckPacketSum((u_short*)&closeHead, sizeof(PacketHead));
    memcpy(buffer, &closeHead, sizeof(PacketHead));
    sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
    cout << "[ACK_SEND]�ڶ��λ������" << endl;
    //����һ�������������ݰ�closeHead����־λ����ΪFIN�����㲢���У��ͷ����ͻ���
    closeHead.flag = 0;
    closeHead.flag |= FIN;
    closeHead.checkSum = CheckPacketSum((u_short*)&closeHead, sizeof(PacketHead));
    memcpy(buffer, &closeHead, sizeof(PacketHead));
    sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
    cout << "[FIN_SEND]�����λ������" << endl;
    //�׽�������Ϊ������ģʽ���Ա������ʱ����
    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);
    clock_t start = clock();
    //�ڷ�����ģʽ�£�ѭ���ȴ����յ��Ĵλ��ֵ�ȷ�����ݰ��������ʱ�����·��͵����λ�������
    while (recvfrom(socket, buffer, sizeof(PacketHead), 0, (sockaddr*)&addr, &addrLen) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &closeHead, sizeof(PacketHead));
            sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
            start = clock();
        }
    }
    //����־λ��У���
    if ((((PacketHead*)buffer)->flag & ACK) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead) == 0))) {
        cout << "[ACK_RECV]���Ĵλ�����ɣ����ӹر�" << endl;
    }
    else {
        return false;
    }
    closesocket(socket);
    return true;
}
int main() {
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //����ʧ��
        cout << "[ERROR]����DLLʧ��" << endl;
        return -1;
    }
    SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);
    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV);
    bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
    SOCKADDR_IN addrClient;
    //�������ֽ�������
    if (!acceptClient(sockSrv, addrClient)) {
        cout << "[ERROR]����ʧ��" << endl;
        return 0;
    }
    //char fileBuffer[MAX_FILE_SIZE];
    //�ɿ����ݴ������
    u_long fileLen = recvmessage(fileBuffer, sockSrv, addrClient);
    //�Ĵλ��ֶϿ�����
    if (!disConnect(sockSrv, addrClient)) {
        cout << "[ERROR]�Ͽ�ʧ��" << endl;
        return 0;
    }
    //д�븴���ļ�
    string filename = R"(save.jpg)";
    ofstream outfile(filename, ios::binary);
    if (!outfile.is_open()) {
        cout << "[ERROR]���ļ�����" << endl;
        return 0;
    }
    cout << fileLen << endl;
    outfile.write(fileBuffer, fileLen);
    outfile.close();

    cout << "�ļ��������" << endl;
    system("PAUSE");
    return 1;
}
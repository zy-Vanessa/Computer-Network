#include <iostream>
#include <WINSOCK2.h>
#include <ctime>
#include <fstream>
#include <cstdio>
#include <windows.h>
#include <iostream>
#include <thread>
#include <assert.h>
#include <chrono>
#include <mutex>
#include "rdt.h"
#pragma warning( disable : 4996 )
#pragma comment(lib, "ws2_32.lib")  //���� ws2_32.dll
#pragma comment(lib, "winmm.lib")
using namespace std;
#define min(a, b) a>b?b:a
static SOCKADDR_IN addrSrv;
static int addrLen = sizeof(addrSrv);
#define PORT 7879
double MAX_TIME = CLOCKS_PER_SEC / 4;
string ADDRSRV;
static int windowSize = 32;
static unsigned int base = 0;//���ֽ׶�ȷ���ĳ�ʼ���к�
static unsigned int nextSeqNum = 0;
static Packet sendPkt;
static int sendIndex = 0;
static bool stopTimer = false;
static clock_t start;
static int packetNum;
static bool ackReceived[100000]={0};
static int sendAgain = 0;
mutex mutexLock;

bool connectToServer(SOCKET& socket, SOCKADDR_IN& addr) {
    int len = sizeof(addr);
    //�����һ���������ݰ�head����־λ����ΪSYN�����㲢���У���
    PacketHead head;
    head.flag |= SYN;
    head.seq = base;
    head.checkSum = CheckPacketSum((u_short*)&head, sizeof(head));
    //��̬����洢���ݵ��ַ�����buffer��
    char* buffer = new char[sizeof(head)];
    //�������ݰ����͸������
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, len);
    cout << "[SYN_SEND]��һ�����ֳɹ�" << endl;
    clock_t start = clock(); //��ʼ��ʱ
    //�ȴ�����˵�ȷ�����ݰ��������ʱ�����·��͵�һ�����ֵ����ݰ�
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &head, sizeof(head));
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr*)&addr, len);
            start = clock();
        }
    }
    //�����ȷ���յ�ȷ�����ݰ�������־λ��У���
    //�����Ԥ�ڵ�SYN+ACK��������ֳɹ�
    memcpy(&head, buffer, sizeof(head));
    if ((head.flag & ACK) && (CheckPacketSum((u_short*)&head, sizeof(head)) == 0) && (head.flag & SYN)) {
        cout << "[ACK_RECV]�ڶ������ֳɹ�" << endl;
    }
    else {
        return false;
    }
    // �ͻ����յ�����˵Ĵ��ڴ�С���ʼ�����ڴ�СΪ��ͬ��С
    windowSize = head.windows;
    //����˽������ӣ��ͻ��������˷���ACK����
    //���¹���һ��ACK���ݱ�head���������
    head.flag = 0;
    head.flag |= ACK;
    head.checkSum = 0;
    head.checkSum = (CheckPacketSum((u_short*)&head, sizeof(head)));
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, len);
    //�ȴ�����MAX_TIME�����û���յ���Ϣ˵��ACKû�ж���
    start = clock();
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &len) <= 0)
            continue;
        //����˵��ACK��ʧ��Ҫ�ش����ݱ�
        memcpy(buffer, &head, sizeof(head));
        sendto(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, len);
        start = clock();
    }
    cout << "[ACK_SEND]�������ֳɹ�" << endl;
    cout << "�ɹ���������������ӣ�׼����������" << endl;
    return true;
}

Packet makePacket(u_int seq, char* data, int len) {
    Packet pkt;
    pkt.head.seq = seq;
    pkt.head.bufSize = len;
    memcpy(pkt.data, data, len);
    pkt.head.checkSum = CheckPacketSum((u_short*)&pkt, sizeof(Packet));
    return pkt;
}
//�ж�nextSeq���к��ڴ����е�ƫ����
u_int waitingNum(u_int nextSeq) {
    if (nextSeq >= base)
        return nextSeq - base;
    return nextSeq + MAX_SEQ - base;
}
//�ж�seq���к����ݰ��Ƿ��ڵ�ǰ���ʹ�����
bool inWindows(u_int seq) {
    if (seq >= base && seq < base + windowSize)
        return true;
    if (seq < base && seq < ((base + windowSize) % MAX_SEQ))
        return true;
    return false;
}
//������ձ��Ĳ����д��ڻ���
DWORD WINAPI ACKHandler(LPVOID param) {
    SOCKET* clientSock = (SOCKET*)param;
    char recvBuffer[sizeof(Packet)];
    Packet recvPacket;
    int wrongACK = -1;
    int wrongCount = 0;
    while (true) {
        //�ж���û��Ӧ����
        if (recvfrom(*clientSock, recvBuffer, sizeof(Packet), 0, (SOCKADDR*)&addrSrv, &addrLen) > 0) {
            memcpy(&recvPacket, recvBuffer, sizeof(Packet));
            if (CheckPacketSum((u_short*)&recvPacket, sizeof(Packet)) == 0 && recvPacket.head.flag & ACK) {
                mutexLock.lock();
                //�ж����е�ackֵ�Ƿ��ڴ�����
                if (base < (recvPacket.head.ack + 1) && recvPacket.head.ack < (base+windowSize)) {
                    cout << "����Ӧ��";
                    ShowPacket(&recvPacket);
                    //[3-3]�����ǰ�յ���ack����base
                    if(base == recvPacket.head.ack){
                        ackReceived[recvPacket.head.ack] = true;
                        packetTimers[recvPacket.head.ack].active = false; // ֹͣ��Ӧ���ļ�ʱ��
                        while(ackReceived[base]){
                            base++;
                            cout << "[���ڻ���]base:" << base << " nextSeq:" << nextSeqNum << " endWindow:"
                            << base + windowSize << endl;
                        }
                    }
                    else{
                        ackReceived[recvPacket.head.ack] = true;
                        packetTimers[recvPacket.head.ack].active = false; // ֹͣ��Ӧ���ļ�ʱ��
                    }
                }
                mutexLock.unlock();
                if (base == nextSeqNum)
                    stopTimer = true;
                else {
                    start = clock();
                    stopTimer = false;
                }
                //base == packetNum��˵�������Ѿ�ȫ���������
                if (base == packetNum)
                    return 0;
            }
        }
    }
}

void sendmessage(u_long len, char* fileBuffer, SOCKET& socket, SOCKADDR_IN& addr) {
    //�����ļ����ݰ�������
    packetNum = int(len / MAX_DATA_SIZE) + (len % MAX_DATA_SIZE ? 1 : 0);
    int losepacket = 0;
    int packetDataLen;
    int addrLen = sizeof(addr);
    char* data_buffer = new char[sizeof(Packet)], * pkt_buffer = new char[sizeof(Packet)];
    nextSeqNum = base;
    cout << "�����ļ����ݳ���Ϊ" << len << "Bytes,��Ҫ����" << packetNum << "�����ݰ�" << endl;
    auto nBeginTime = chrono::system_clock::now();
    auto nEndTime = nBeginTime;
    HANDLE ackhandler = CreateThread(nullptr, 0, ACKHandler, LPVOID(&socket), 0, nullptr);
    while (true) {
        //�������ݰ������ܸ������������
        if (base == packetNum) {
            nEndTime = chrono::system_clock::now();
            auto duration = chrono::duration_cast<chrono::microseconds>(nEndTime - nBeginTime);
            // ����������
            double throughput = (static_cast<double>(len) / duration.count()) * 1e6; // �ֽ�/��
            printf("���ݴ���ʱ��Ϊ %lf s, ������Ϊ %lf bytes/s\n",
                double(duration.count()) * chrono::microseconds::period::num /
                chrono::microseconds::period::den, throughput);
            //���㶪����
            //double losepkt = static_cast<double>(losepacket) / packetNum;
            //cout << "������Ϊ" << losepkt << endl;
            CloseHandle(ackhandler);
            //����һ����־λΪEND�����ݰ�endPacket
            PacketHead endPacket;
            endPacket.flag |= END;
            endPacket.checkSum = CheckPacketSum((u_short*)&endPacket, sizeof(PacketHead));
            memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
            cout << "���ڷ��ͣ�";
            ShowPacket((Packet*)&endPacket);
            sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
            //����ȷ�ϣ������ʱ�����·��ͽ�����־��
            while (recvfrom(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &addrLen) <= 0) {
                if (clock() - start >= MAX_TIME) {
                    start = clock();
                    goto resend;
                }
            }
            //���Packet�ṹ��ͷ����flag�ֶ��Ƿ����ACK��־��
            //����ǣ����ļ��������
            if (((PacketHead*)(pkt_buffer))->flag & ACK &&
                CheckPacketSum((u_short*)pkt_buffer, sizeof(PacketHead)) == 0) {
                cout << "�ļ��������" << endl;
                return;
            }
        resend:
            continue;
        }
        packetDataLen = min(MAX_DATA_SIZE, len - sendIndex * MAX_DATA_SIZE);
        mutexLock.lock();//����������������Դ
        //[3-3]����ж�����ֻ����û���յ�ack�İ�
        if (inWindows(nextSeqNum) && nextSeqNum < packetNum && ackReceived[nextSeqNum] == false) {
            memcpy(data_buffer, fileBuffer + nextSeqNum * MAX_DATA_SIZE, packetDataLen);
            sendPkt = makePacket(nextSeqNum, data_buffer, packetDataLen);
            memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
            packetTimers[nextSeqNum].sendTime = clock(); // ��¼����ʱ��
            packetTimers[nextSeqNum].active = true; // ���ü�ʱ��Ϊ�״̬
            nextSeqNum = (nextSeqNum + 1) % MAX_SEQ;
            //ShowPacket(&sendPkt[(int)waitingNum(nextSeqNum)]);
            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR*)&addr, addrLen);
            cout<<"�ѷ���seq = " << nextSeqNum - 1 << "�İ�"<<endl;
            sendIndex++;
        }
        mutexLock.unlock();
    time_out:
        mutexLock.lock();
        for (int i = base; i != nextSeqNum; i++) {
            if (packetTimers[i].active && (clock() - packetTimers[i].sendTime >= MAX_TIME)) {
                // �������ʱ���ط������
                packetDataLen = min(MAX_DATA_SIZE, len - i * MAX_DATA_SIZE);
                memcpy(data_buffer, fileBuffer + i * MAX_DATA_SIZE, packetDataLen);
                sendPkt = makePacket(i, data_buffer, packetDataLen);
                memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR*)&addr, addrLen);
                cout << "��ʱ���������·���seq = " << i << "�İ�" << endl;
                packetTimers[i].sendTime = clock(); // ���ü�ʱ��
                ShowPacket(&sendPkt);
            }
        }
        mutexLock.unlock();
    }
}
bool disConnect(SOCKET& socket, SOCKADDR_IN& addr) {
    char* buffer = new char[sizeof(PacketHead)];
    //����һ����־λΪFIN�����ݰ�closeHead�������������
    PacketHead closeHead;
    closeHead.flag |= FIN;
    closeHead.checkSum = CheckPacketSum((u_short*)&closeHead, sizeof(PacketHead));
    memcpy(buffer, &closeHead, sizeof(PacketHead));
    if (sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen) != SOCKET_ERROR)
        cout << "[FIN_SEND]��һ�λ��ֳɹ�" << endl;
    else
        return false;
    clock_t start = clock();//��ʱ
    //�ȴ����շ���˵�ȷ�ϱ��ģ������ʱ���ط���һ�λ���
    while (recvfrom(socket, buffer, sizeof(PacketHead), 0, (sockaddr*)&addr, &addrLen) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &closeHead, sizeof(PacketHead));
            sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
            start = clock();
        }
    }
    //����־λ��У���
    if ((((PacketHead*)buffer)->flag & ACK) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead) == 0))) {
        cout << "[ACK_RECV]�ڶ��λ��ֳɹ����ͻ����Ѿ��Ͽ�" << endl;
    }
    else {
        return false;
    }
    //����ģʽ
    u_long imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);
    //�������Է���˵Ļ��ֶ�������
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &addrLen);
    memcpy(&closeHead, buffer, sizeof(PacketHead));
    //����־λ��У���
    if ((((PacketHead*)buffer)->flag & FIN) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead) == 0))) {
        cout << "[FIN_RECV]�����λ��ֳɹ����������Ͽ�" << endl;
    }
    else {
        return false;
    }
    //������ģʽ��recvfrom�����������������ִ��
    imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);
    //����һ��ACK���Ļظ��������
    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = CheckPacketSum((u_short*)&closeHead, sizeof(PacketHead));

    memcpy(buffer, &closeHead, sizeof(PacketHead));
    sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
    start = clock();
    //�ڷ�����ģʽ��ѭ���ȴ�����MAX_TIME�����û���յ���Ϣ˵��ACKû�ж�ʧ
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &addrLen) <= 0)
            continue;
        //����˵��ACK��ʧ��Ҫ�ش�
        memcpy(buffer, &closeHead, sizeof(PacketHead));
        sendto(socket, buffer, sizeof(PacketHead), 0, (sockaddr*)&addr, addrLen);
        start = clock();
    }
    cout << "[ACK_SEND]���Ĵλ��ֳɹ��������ѹر�" << endl;
    closesocket(socket);
    return true;
}
int main() {
    //��ʼ���׽��ֿ�
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //����ʧ��
        cout << "����DLLʧ��" << endl;
        return -1;
    }
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, 0);
    //������ģʽ
    u_long imode = 1;
    ioctlsocket(sockClient, FIONBIO, &imode);
    cout << "����������������ĵ�ַ" << endl;
    cin >> ADDRSRV;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());
    //����
    if (!connectToServer(sockClient, addrSrv)) {
        cout << "[ERROR]����ʧ��" << endl;
        return 0;
    }
    string filename;
    cout << "��������Ҫ������ļ���" << endl;
    cin >> filename;
    ifstream infile(filename, ifstream::binary);
    if (!infile.is_open()) {
        cout << "[ERROR]�޷����ļ�" << endl;
        return 0;
    }
    infile.seekg(0, infile.end);
    u_long fileLen = infile.tellg();
    infile.seekg(0, infile.beg);
    cout << fileLen << endl;
    char* fileBuffer = new char[fileLen];
    infile.read(fileBuffer, fileLen);
    infile.close();
    //cout.write(fileBuffer,fileLen);
    cout << "��ʼ����" << endl;
    sendmessage(fileLen, fileBuffer, sockClient, addrSrv);
    if (!disConnect(sockClient, addrSrv)) {
        cout << "[ERROR]�Ͽ�ʧ��" << endl;
        return 0;
    }
    cout << "�ļ��������" << endl;
    system("PAUSE");
    return 1;
}
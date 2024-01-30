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
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#pragma comment(lib, "winmm.lib")
using namespace std;
#define min(a, b) a>b?b:a
static SOCKADDR_IN addrSrv;
static int addrLen = sizeof(addrSrv);
#define PORT 7879
double MAX_TIME = CLOCKS_PER_SEC / 4;
string ADDRSRV;
static int windowSize = 32;
static unsigned int base = 0;//握手阶段确定的初始序列号
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
    //构造第一次握手数据包head，标志位设置为SYN，计算并填充校验和
    PacketHead head;
    head.flag |= SYN;
    head.seq = base;
    head.checkSum = CheckPacketSum((u_short*)&head, sizeof(head));
    //动态分配存储数据的字符数组buffer中
    char* buffer = new char[sizeof(head)];
    //复制数据包发送给服务端
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, len);
    cout << "[SYN_SEND]第一次握手成功" << endl;
    clock_t start = clock(); //开始计时
    //等待服务端的确认数据包，如果超时则重新发送第一次握手的数据包
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &head, sizeof(head));
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr*)&addr, len);
            start = clock();
        }
    }
    //如果正确接收到确认数据包，检查标志位及校验和
    //如果是预期的SYN+ACK则表明握手成功
    memcpy(&head, buffer, sizeof(head));
    if ((head.flag & ACK) && (CheckPacketSum((u_short*)&head, sizeof(head)) == 0) && (head.flag & SYN)) {
        cout << "[ACK_RECV]第二次握手成功" << endl;
    }
    else {
        return false;
    }
    // 客户端收到服务端的窗口大小后初始化窗口大小为相同大小
    windowSize = head.windows;
    //服务端建立连接，客户端向服务端发送ACK报文
    //重新构造一个ACK数据报head发给服务端
    head.flag = 0;
    head.flag |= ACK;
    head.checkSum = 0;
    head.checkSum = (CheckPacketSum((u_short*)&head, sizeof(head)));
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, len);
    //等待两个MAX_TIME，如果没有收到消息说明ACK没有丢包
    start = clock();
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &len) <= 0)
            continue;
        //否则说明ACK丢失需要重传数据报
        memcpy(buffer, &head, sizeof(head));
        sendto(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, len);
        start = clock();
    }
    cout << "[ACK_SEND]三次握手成功" << endl;
    cout << "成功与服务器建立连接，准备发送数据" << endl;
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
//判断nextSeq序列号在窗口中的偏移量
u_int waitingNum(u_int nextSeq) {
    if (nextSeq >= base)
        return nextSeq - base;
    return nextSeq + MAX_SEQ - base;
}
//判断seq序列号数据包是否在当前发送窗口中
bool inWindows(u_int seq) {
    if (seq >= base && seq < base + windowSize)
        return true;
    if (seq < base && seq < ((base + windowSize) % MAX_SEQ))
        return true;
    return false;
}
//处理接收报文并进行窗口滑动
DWORD WINAPI ACKHandler(LPVOID param) {
    SOCKET* clientSock = (SOCKET*)param;
    char recvBuffer[sizeof(Packet)];
    Packet recvPacket;
    int wrongACK = -1;
    int wrongCount = 0;
    while (true) {
        //判断有没有应答报文
        if (recvfrom(*clientSock, recvBuffer, sizeof(Packet), 0, (SOCKADDR*)&addrSrv, &addrLen) > 0) {
            memcpy(&recvPacket, recvBuffer, sizeof(Packet));
            if (CheckPacketSum((u_short*)&recvPacket, sizeof(Packet)) == 0 && recvPacket.head.flag & ACK) {
                mutexLock.lock();
                //判断其中的ack值是否在窗口内
                if (base < (recvPacket.head.ack + 1) && recvPacket.head.ack < (base+windowSize)) {
                    cout << "接收应答：";
                    ShowPacket(&recvPacket);
                    //[3-3]如果当前收到的ack等于base
                    if(base == recvPacket.head.ack){
                        ackReceived[recvPacket.head.ack] = true;
                        packetTimers[recvPacket.head.ack].active = false; // 停止对应包的计时器
                        while(ackReceived[base]){
                            base++;
                            cout << "[窗口滑动]base:" << base << " nextSeq:" << nextSeqNum << " endWindow:"
                            << base + windowSize << endl;
                        }
                    }
                    else{
                        ackReceived[recvPacket.head.ack] = true;
                        packetTimers[recvPacket.head.ack].active = false; // 停止对应包的计时器
                    }
                }
                mutexLock.unlock();
                if (base == nextSeqNum)
                    stopTimer = true;
                else {
                    start = clock();
                    stopTimer = false;
                }
                //base == packetNum，说明数据已经全部传输完毕
                if (base == packetNum)
                    return 0;
            }
        }
    }
}

void sendmessage(u_long len, char* fileBuffer, SOCKET& socket, SOCKADDR_IN& addr) {
    //计算文件数据包的数量
    packetNum = int(len / MAX_DATA_SIZE) + (len % MAX_DATA_SIZE ? 1 : 0);
    int losepacket = 0;
    int packetDataLen;
    int addrLen = sizeof(addr);
    char* data_buffer = new char[sizeof(Packet)], * pkt_buffer = new char[sizeof(Packet)];
    nextSeqNum = base;
    cout << "本次文件数据长度为" << len << "Bytes,需要传输" << packetNum << "个数据包" << endl;
    auto nBeginTime = chrono::system_clock::now();
    auto nEndTime = nBeginTime;
    HANDLE ackhandler = CreateThread(nullptr, 0, ACKHandler, LPVOID(&socket), 0, nullptr);
    while (true) {
        //传输数据包等于总个数即传输完成
        if (base == packetNum) {
            nEndTime = chrono::system_clock::now();
            auto duration = chrono::duration_cast<chrono::microseconds>(nEndTime - nBeginTime);
            // 计算吞吐率
            double throughput = (static_cast<double>(len) / duration.count()) * 1e6; // 字节/秒
            printf("数据传输时间为 %lf s, 吞吐率为 %lf bytes/s\n",
                double(duration.count()) * chrono::microseconds::period::num /
                chrono::microseconds::period::den, throughput);
            //计算丢包率
            //double losepkt = static_cast<double>(losepacket) / packetNum;
            //cout << "丢包率为" << losepkt << endl;
            CloseHandle(ackhandler);
            //构造一个标志位为END的数据包endPacket
            PacketHead endPacket;
            endPacket.flag |= END;
            endPacket.checkSum = CheckPacketSum((u_short*)&endPacket, sizeof(PacketHead));
            memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
            cout << "正在发送：";
            ShowPacket((Packet*)&endPacket);
            sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
            //接收确认，如果超时则重新发送结束标志包
            while (recvfrom(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &addrLen) <= 0) {
                if (clock() - start >= MAX_TIME) {
                    start = clock();
                    goto resend;
                }
            }
            //检查Packet结构体头部的flag字段是否包含ACK标志，
            //如果是，则文件传输完成
            if (((PacketHead*)(pkt_buffer))->flag & ACK &&
                CheckPacketSum((u_short*)pkt_buffer, sizeof(PacketHead)) == 0) {
                cout << "文件传输完成" << endl;
                return;
            }
        resend:
            continue;
        }
        packetDataLen = min(MAX_DATA_SIZE, len - sendIndex * MAX_DATA_SIZE);
        mutexLock.lock();//互斥锁保护共享资源
        //[3-3]添加判断条件只发送没有收到ack的包
        if (inWindows(nextSeqNum) && nextSeqNum < packetNum && ackReceived[nextSeqNum] == false) {
            memcpy(data_buffer, fileBuffer + nextSeqNum * MAX_DATA_SIZE, packetDataLen);
            sendPkt = makePacket(nextSeqNum, data_buffer, packetDataLen);
            memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
            packetTimers[nextSeqNum].sendTime = clock(); // 记录发送时间
            packetTimers[nextSeqNum].active = true; // 设置计时器为活动状态
            nextSeqNum = (nextSeqNum + 1) % MAX_SEQ;
            //ShowPacket(&sendPkt[(int)waitingNum(nextSeqNum)]);
            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR*)&addr, addrLen);
            cout<<"已发送seq = " << nextSeqNum - 1 << "的包"<<endl;
            sendIndex++;
        }
        mutexLock.unlock();
    time_out:
        mutexLock.lock();
        for (int i = base; i != nextSeqNum; i++) {
            if (packetTimers[i].active && (clock() - packetTimers[i].sendTime >= MAX_TIME)) {
                // 如果包超时，重发这个包
                packetDataLen = min(MAX_DATA_SIZE, len - i * MAX_DATA_SIZE);
                memcpy(data_buffer, fileBuffer + i * MAX_DATA_SIZE, packetDataLen);
                sendPkt = makePacket(i, data_buffer, packetDataLen);
                memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR*)&addr, addrLen);
                cout << "超时，正在重新发送seq = " << i << "的包" << endl;
                packetTimers[i].sendTime = clock(); // 重置计时器
                ShowPacket(&sendPkt);
            }
        }
        mutexLock.unlock();
    }
}
bool disConnect(SOCKET& socket, SOCKADDR_IN& addr) {
    char* buffer = new char[sizeof(PacketHead)];
    //构造一个标志位为FIN的数据包closeHead，发起断连请求
    PacketHead closeHead;
    closeHead.flag |= FIN;
    closeHead.checkSum = CheckPacketSum((u_short*)&closeHead, sizeof(PacketHead));
    memcpy(buffer, &closeHead, sizeof(PacketHead));
    if (sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen) != SOCKET_ERROR)
        cout << "[FIN_SEND]第一次挥手成功" << endl;
    else
        return false;
    clock_t start = clock();//计时
    //等待接收服务端的确认报文，如果超时则重发第一次挥手
    while (recvfrom(socket, buffer, sizeof(PacketHead), 0, (sockaddr*)&addr, &addrLen) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &closeHead, sizeof(PacketHead));
            sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
            start = clock();
        }
    }
    //检查标志位和校验和
    if ((((PacketHead*)buffer)->flag & ACK) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead) == 0))) {
        cout << "[ACK_RECV]第二次挥手成功，客户端已经断开" << endl;
    }
    else {
        return false;
    }
    //阻塞模式
    u_long imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);
    //接收来自服务端的挥手断连申请
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &addrLen);
    memcpy(&closeHead, buffer, sizeof(PacketHead));
    //检查标志位和校验和
    if ((((PacketHead*)buffer)->flag & FIN) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead) == 0))) {
        cout << "[FIN_RECV]第三次挥手成功，服务器断开" << endl;
    }
    else {
        return false;
    }
    //非阻塞模式，recvfrom函数不会阻塞程序的执行
    imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);
    //构造一个ACK报文回复给服务端
    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = CheckPacketSum((u_short*)&closeHead, sizeof(PacketHead));

    memcpy(buffer, &closeHead, sizeof(PacketHead));
    sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
    start = clock();
    //在非阻塞模式下循环等待两个MAX_TIME，如果没有收到消息说明ACK没有丢失
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &addrLen) <= 0)
            continue;
        //否则说明ACK丢失需要重传
        memcpy(buffer, &closeHead, sizeof(PacketHead));
        sendto(socket, buffer, sizeof(PacketHead), 0, (sockaddr*)&addr, addrLen);
        start = clock();
    }
    cout << "[ACK_SEND]第四次挥手成功，连接已关闭" << endl;
    closesocket(socket);
    return true;
}
int main() {
    //初始化套接字库
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //加载失败
        cout << "加载DLL失败" << endl;
        return -1;
    }
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, 0);
    //非阻塞模式
    u_long imode = 1;
    ioctlsocket(sockClient, FIONBIO, &imode);
    cout << "请输入聊天服务器的地址" << endl;
    cin >> ADDRSRV;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());
    //建连
    if (!connectToServer(sockClient, addrSrv)) {
        cout << "[ERROR]连接失败" << endl;
        return 0;
    }
    string filename;
    cout << "请输入需要传输的文件名" << endl;
    cin >> filename;
    ifstream infile(filename, ifstream::binary);
    if (!infile.is_open()) {
        cout << "[ERROR]无法打开文件" << endl;
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
    cout << "开始传输" << endl;
    sendmessage(fileLen, fileBuffer, sockClient, addrSrv);
    if (!disConnect(sockClient, addrSrv)) {
        cout << "[ERROR]断开失败" << endl;
        return 0;
    }
    cout << "文件传输完成" << endl;
    system("PAUSE");
    return 1;
}
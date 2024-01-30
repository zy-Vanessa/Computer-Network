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
    //动态分配存储接收数据的字符数组
    char* buffer = new char[sizeof(PacketHead)];
    int len = sizeof(addr);//获取addr结构体的大小
    //套接字中接收数据（PacketHead）存储在buffer中
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &len);
    //将buffer强制转换为PacketHead结构体指针
    //检查接收到的数据包是否是握手请求，通过按位与检查标志位是否包含SYN
    //检查数据包的校验和是否为零
    if ((((PacketHead*)buffer)->flag & SYN) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead)) == 0))
        cout << "[SYN_RECV]第一次握手成功" << endl;
    else
        return false;
    //将接收到的数据包中的序列号存储到base_stage中
    base_stage = ((PacketHead*)buffer)->seq;
    //构造一个确认包头部信息发送给客户端
    PacketHead head;
    head.flag |= ACK;
    head.flag |= SYN;
    head.windows = windowSize;
    head.checkSum = CheckPacketSum((u_short*)&head, sizeof(PacketHead));
    memcpy(buffer, &head, sizeof(PacketHead));
    if (sendto(socket, buffer, sizeof(PacketHead), 0, (sockaddr*)&addr, len) == -1) {
        return false;
    }
    cout << "[SYN_ACK_SEND]第二次握手成功" << endl;
    //将套接字设置为非阻塞模式
    //这样recvfrom函数不会阻塞程序的执行
    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);
    clock_t start = clock(); //开始计时
    //在非阻塞模式下循环接收数据，超时则发送一个重传的确认包
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr*)&addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr*)&addr, len);
            start = clock();
        }
    }
    if ((((PacketHead*)buffer)->flag & ACK) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead)) == 0)) {
        cout << "[ACK_RECV]第三次握手成功" << endl;
    }
    else {
        return false;
    }
    imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//阻塞

    cout << "与用户端成功建立连接，准备接收文件" << endl;
    return true;
}

Packet makePacket(u_int ack) {
    Packet pkt;
    pkt.head.ack = ack;
    pkt.head.flag |= ACK;
    pkt.head.checkSum = CheckPacketSum((u_short*)&pkt, sizeof(Packet));
    return pkt;
}
//接收函数，fileBuffer用于存储接收到的文件数据
//函数返回一个 u_long 类型的值，代表接收的文件长度
u_long recvmessage(char* fileBuffer, SOCKET& socket, SOCKADDR_IN& addr) {
    u_long fileLen = 0;//初始化变量fileLen，用于跟踪接收到的文件长度
    //获取地址结构体的大小
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
        //判断接收到的报文flag中END位是否被置为1
        //如果是则说明文件传输完毕
        if (recvPkt.head.flag & END && CheckPacketSum((u_short*)&recvPkt, sizeof(PacketHead)) == 0) {
            cout << "传输完毕" << endl;
            PacketHead endPacket;
            endPacket.flag |= ACK;
            endPacket.checkSum = CheckPacketSum((u_short*)&endPacket, sizeof(PacketHead));
            memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
            sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
            //[3-3]把缓存的包都填入fileBuffer
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
        //[3-3]判断收到的seq是不是等于base，相等则滑动窗口
        if (recvPkt.head.seq == base_stage && CheckPacketSum((u_short*)&recvPkt, sizeof(Packet)) == 0) {
            sendPkt = makePacket(recvPkt.head.seq);
            cout<<"[send]: ack = " << recvPkt.head.seq <<endl;
            if(receivedPkt[recvPkt.head.seq] == false){
                //保存报文
                dataLen = recvPkt.head.bufSize;
                memcpy(filetmp[recvPkt.head.seq], recvPkt.data, dataLen);
                fileLen += dataLen;
                receivedPkt[recvPkt.head.seq] = true;
                while(receivedPkt[base_stage]){
                    base_stage++;
                    expectedSeq++;
                    cout << "[窗口滑动]base:" << base_stage << " expectedSeq:" << expectedSeq<<endl;
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
                //保存报文
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
    //套接字接收数据包存储在buffer中
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, &addrLen);
    //检查接收数据包中的标志位及校验和确定是否是第一次挥手请求
    if ((((PacketHead*)buffer)->flag & FIN) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead) == 0))) {
        cout << "[FIN_RECV]第一次挥手完成，用户端断开" << endl;
    }
    else {
        return false;
    }
    //构造一个确认数据包closeHead，标志位设置为ACK，计算并填充校验和
    //将确认数据包发送给客户端，表示服务端已经收到第一次挥手请求
    PacketHead closeHead;
    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = CheckPacketSum((u_short*)&closeHead, sizeof(PacketHead));
    memcpy(buffer, &closeHead, sizeof(PacketHead));
    sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
    cout << "[ACK_SEND]第二次挥手完成" << endl;
    //构造一个挥手请求数据包closeHead，标志位设置为FIN，计算并填充校验和发给客户端
    closeHead.flag = 0;
    closeHead.flag |= FIN;
    closeHead.checkSum = CheckPacketSum((u_short*)&closeHead, sizeof(PacketHead));
    memcpy(buffer, &closeHead, sizeof(PacketHead));
    sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
    cout << "[FIN_SEND]第三次挥手完成" << endl;
    //套接字设置为非阻塞模式，以便后续超时处理
    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);
    clock_t start = clock();
    //在非阻塞模式下，循环等待接收第四次挥手的确认数据包，如果超时则重新发送第三次挥手请求
    while (recvfrom(socket, buffer, sizeof(PacketHead), 0, (sockaddr*)&addr, &addrLen) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &closeHead, sizeof(PacketHead));
            sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR*)&addr, addrLen);
            start = clock();
        }
    }
    //检查标志位和校验和
    if ((((PacketHead*)buffer)->flag & ACK) && (CheckPacketSum((u_short*)buffer, sizeof(PacketHead) == 0))) {
        cout << "[ACK_RECV]第四次挥手完成，链接关闭" << endl;
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
        //加载失败
        cout << "[ERROR]加载DLL失败" << endl;
        return -1;
    }
    SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);
    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV);
    bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
    SOCKADDR_IN addrClient;
    //三次握手建立连接
    if (!acceptClient(sockSrv, addrClient)) {
        cout << "[ERROR]连接失败" << endl;
        return 0;
    }
    //char fileBuffer[MAX_FILE_SIZE];
    //可靠数据传输过程
    u_long fileLen = recvmessage(fileBuffer, sockSrv, addrClient);
    //四次挥手断开连接
    if (!disConnect(sockSrv, addrClient)) {
        cout << "[ERROR]断开失败" << endl;
        return 0;
    }
    //写入复制文件
    string filename = R"(save.jpg)";
    ofstream outfile(filename, ios::binary);
    if (!outfile.is_open()) {
        cout << "[ERROR]打开文件出错" << endl;
        return 0;
    }
    cout << fileLen << endl;
    outfile.write(fileBuffer, fileLen);
    outfile.close();

    cout << "文件复制完毕" << endl;
    system("PAUSE");
    return 1;
}
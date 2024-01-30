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
const u_short SOURCEPORT = 7777;//源端口是9999
const u_short DESPORT = 9999;   //服务端端口号是7777
const unsigned char SYN = 0x01;          //FC=0,OVER=0,FIN=0,ACK=0,SYN=1
const unsigned char ACK = 0x02;          //FC=0,OVER=0,FIN=0,ACK=1,SYN=0
const unsigned char SYN_ACK = 0x03;      //FC=0,OVER=0,FIN=0,ACK=1,SYN=1
const unsigned char FIN = 0x04;          //FC=0,OVER=0,FIN=1,ACK=0,SYN=0
const unsigned char FIN_ACK = 0x06;      //FC=0,OVER=0,FIN=1,ACK=1,SYN=0
const unsigned char OVER = 0x08;         //FC=0,OVER=1,FIN=0,ACK=0,SYN=0
const unsigned char OVER_ACK = 0xA;      //FC=0,OVER=1,FIN=0,ACK=1,SYN=0
const unsigned char FINAL_CHECK=0x10;    //FC=1,OVER=0,FIN=0,ACK=0,SYN=0
const double MAX_TIME = 0.2*CLOCKS_PER_SEC;
//数据头
struct Header {
    u_short checksum;     //校验和
    u_short seq;          //序列号,停等最低位只有0和1两种状态
    u_short ack;          //ack号，停等最低位只有0和1两种状态
    u_short flag;         //状态位
    u_short length;       //长度位
    u_int source_ip;      //源ip地址
    u_int des_ip;         //目的ip地址
    u_short source_port;  //源端口号
    u_short des_port;     //目的端口号
    Header() {//构造函数
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
//全局时钟设置
clock_t linkClock;
clock_t ALLSTART;
clock_t ALLEND;

u_short calcksum(u_short* mes, int size) {
    // 计算16位的字节数
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    // 将输入数据复制到缓冲区
    memcpy(buf, mes, size);
    // 初始化校验和变量
    u_long sum = 0;
    buf += 1;
    count -= 1;
    // 遍历缓冲区中的16位字，将它们相加
    while (count--) {
        sum += *buf++;
        // 如果和的高16位不为零，则将溢出部分加到低16位
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    // 返回校验和的反码
    return ~(sum & 0xffff);
}

u_short checksum(u_short* mes, int size) {
    // 计算16位的字节数
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    // 将输入数据复制到缓冲区
    memcpy(buf, mes, size);
    // 初始化校验和变量
    u_long sum = 0;
    // 遍历缓冲区中的16位字，将它们相加
    while (count--) {
        sum += *buf++;
        // 如果和的高16位不为零，则将溢出部分加到低16位
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    // 返回校验和的反码
    return ~(sum & 0xffff);
}

int Connect() {
    linkClock = clock();
    Header header;
    char* recvshbuffer = new char[sizeof(header)];
    char* sendshbuffer = new char[sizeof(header)];

    //第一次握手请求
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
        cout << "[Failed]:第一次握手请求发送失败..." << endl;
        return -1;
    }
    cout << "[1]:第一次握手消息发送成功...." << endl;
    //设置是否为非阻塞模式
    ioctlsocket(client, FIONBIO, &unlockmode);

    //设置计时器
    clock_t start = clock();

    //第一次握手重传
    while (recvfrom(client, recvshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:连接超时,服务器自动断开" << endl;
            return -1;
        }
        if (clock() - start > MAX_TIME) {
            if (sendto(client, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:第一次握手请求发送失败..." << endl;
                return -1;
            }
            start = clock();
            cout << "[1]:第一次握手消息反馈超时....正在重新发送" << endl;
            goto FIRSTSHAKE;
        }
    }

    //检验第二次握手信息是否准确
    memcpy(&header, recvshbuffer, sizeof(header));
    if (header.flag == SYN_ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[2]:第二次握手建立成功..." << endl;
    }
    else {
        cout << "[1]:不是期待的服务端数据包,即将重传第一次握手数据包...." << endl;
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:连接超时,服务器自动断开" << endl;
            return -1;
        }
        goto FIRSTSHAKE;
    }

    //发送第三次握手信息
    header.source_port = SOURCEPORT;
    header.des_port = DESPORT;
    header.flag = ACK;
    header.seq = 1;
    header.ack = 1;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendshbuffer, &header, sizeof(header));
THIRDSHAKE:
    if (sendto(client, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:第三次握手信息发送失败...." << endl;
        return -1;
    }
    start = clock();
    while (recvfrom(client, recvshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:连接超时,服务器自动断开" << endl;
            return -1;
        }
        if (clock() - start >= 5 * MAX_TIME) {
            cout << "[Failed]:第三次握手信息反馈超时...正在重新发送" << endl;
            if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
                cout << "[Failed]:连接超时,服务器自动断开" << endl;
                return -1;
            }
            goto THIRDSHAKE;
        }
    }
    memcpy(&header, recvshbuffer, sizeof(header));
    if (header.flag == ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[3]:正确发送第三次握手消息" << endl;
        cout << "[DONE]:与服务器连接成功，准备发送数据" << endl;
        return 1;
    }
    else {
        cout << "[ERROR]:确认消息接受失败...重发第三次握手" << endl;
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
        cout << "[Failed]:第一次挥手发送失败" << endl;
        return -1;
    }
WAITWAVE2:
    clock_t start = clock();
    while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > MAX_TIME) {
            if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:第一次挥手发送失败" << endl;
                return -1;
            }
            start = clock();
            cout << "[Failed]:第一次挥手消息反馈超时" << endl;
        }
    }
    cout << "[1]:第一次挥手消息发送成功" << endl;
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[2]:第二次挥手消息接收成功" << endl;
    }
    else {
        cout << "[Failed]:第二次挥手消息接受失败" << endl;
        goto WAITWAVE2;
    }
WAITWAVE3:
    while(recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0){}
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == FIN_ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[3]:第三次挥手消息接收成功" << endl;
    }
    else {
        cout << "[Failed]:第三次挥手消息接受失败" << endl;
        goto WAITWAVE3;
    }
    header.seq = 1;
    header.flag = ACK;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
SENDWAVE4:
    if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:第四次挥手发送失败" << endl;
        return -1;
    }
    start = clock();
    ioctlsocket(client, FIONBIO, &unlockmode);
    while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > 2 * MAX_TIME) {
            cout << "[Failed]:接受反馈超时，重发第四次挥手" << endl;
            goto SENDWAVE4;
        }
    }
    cout << "[4]:第四次挥手发送成功" << endl;
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == FINAL_CHECK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[System]:四次挥手完成，即将断开连接" << endl;
        return 1;
    }
    else {
        cout << "[Failed]:数据包错误,准备重发第四次握手" << endl;
        goto SENDWAVE4;
    }
}

int loadMessage() {
    string filename;
    cout << "[INPUT]:请输入要传输的文件名" << endl;
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

    cout << "[FINISH]:完成文件读入工作" << endl;
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
        cout << "[Failed]:传输结束信号发送失败" << endl;
        return -1;
    }
    cout << "[SEND]:传输结束信号发送成功" << endl;
    clock_t start = clock();
RECV:
    while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > MAX_TIME) {
            if (sendto(client, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:传输结束信号发送失败" << endl;
                return -1;
            }
            start = clock();
            cout << "[ERROR]:传输结束信号反馈超时" << endl;
            goto SEND;
        }
    }
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == OVER_ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        cout << "[FINFISH]:传输结束消息发送成功" << endl;
        return 1;
    }
    else {
        cout << "[ERROR]:数据包错误" << endl;
        goto RECV;
    }
}

int sendmessage() {
    ALLSTART = clock();
    //设置是否为非阻塞模式
    ioctlsocket(client, FIONBIO, &unlockmode);

    Header header;
    char* recvbuffer = new char[sizeof(header)];
    char* sendbuffer = new char[sizeof(header) + MAX_DATA_LENGTH];

    while (true) {

        int ml;//本次数据传输长度
        if (messagepointer > messagelength) {//不需要再发了，都发完了
            if (endsend() == 1) { return 1; }
            return -1;
        }
        if (messagelength - messagepointer >= MAX_DATA_LENGTH) {//可以按照最大限度发送
            ml = MAX_DATA_LENGTH;
        }
        else {
            ml = messagelength - messagepointer + 1;//需要计算发送的长度
        }

        header.seq = 0;//这次发的是零号序列号
        header.length = ml;//实际数据长度
        memset(sendbuffer, 0, sizeof(header) + MAX_DATA_LENGTH);//sendbuffer全部置零
        memcpy(sendbuffer, &header, sizeof(header));//拷贝header内容
        memcpy(sendbuffer+sizeof(header), message + messagepointer, ml);//拷贝数据内容
        messagepointer += ml;//更新数据指针
        header.checksum = calcksum((u_short*)sendbuffer, sizeof(header)+MAX_DATA_LENGTH);//计算校验和
        memcpy(sendbuffer, &header, sizeof(header));//填充校验和
    SEQ0SEND:
        //发送seq=0的数据包
        cout << "[send]:" << endl << "seq:0" << " length:" << ml<<" ";
        cout << "checksum:" << checksum((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;
        if (sendto(client, sendbuffer, (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
            cout << "[Failed]:seq0数据包发送失败" << endl;
            return -1;
        }
        clock_t start = clock();
    SEQ0RECV:
        //如果收到数据了就不发了，否则延时重传
        //设置了非阻塞，所以不会卡住
        while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
            if (clock() - start > MAX_TIME) {
                if (sendto(client, sendbuffer, (sizeof(header)+MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
                    cout << "[Failed]:seq0数据包发送失败" << endl;
                    return -1;
                }
                start = clock();
                cout << "[ERROR]:seq0数据包反馈超时" << endl;
            }
        }
        //检查ack位是否正确，如果正确则准备发下一个数据包
        memcpy(&header, recvbuffer, sizeof(header));
        cout << "[RECV]:" << endl << "ack:" << header.ack << "checksum:" << checksum((u_short*)&header, sizeof(header)) << endl;
        if (header.ack == 1 && checksum((u_short*)&header, sizeof(header) == 0)) {
            cout << "[CHECKED]:seq0接收成功" << endl;
        }
        else {
            cout << "[ERROR]:服务端未反馈正确的数据包" << endl;
            goto SEQ0SEND;
        }

        //准备开始发SEQ=1的数据包
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

        header.seq = 1;//零号序列号
        header.length = ml;//实际数据长度
        memset(sendbuffer, 0, sizeof(header) + MAX_DATA_LENGTH);//sendbuffer全部置零
        memcpy(sendbuffer, &header, sizeof(header));//拷贝header内容
        memcpy(sendbuffer + sizeof(header), message + messagepointer, ml);//拷贝数据内容
        messagepointer += ml;//更新数据指针
        header.checksum = calcksum((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH);//计算校验和
        memcpy(sendbuffer, &header, sizeof(header));//填充校验和
        //cout << checksum((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;
    SEQ1SEND:
        //发送seq=1的数据包
        cout << "[SEND]:" << endl << "seq:1" << " length:" << ml <<" ";
        cout << "checksum:" << checksum((u_short*)sendbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;
        if (sendto(client, sendbuffer, (sizeof(header) + MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
            cout << "[Failed]:seq1数据包发送失败" << endl;
            return -1;
        }
        start = clock();
    SEQ1RECV:
        //如果收到数据了就不发了，否则延时重传
        while (recvfrom(client, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
            if (clock() - start > MAX_TIME) {
                if (sendto(client, sendbuffer, (sizeof(header)+MAX_DATA_LENGTH), 0, (sockaddr*)&router_addr, rlen) == -1) {
                    cout << "[Failed]:seq1数据包发送失败" << endl;
                    return -1;
                }
                start = clock();
                cout << "[ERROR]:seq1数据包反馈超时" << endl;
            }
        }
        //检查ack位是否正确，如果正确则准备发下一个数据包
        memcpy(&header, recvbuffer, sizeof(header));
        cout << "[RECV]:" << endl << "ack:" << header.ack << " checksum:" << checksum((u_short*)&header, sizeof(header)) << endl;
        if (header.ack == 0 && checksum((u_short*)&header, sizeof(header)) == 0) {
            cout << "[CHECKED]seq1接收成功" << endl;
        }
        else {
            cout << "[ERROR]:服务端未反馈正确的数据包" << endl;
            goto SEQ1SEND;
        }
    }
}

void printlog() {
    cout << "--------------传输日志--------------" << endl;
    cout << "本次报文总长度为 " << messagepointer << "字节，共分为 " << (messagepointer / 256) + 1 << "个报文段分别转发" << endl;
    double t = (ALLEND - ALLSTART) / CLOCKS_PER_SEC;
    cout << "本次传输的总时长为" <<t <<"秒"<< endl;
    t = messagepointer / t;
    cout << "本次传输吞吐率为" << t <<"字节每秒"<< endl;
}

int main() {
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
    int flag = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (flag == 0)
	{
		cout << "[System]:";
		cout << "WSA加载成功" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "WSA加载失败" << endl;
	}
    //服务端
    server_addr.sin_family = AF_INET;//使用IPV4
    server_addr.sin_port = htons(9999);//server的端口号
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//主机127.0.0.1

    //路由器
    router_addr.sin_family = AF_INET;//使用IPV4
    router_addr.sin_port = htons(8888);//router的端口号
    router_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//主机127.0.0.1

    //客户端
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(7777);
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//主机127.0.0.1

    client = socket(AF_INET, SOCK_DGRAM, 0);
    flag = bind(client, (SOCKADDR*)&client_addr, sizeof(client_addr));
    if (flag == 0)//判断绑定
	{
		cout << "[System]:";
		cout << "绑定成功" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "绑定失败" << endl;
		closesocket(client);
		WSACleanup();
		return 0;
	}
    cout << "[PREPARE]初始化工作完成" << endl;
    flag = Connect();
        if (flag != 0)
	{
		cout << "[System]:";
		cout << "建立连接成功" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "建立连接失败" << endl;
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
		cout << "关闭连接成功" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "关闭连接失败" << endl;
		closesocket(client);
		WSACleanup();
		return 0;
	}
    printlog();
    cout<<"按任意键继续...";
    cin.clear();
    cin.sync();
    cin.get();
    return 0;
}
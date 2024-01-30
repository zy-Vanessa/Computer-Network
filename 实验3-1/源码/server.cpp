#include<iostream>
#include<WinSock2.h>
#include<time.h>
#include<fstream>
#include<iostream>
#include<windows.h>
using namespace std;
#pragma comment(lib,"ws2_32.lib")

SOCKET server;  // 服务器套接字
SOCKADDR_IN server_addr;  // 服务器地址结构
SOCKADDR_IN router_addr;  // 路由器地址结构
SOCKADDR_IN client_addr;  // 客户端地址结构
char* sbuffer = new char[1000];  // 发送缓冲区
char* rbuffer = new char[1000];  // 接收缓冲区
char* message = new char[100000000];  // 消息缓冲区
unsigned long long int messagepointer;  // 消息指针，指向消息缓冲区中的当前位置

int clen = sizeof(client_addr);
int rlen = sizeof(router_addr);

u_long blockmode = 0;  // 阻塞模式
u_long unblockmode = 1;  // 非阻塞模式

const unsigned int MAX_DATA_LENGTH = 1000;
const u_int SOURCEIP = 0x7f000001;
const u_int DESIP = 0x7f000001;
const u_short SOURCEPORT = 9999;//源端口是9999
const u_short DESPORT = 7777;   //客户端端口号是7777
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
    u_short checksum;    //校验和
    u_short seq;         //序列号，停等最低位只有0和1两种状态
    u_short ack;         //ack号，停等最低位只有0和1两种状态
    u_short flag;        //状态位
    u_short length;      //长度位
    u_int source_ip;     //源ip地址
    u_int des_ip;        //目的ip地址
    u_short source_port; //源端口号
    u_short des_port;    //目的端口号
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
//全局时钟设置
clock_t linkClock;

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
    Header header;//声明一个数据头
    char* recvshbuffer = new char[sizeof(header)];//创建一个和数据头一样大的接收缓冲区
    char* sendshbuffer = new char[sizeof(header)];//创建一个和数据头一样大的发送缓冲区
	cout << "[System]:";
    cout << "等待连接..." << endl;
    //等待第一次握手
    while (true) {
        // 将套接字设置为非阻塞模式
        ioctlsocket(server, FIONBIO, &unblockmode);
        // 接收第一次握手的申请
        while (recvfrom(server, recvshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen)<=0) {
            // 判断连接超时
            if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
	            cout  << "[Failed]:";
                cout << "连接超时,服务器自动断开" << endl;
                return -1;
            }
        }
        // 将接收到的数据复制到数据头结构体中
        memcpy(&header, recvshbuffer, sizeof(header));
        //如果是单纯的请求建立连接请求，并且校验和相加取反之后就是0
        if (header.flag == SYN && checksum((u_short*)(&header), sizeof(header)) == 0) {
	        cout << "[1]:";
            cout << "第一次握手建立成功。" << endl;
            break;
        }
        else {
	        cout << "[Failed]:";
            cout << "第一次握手失败，正在等待重传..." << endl;
        }
    }
SECONDSHAKE:
    //准备发送第二次握手的信息
    header.source_port = SOURCEPORT;
    header.des_port = DESPORT;
    header.flag = SYN_ACK;
    header.ack = (header.seq + 1) % 2;
    header.seq = 0;
    header.length = 0;
    header.checksum = calcksum((u_short*)(&header), sizeof(header));
    // 将数据头复制到发送缓冲区
    memcpy(sendshbuffer, &header, sizeof(header));
    // 发送第二次握手消息
    if (sendto(server, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:第二次握手消息发送失败...." << endl;
        return -1;
    }
    cout << "[2]:第二次握手消息发送成功...." << endl;
    // 判断连接超时
    clock_t start = clock();
    if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
        cout << "[Failed]:连接超时,服务器自动断开" << endl;
        return -1;
    }

    // 第二次握手消息的超时重传，重传时直接重传sendshbuffer里的内容
    while (recvfrom(server, recvshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:连接超时,服务器自动断开" << endl;
            return -1;
        }
        if (clock() - start > MAX_TIME) {
            // 重传第二次握手消息
            if (sendto(server, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:第二次握手消息重新发送失败..." << endl;
                return -1;
            }
            cout << "[2]:第二次握手消息重新发送成功..." << endl;
            start = clock();
        }
    }

    memcpy(&header, recvshbuffer, sizeof(header));
    if (header.flag == ACK && checksum((u_short*)(&header), sizeof(header)) == 0) {
        cout << "[3]:第三次握手建立成功！可以开始接收数据..." << endl;
        // 设置第三次握手消息的信息
        header.source_port = SOURCEPORT;
        header.des_port = DESPORT;
        header.flag = ACK;
        header.ack = (header.seq + 1) % 2;
        header.seq = 0;
        header.length = 0;
        header.checksum = calcksum((u_short*)(&header), sizeof(header));
        memcpy(sendshbuffer, &header, sizeof(header));
        sendto(server, sendshbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen);
        cout << "[DONE]:确认信息传输成功...." << endl;
    }
    else {
        cout << "[Failed]:不是期待的数据包，正在重传并等待客户端等待重传" << endl;
        if (clock() - linkClock > 75 * CLOCKS_PER_SEC) {
            cout << "[Failed]:连接超时,服务器自动断开" << endl;
            return -1;
        }
        goto SECONDSHAKE;
    }
    cout << "[WAITING]:正在等待接收数据...." << endl;
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
        cout << "[1]:第一次挥手消息接收成功" << endl;
    }
    else {
        cout << "[Failed]:第一次挥手消息接收失败" << endl;
        goto RECVWAVE1;
    }
SENDWAVE2:
    header.seq = 0;
    header.flag = ACK;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:第二次挥手消息发送失败" << endl;
        return -1;
    }
    cout << "[2]:第二次挥手消息发送成功" << endl;
    Sleep(80);
    //第三次挥手
    header.seq = 1;
    header.flag = FIN_ACK;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:第三次挥手消息发送失败" << endl;
        return -1;
    }
    cout << "[3]:第三次挥手消息发送成功" << endl;
    clock_t start = clock();
    ioctlsocket(server, FIONBIO, &unblockmode);
    while (recvfrom(server, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > MAX_TIME) {
            cout << "[Failed]:第四次挥手消息接收超时...准备重发二三次挥手" << endl;
            ioctlsocket(server, FIONBIO, &blockmode);
            goto SENDWAVE2;
        }
    }
    cout << "[4]:第四次挥手消息接收成功" << endl;
FINALCHECK:
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == ACK && checksum((u_short*)&header, sizeof(header)) == 0) {
        header.seq = 0;
        header.flag = FINAL_CHECK;
        header.checksum = calcksum((u_short*)&header, sizeof(header));
        memcpy(sendbuffer, &header, sizeof(header));
        sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen);
        cout << "[FINAL]:成功发送确认报文" << endl;
    }
    start = clock();
    while (recvfrom(server, recvbuffer, sizeof(header), 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > 10 * MAX_TIME) {
            cout << "[System]:四次挥手结束，即将断开连接" << endl;
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
    cout << "[FINISH]:确认消息发送成功" << endl;
    return 0;
}

int receivemessage() {
    Header header;
    char* recvbuffer = new char[sizeof(header) + MAX_DATA_LENGTH];
    char* sendbuffer = new char[sizeof(header)];

RECVSEQ0:
    //接受seq=0的数据
    while (true) {
        ioctlsocket(server, FIONBIO, &unblockmode);
        while (recvfrom(server, recvbuffer, sizeof(header) + MAX_DATA_LENGTH, 0, (sockaddr*)&router_addr, &rlen) <= 0) {}
        memcpy(&header, recvbuffer, sizeof(header));
        //传输结束
        if (header.flag == OVER) {
            //发送OVER_ACK
            if (checksum((u_short*)&header, sizeof(header)) == 0) { if (endreceive()) { return 1; }return 0; }
            else { cout << "[ERROR]:数据包出错，正在等待重传" << endl; goto RECVSEQ0; }
        }
        cout << "[RECV]:" << endl << "seq:" << header.seq << " checksum:" << checksum((u_short*)recvbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;
        if (header.seq == 0 && checksum((u_short*)recvbuffer, sizeof(header)+MAX_DATA_LENGTH) == 0) {
            cout << "[CHECKED]:成功接收seq0数据包" << endl;
            memcpy(message + messagepointer, recvbuffer + sizeof(header), header.length);
            messagepointer += header.length;
            break;
        }
        else {
            cout << "[ERROR]:数据包错误，正在等待对方重新发送" << endl;
        }
    }
    header.ack = 1;
    header.seq = 0;
    header.checksum = calcksum((u_short*)&header, sizeof(header));
    memcpy(sendbuffer, &header, sizeof(header));
SENDACK1:
    if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
        cout << "[Failed]:ack1发送失败" << endl;
        return -1;
    }
    clock_t start = clock();
RECVSEQ1:
    //设置为非阻塞模式
    ioctlsocket(server, FIONBIO, &unblockmode);
    while (recvfrom(server, recvbuffer, sizeof(header) + MAX_DATA_LENGTH, 0, (sockaddr*)&router_addr, &rlen) <= 0) {
        if (clock() - start > MAX_TIME) {
            if (sendto(server, sendbuffer, sizeof(header), 0, (sockaddr*)&router_addr, rlen) == -1) {
                cout << "[Failed]:ack1发送失败" << endl;
                return -1;
            }
            start = clock();
            cout << "[ERROR]:ack1消息反馈超时" << endl;
            goto SENDACK1;
        }
    }
    memcpy(&header, recvbuffer, sizeof(header));
    if (header.flag == OVER) {
        //传输结束
        if (checksum((u_short*)&header, sizeof(header) == 0)) { if (endreceive()) { return 1; }return 0; }
        else { cout << "[ERROR]:数据包出错，正在等待重传" << endl; goto RECVSEQ0; }
    }
    cout << "[RECV]:" << endl << "seq:" << header.seq << " checksum:" << checksum((u_short*)recvbuffer, sizeof(header) + MAX_DATA_LENGTH) << endl;

    if (header.seq == 1 && checksum((u_short*)recvbuffer, sizeof(header)+MAX_DATA_LENGTH)==0) {
        cout << "[CHECKED]:成功接受seq1数据包" << endl;
        memcpy(message + messagepointer, recvbuffer + sizeof(header), header.length);
        messagepointer += header.length;
    }
    else {
        cout << "[ERROR]:数据包损坏，正在等待重新传输" << endl;
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
        cout << "[ERROR]:消息为空，无法保存文件" << endl;
        return -1;
    }

    unsigned char fileFormat = message[0];
    string fileExtension = (fileFormat == '1') ? "png" : "txt";

    string filename = to_string(time(0)) + "." + fileExtension;
    ofstream fout(filename.c_str(), ofstream::binary);
    cout << "[STORE]:文件将被保存为 " << filename << endl;

    for (unsigned long long int i = 1; i < messagepointer; i++) {
        fout << message[i];
    }

    fout.close();

    cout << "[FINISH]:文件已成功下载到本地" << endl;
    return 0;
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

    //绑定服务端
    server = socket(AF_INET, SOCK_DGRAM, 0);
    flag = bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));
    if (flag == 0)//判断绑定
	{
		cout << "[System]:";
		cout << "绑定成功" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "绑定失败" << endl;
		closesocket(server);
		WSACleanup();
		return 0;
	}

    //计算地址性质
    int clen = sizeof(client_addr);
    int rlen = sizeof(router_addr);
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
		cout << "关闭连接成功" << endl;
	}
	else
	{
		cout << "[System]:";
		cout << "关闭连接失败" << endl;
		closesocket(server);
		WSACleanup();
		return 0;
	}
    cout<<"按任意键继续...";
    cin.clear();
    cin.sync();
    cin.get();
}

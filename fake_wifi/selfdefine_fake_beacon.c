#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netpacket/packet.h>
#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <net/if.h>

typedef unsigned char bool;
typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;
typedef signed long long int64;
typedef unsigned long long uint64;

// 用于生成beacon时的变量，此处只有bssid和essid两个字段可更改
struct ap
{
    uint8 bssid[6];
    uint16 seq_id;
    uint8 essid_len;
    char essid[32];
};

// 初始化ap，写入bssid和essid
void init_ap(struct ap *p_ap, uint8 *p_bssid, char *p_essid)
{
    memcpy(p_ap->bssid, p_bssid, 6);
    p_ap->seq_id = 0;
    uint32 t_len = strlen(p_essid);
    if (t_len > 32)
        t_len = 32;
    p_ap->essid_len = t_len;
    memcpy(p_ap->essid, p_essid, t_len);
}

// 组帧
uint16 create_beacon_frame(uint8 *p_buffer, struct ap *p_ap)
{
    // Frame Control	0×80 0×00
    // Duration	0×00 0×00
    // Destination Address	0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
    memcpy(p_buffer, "\x80\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF", 10);

    // Source Addres
    memcpy(p_buffer + 10, p_ap->bssid, 6);

    // BSSID
    memcpy(p_buffer + 16, p_ap->bssid, 6);

    // Seq-ID	低4位：0×0，高12位：帧序号
    p_buffer[22] = (uint8)(p_ap->seq_id & 0xFF);        // 低八位
    p_buffer[23] = (uint8)((p_ap->seq_id >> 8) & 0xFF); // 高八位
    p_ap->seq_id += 0x10;                               // 低四位置零，高四位加1

    struct timeval t_time;
    gettimeofday(&t_time, 0);
    uint64 t_timestamp = ((uint64)t_time.tv_sec) * 1000000 + t_time.tv_usec;
    uint8 t_i;

    // Timestamp	8字节，当前时间
    for (t_i = 0; t_i < 8; t_i++)
        p_buffer[24 + t_i] = (uint8)((t_timestamp >> (t_i << 3)) & 0xFF);

    // Beacon Interval	2字节，0x64 0×00
    // Capability info	0×01 0×00
    memcpy(p_buffer + 32, "\x64\x00\x01\x00", 4);
    p_buffer[36] = 0;

    // SSID 2字节长度 + SSID
    p_buffer[37] = p_ap->essid_len;
    memcpy(p_buffer + 38, p_ap->essid, p_ap->essid_len);

    // 返回帧字节长度
    return 38 + p_ap->essid_len;
}

// 次序依次为创建链路层套接字、找出wlan0的网卡编号、将原始套接字与wlan0绑定、将原始套接字设置为混杂模式
int32 create_raw_socket(char *p_iface)
{
    /* new raw socket */
    /*给入三个参数(int af, int type, int protocol)
    1) af 为地址族（Address Family），也就是 IP 地址类型，常用的有 AF_INET 和 AF_INET6。
            AF 是“Address Family”的简写
    2) type 为数据传输方式/套接字类型，常用的有 SOCK_STREAM（流格式套接字/面向连接的套接字） 
            和 SOCK_DGRAM（数据报套接字/无连接的套接字）
    3) protocol 为传输协议，常用的有 IPPROTO_TCP 和 IPPROTO_UDP。
    */
    int32 t_socket = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (t_socket < 0)
    {
        perror("<create_raw_socket> socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL)) failed!");
        return -1;
    }
    /* get the index of the interface */
    struct ifreq t_ifr;
    memset(&t_ifr, 0, sizeof(t_ifr));
    strncpy(t_ifr.ifr_name, p_iface, sizeof(t_ifr.ifr_name) - 1);
    if (ioctl(t_socket, SIOCGIFINDEX, &t_ifr) < 0)
    {
        perror("<create_raw_socket> ioctl(SIOCGIFINDEX) failed!");
        return -1;
    }
    /* bind the raw socket to the interface */
    struct sockaddr_ll t_sll;
    memset(&t_sll, 0, sizeof(t_sll));
    t_sll.sll_family = AF_PACKET;
    t_sll.sll_ifindex = t_ifr.ifr_ifindex;
    t_sll.sll_protocol = htons(ETH_P_ALL);
    if (bind(t_socket, (struct sockaddr *)&t_sll, sizeof(t_sll)) < 0)
    {
        perror("<create_raw_socket> bind(ETH_P_ALL) failed!");
        return -1;
    }
    /* open promisc */
    struct packet_mreq t_mr;
    memset(&t_mr, 0, sizeof(t_mr));
    t_mr.mr_ifindex = t_sll.sll_ifindex;
    t_mr.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(t_socket, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &t_mr, sizeof(t_mr)) < 0)
    {
        perror("<create_raw_socket> setsockopt(PACKET_MR_PROMISC) failed!");
        return -1;
    }
    return t_socket;
}

// 附上radiotap头，发送帧
int32 send_80211_frame(int32 p_socket, uint8 *p_buffer, uint32 p_size)
{
    uint8 t_buffer[4096];

    /*RadioTap 是由提供无线网卡的抓包驱动在捕获数据帧时 实时 添加的，
    并不是所有的抓包驱动都会添加 RadioTap 头。
    所以，需要特别注意的是：RadioTap 并不是 IEEE 802.11 数据帧格式规范的组成部分，
    实际传输的 802.11 帧并不包含所谓的 RadioTap 头部。
    */
    // 无线网卡会附上一个radiotap头，以展现与物理层有关的信息，比如功率、速率等
    uint8 *t_radiotap = (uint8 *)"\x00\x00\x0d\x00\x04\x80\x08\x00\x02\x00\x00\x00\x00";
    // reversion:8bits 始终为0
    // pad：8bits 始终为0
    // length:8bits 0x0d 整个radiotap的长度
    // present:4bytes 0x04 0x80 0x02 0x00 0x02 0x00 0x00 0x00 0x00
    //rate: 1byte 0x02 0x00
    //channel: 4bytes 0x00 0x00 0x00 0x00




    memcpy(t_buffer, t_radiotap, 13);
    memcpy(t_buffer + 13, p_buffer, p_size);
    p_size += 13;
    int32 t_size = write(p_socket, t_buffer, p_size);
    if (t_size < 0)
    {
        perror("<send_80211_frame> write() failed!");
        return -1;
    }
    return t_size;
}

/*
命令行输入参数：网口名称<iface>,  beacon名称<ssid>,
             beacon间隔<beacon_interval> ,  signal<signal>

sudo ./wifi_tx wlan0mon fake_beacon 300 101010
*/ 
int32 main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Usage: %s <iface> <essid> <beacon_interval> <signal>\n", argv[0]);
        printf("Example(in longyf's mac): sudo %s wlan0mon fake_beacon_longyf 100 101010\n", argv[0]);
        return 0;
    }
    else
    {
        printf("iface: %s\n essid: %s\n beacon_interval: %s\n signal: %s\n", argv[1], argv[2], argv[3], argv[4]);
        struct ap t_ap1;
        init_ap(&t_ap1, (uint8 *)"\xEC\x17\x2F\x2D\xB6\xB8", argv[2]);
        uint8 t_buffer[1024];
        int32 t_socket = create_raw_socket(argv[1]);
        int t_interval = atoi(argv[3])*1000;
        while (1)
        {
            uint16 t_len = create_beacon_frame(t_buffer, &t_ap1);
            printf("%d\n", send_80211_frame(t_socket, t_buffer, t_len));
            usleep(t_interval); // 100ms
        }
        return 0;
    }
}
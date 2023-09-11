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

struct ap
{
    uint8 bssid[6];
    uint16 seq_id;
    uint8 essid_len;
    char essid[32];
};

void init_ap(struct ap* p_ap,uint8* p_bssid,char* p_essid)
{
    memcpy(p_ap->bssid,p_bssid,6);
    p_ap->seq_id=0;
    uint32 t_len=strlen(p_essid);
    if(t_len>32)
        t_len=32;
    p_ap->essid_len=t_len;
    memcpy(p_ap->essid,p_essid,t_len);
}

uint16 create_beacon_frame(uint8* p_buffer,struct ap* p_ap)
{
    memcpy(p_buffer,"\x80\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF",10);
    memcpy(p_buffer+10,p_ap->bssid,6);
    memcpy(p_buffer+16,p_ap->bssid,6);
    p_buffer[22]=(uint8)(p_ap->seq_id&0xFF);
    p_buffer[23]=(uint8)((p_ap->seq_id>>8)&0xFF);
    p_ap->seq_id+=0x10;
    struct timeval t_time;
    gettimeofday(&t_time,0);
    uint64 t_timestamp=((uint64)t_time.tv_sec)*1000000+t_time.tv_usec;
    uint8 t_i;
    for(t_i=0;t_i<8;t_i++)
         p_buffer[24+t_i]=(uint8)((t_timestamp>>(t_i<<3))&0xFF);
    memcpy(p_buffer+32,"\x64\x00\x01\x00",4);
    p_buffer[36]=0;
    p_buffer[37]=p_ap->essid_len;
    memcpy(p_buffer+38,p_ap->essid,p_ap->essid_len);
    return 38+p_ap->essid_len;
}

int32 create_raw_socket(char* p_iface)
{
    /* new raw socket */
    int32 t_socket=socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
    if(t_socket<0)
    {
        perror("<create_raw_socket> socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL)) failed!");
        return -1;
    }
    /* get the index of the interface */
    struct ifreq t_ifr;
    memset(&t_ifr,0,sizeof(t_ifr));
    strncpy(t_ifr.ifr_name,p_iface,sizeof(t_ifr.ifr_name)-1);
    if(ioctl(t_socket,SIOCGIFINDEX,&t_ifr)<0)
    {
        perror("<create_raw_socket> ioctl(SIOCGIFINDEX) failed!");
        return -1;
    }
    /* bind the raw socket to the interface */
    struct sockaddr_ll t_sll;
    memset(&t_sll,0,sizeof(t_sll));
    t_sll.sll_family=AF_PACKET;
    t_sll.sll_ifindex=t_ifr.ifr_ifindex;
    t_sll.sll_protocol=htons(ETH_P_ALL);
    if(bind(t_socket,(struct sockaddr*)&t_sll,sizeof(t_sll))<0)
    {
        perror("<create_raw_socket> bind(ETH_P_ALL) failed!");
        return -1;
    }
    /* open promisc */
    struct packet_mreq t_mr;
    memset(&t_mr,0,sizeof(t_mr));
    t_mr.mr_ifindex=t_sll.sll_ifindex;
    t_mr.mr_type=PACKET_MR_PROMISC;
    if(setsockopt(t_socket,SOL_PACKET,PACKET_ADD_MEMBERSHIP,&t_mr,sizeof(t_mr))<0)
    {
        perror("<create_raw_socket> setsockopt(PACKET_MR_PROMISC) failed!");
        return -1;
    }
    return t_socket;
}

int32 send_80211_frame(int32 p_socket,uint8* p_buffer,uint32 p_size)
{
    uint8 t_buffer[4096];
    uint8* t_radiotap=(uint8*)"\x00\x00\x0d\x00\x04\x80\x02\x00\x02\x00\x00\x00\x00";
    memcpy(t_buffer,t_radiotap,13);
    memcpy(t_buffer+13,p_buffer,p_size);
    p_size+=13;
    int32 t_size=write(p_socket,t_buffer,p_size);
    if(t_size<0)
    {
        perror("<send_80211_frame> write() failed!");
        return -1;
    }
    return t_size;
}

int32 main()
{
    struct ap t_ap1,t_ap2;
    init_ap(&t_ap1,(uint8*)"\xEC\x17\x2F\x2D\xB6\xB8","finally!_hahaha");
    init_ap(&t_ap2,(uint8*)"\xEC\x17\x2F\x2D\xB6\xB9","fake_beacon_longyf");
    uint8 t_buffer[1024];
    int32 t_socket=create_raw_socket("wlx0013eff10297");
    while(1)
    {
        uint16 t_len=create_beacon_frame(t_buffer,&t_ap1);
        printf("%dn",send_80211_frame(t_socket,t_buffer,t_len));
        t_len=create_beacon_frame(t_buffer,&t_ap2);
        printf("%dn",send_80211_frame(t_socket,t_buffer,t_len));
        usleep(100000);
    }
    return 0;
}
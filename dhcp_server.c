#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <math.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <netinet/ether.h>
#include <linux/udp.h>



#define INT_NAME "eth1" // define ethernet name
#define SERVER_PORT 67
#define CLIENT_PORT 68

#define BOOTP_REQUEST 1
#define BOOTP_REPLY   2

#define DHCP_MSG_DICOVER 1
#define DHCP_MSG_OFFER   2
#define DHCP_MSG_REQUST  3
#define DHCP_MSG_ACK     5
#define DHCP_MSG_NACK    6
#define DHCP_MSG_RELEASE 7
#define DHCP_MSG_INFORM  8

#define DHCP_OPTION_NETMASK         1
#define DHCP_OPTION_ROUTER          3
#define DHCP_OPTION_DNS             6
#define DHCP_OPTION_IPADDRLEAS      51
#define DHCP_OPTION_MSGTYPE         53
#define DHCP_OPTION_SERVERID        54
#define DHCP_OPTION_PARAREQLIST     55
#define DHCP_OPTION_RENEWALTIME     58
#define DHCP_OPTION_REBINDINGTIME   59
#define DHCP_OPTION_CLASSID         60
#define DHCP_OPTION_END             255

#define USHORT unsigned short

typedef struct dhcp_header{
    unsigned char bp_op;
    unsigned char bp_htype; //hardware type
    unsigned char bp_hlen;
    unsigned char bp_hops;
    unsigned int  bp_xid; //transaction ID
    struct{
        unsigned bp_secs:16;
        unsigned bp_spare:7;
        unsigned bp_broadcast:1;
        unsigned bp_spare1:8;
    };
    unsigned int bp_clntipaddr;
    unsigned int bp_yipaddr;
    unsigned int bp_servipaddr;
    unsigned int bp_gipaddr;
    unsigned char bp_chaddr[16]; //client hardware address
    char bp_servname[64];
    unsigned char bp_file[128];
    unsigned char bp_magic_num[4] = {'D','H','C','P'};

} dhcp_header;

typedef struct dhcp_packet{
    dhcp_header header;
    unsigned char opt_data[1024-sizeof(dhcp_header)];
} dhcp_packet;


struct dhcp_opt_netmask{

};

struct dhcp_opt_router{

};

struct dhcp_opt_dns{

};

struct dhcp_opt_ipaddrleas{
    unsigned char dhcp_ipleastime = DHCP_OPTION_IPADDRLEAS;
};

struct dhcp_opt_msgtype{
    //unsigned char len = 1;
    unsigned char dhcp_msgtype = DHCP_OPTION_MSGTYPE;
};

struct dhcp_opt_serverid{
    unsigned char dhcp_serverid = DHCP_OPTION_SERVERID;
};

struct dhcp_opt_parareqlist{
    //unsigned char len = 13;
    unsigned char subnetmask = 28;
    unsigned char timeoffset = 2;
    unsigned char router = 3;
    unsigned char domainname = 15;
    unsigned char domainnameserver = 6;
    unsigned char domainsearch = 119;
    unsigned char hostname = 12;
    unsigned char netbiosnameserver = 44;
    unsigned char netbiosscope = 47;
    unsigned char interfacemtu = 26;
    unsigned char classless = 121;
    unsigned char networktime = 42;
};

struct dhcp_opt_renewaltime{
    unsigned char renewtime;
};

struct dhcp_opt_rebindingtime{

};

struct dhcp_opt_classid{ //set id 2014213129
    unsigned char classID[10] ;
};

struct dhcp_opt_end{
    unsigned char END = DHCP_OPTION_END;
};

struct ipAddrInfo {
    int isUsed;
    char MACAddr[6];
    pthread_t timerTid;
};

struct fakeUDPHeader {
    int srcAddr;
    int dstAddr;
    unsigned short udpLen;
    unsigned short padding;
    udphdr udpHeader;
};

int sock;
int startIPAddr;
int endIPAddr;
int router;
int dns;
int leaseTime;
int serverID;
int renewTime;
int rebindingTime;
unsigned char classID[10] = {'2','0','1','4','2','1','3','1','2','9'};
ipAddrInfo* ipMACMap;
int availableLength;
unsigned int subnet;
FILE *flease;

struct ifreq if_ens33;
struct sockaddr_in servAddr;
struct sockaddr_in clntAddr;
struct dhcp_packet recv_packet;
struct dhcp_packet send_packet;

USHORT CheckSum(USHORT *buffer, int size)
{
    unsigned long cksum=0;
    while (size > 1)
    {
        cksum += *buffer++;
        size -= sizeof(USHORT);
    }
    if (size)
    {
        cksum += *(unsigned char*)buffer;
    }
    /*对每个16bit进行二进制反码求和*/
    cksum = (cksum >> 16) + (cksum & 0xffff);
    cksum += (cksum >>16);
    return (USHORT)(~cksum);
}

//srcipaddr is wangluoxu
void sendFrame(unsigned char dstMacAddr[6], int srcIPAddr, int dstIPAddr, unsigned short payloadLength, unsigned char* payload) {
    static int isInit = 1;
    static int sockfd;
    static struct ifreq if_idx;
    static struct ifreq if_mac;

    char sendbuf[1500];
    int tx_len = 0;
    struct ether_header *eh = (struct ether_header *) sendbuf;
    struct sockaddr_ll socket_address;

    if (isInit == 1) {
        /* Open RAW socket to send on */
        if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
            perror("socket");
        }
        /* Get the index of the interface to send on */
        memset(&if_idx, 0, sizeof(struct ifreq));
        strncpy(if_idx.ifr_name, INT_NAME, IFNAMSIZ - 1);
        if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0)
            perror("SIOCGIFINDEX");
        /* Get the MAC address of the interface to send on */
        memset(&if_mac, 0, sizeof(struct ifreq));
        strncpy(if_mac.ifr_name, INT_NAME, IFNAMSIZ - 1);
        if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0)
            perror("SIOCGIFHWADDR");
        isInit = 0;
    }
    /* Construct the Ethernet header */
    memset(sendbuf, 0, 1500);
    /* Ethernet header */
    eh->ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
    eh->ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
    eh->ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
    eh->ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
    eh->ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
    eh->ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
    memcpy(eh->ether_dhost, dstMacAddr, 6);
    /* Ethertype field */
    eh->ether_type = htons(ETH_P_IP);
    tx_len += sizeof(struct ether_header);


    /* Packet data */
    struct iphdr ipHeader;
    *(unsigned char*)(&ipHeader) = 0x45;
    ipHeader.tot_len = htons(20 + payloadLength + sizeof(udphdr));
    ipHeader.id = htons((unsigned short)(rand() % 65536));
    ipHeader.frag_off = 0;
    ipHeader.ttl = 10;
    ipHeader.protocol = 17;
    ipHeader.check = 0;
    ipHeader.saddr = srcIPAddr;
    ipHeader.daddr = dstIPAddr;
    ipHeader.check = CheckSum((USHORT*)&ipHeader, 20);
    memcpy(sendbuf + tx_len, &ipHeader, 20);
    tx_len += 20;

    struct udphdr udpHeader;
    udpHeader.source = htons(SERVER_PORT);
    udpHeader.dest = htons(CLIENT_PORT);
    udpHeader.len = htons(sizeof(udphdr) + payloadLength);
    udpHeader.check = 0;
    fakeUDPHeader fakeHeader;
    fakeHeader.srcAddr = ipHeader.saddr;
    fakeHeader.dstAddr = ipHeader.daddr;
    fakeHeader.udpLen = udpHeader.len;
    fakeHeader.padding = 0x0011;
    fakeHeader.udpHeader = udpHeader;
   // udpHeader.check = CheckSum(((USHORT*)&fakeHeader), sizeof(fakeHeader));
    memcpy(sendbuf + tx_len, &udpHeader, sizeof(udpHeader));
    tx_len += sizeof(udpHeader);


    memcpy(sendbuf + tx_len, payload, payloadLength);
    tx_len += payloadLength;

    /* Index of the network device */
    socket_address.sll_ifindex = if_idx.ifr_ifindex;
    /* Address length*/
    socket_address.sll_halen = ETH_ALEN;
    /* Destination MAC */
    memcpy(socket_address.sll_addr, dstMacAddr, 6);

   //udpHeader.check = CheckSum(((USHORT*)&fakeHeader), sizeof(sendbuf));

    /* Send packet */
    if (sendto(sockfd, sendbuf, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
        printf("Send failed\n");






}

void deassignIPAddr(int id) {
    ipMACMap[id].isUsed = 0;
    in_addr a;
    a.s_addr = htonl(startIPAddr + id + 1);
    fprintf(flease, "Release %s\n", inet_ntoa(a));
    fprintf(stdout, "Release %s\n", inet_ntoa(a));
}

//ipAddr is wangluoxu
void deassignIPAddr(int ipAddr, char) {
    int id = ntohl(ipAddr) - startIPAddr - 1;
    pthread_cancel(ipMACMap[id].timerTid);
    deassignIPAddr(id);
}

//seconds
void* timer(void* id) {
    printf("%d start timer\n", *(int*)id);
    usleep(ntohl(leaseTime) * 1000000);
    printf("%d stop timer\n", *(int*)id);
    deassignIPAddr(*(int*)id);
    free((int*)id);

}

void renewLease(int id) {
    if (id >= 0 && id <= availableLength) {
        pthread_cancel(ipMACMap[id].timerTid);
        int *copiedId = (int*)malloc(sizeof(int));
        *copiedId = id;
        pthread_create(&(ipMACMap[id].timerTid),0,timer,copiedId);
    }
}

//return wangluoxu
int getAvailableIPAddr() {
    static int init = 1;
    availableLength = endIPAddr - startIPAddr - 1;
    if (init) {
        ipMACMap = (ipAddrInfo*)malloc(sizeof(ipAddrInfo) * availableLength);
        for (int i = 0; i < availableLength; i++) {
            ipMACMap[i].isUsed = 0;
        }

        if (ntohl(serverID) > startIPAddr && ntohl(serverID) < endIPAddr)
            ipMACMap[ntohl(serverID) - startIPAddr - 1].isUsed = 1;
        init = 0;
    }
    for (int i = 0; i < availableLength; i++)
        if (ipMACMap[i].isUsed == 0)
            return htonl(startIPAddr + i + 1);
}

//ipAddr is wangluoxu
int confirmAndAssignIPAddr(int ipAddr, const unsigned char* MACAddr) {
    ipAddr = ntohl(ipAddr);
    int id = ipAddr - startIPAddr - 1;
    if (id < 0 || id >= availableLength)
        return 0;
    if (ipMACMap[id].isUsed == 0) {
        memcpy(ipMACMap[id].MACAddr, MACAddr, 6);
        ipMACMap[id].isUsed = 1;
        int *copiedId = (int*)malloc(sizeof(int));
        *copiedId = id;
        pthread_create(&(ipMACMap[id].timerTid),0,timer,copiedId);
        ipAddr = htonl(ipAddr);
        fprintf(flease, "Assign %s to %2X", inet_ntoa(*(in_addr*)&ipAddr), MACAddr[0]);
        for (int i = 0; i < 5; i++)
            fprintf(flease, ":%2X", MACAddr[i + 1]);
        fprintf(flease, "\n");
        fprintf(stdout, "Assign %s to %2X", inet_ntoa(*(in_addr*)&ipAddr), MACAddr[0]);
        for (int i = 0; i < 5; i++)
            fprintf(stdout, ":%2X", MACAddr[i + 1]);
        fprintf(stdout, "\n");
        return 1;
    }
    else {
        printf("id=%d\n",id);
        if (memcmp(MACAddr, ipMACMap[id].MACAddr, 6) == 0) {
            ipMACMap[id].isUsed = 1;
            renewLease(id);
            ipAddr = htonl(ipAddr);
            fprintf(flease, "RenewLease %s to %2X", inet_ntoa(*(in_addr*)&ipAddr), MACAddr[0]);
            for (int i = 0; i < 5; i++)
                fprintf(flease, ":%2X", MACAddr[i + 1]);
            fprintf(flease, "\n");
            fprintf(stdout, "RenewLease %s to %2X", inet_ntoa(*(in_addr*)&ipAddr), MACAddr[0]);
            for (int i = 0; i < 5; i++)
                fprintf(stdout, ":%2X", MACAddr[i + 1]);
            fprintf(stdout, "\n");
            return 1;
        }
        else return 0;
    }
}

void readConfig(const char* filename) {
    FILE *f = fopen(filename, "r");
    char tempIP[16];

    fgets(tempIP, 15, f);
    int length;
    fscanf(f,"%d",&length);

    subnet = 0xffffffff;
    subnet >>= (32 - length);
    subnet <<= (32 - length);
    subnet = htonl(subnet);

    startIPAddr = ntohl(inet_addr(tempIP));
//    printf("%x", (int)(startIPAddr & 0x000000ff));
    endIPAddr = startIPAddr + (int)(pow(2, 32 - length));

    fgets(tempIP, 2, f);
    fgets(tempIP, 15, f);
    router = inet_addr(tempIP);
    printf("subnet=%x\n", tempIP);

    fgets(tempIP, 15, f);
    dns = inet_addr(tempIP);

    fscanf(f,"%d",&leaseTime);
    leaseTime = htonl(leaseTime);

    fgets(tempIP, 2, f);
    fgets(tempIP, 15, f);
    serverID = inet_addr(tempIP);

    fscanf(f,"%d",&renewTime);
    fscanf(f,"%d",&rebindingTime);

   // fscanf(f, "%d", classID);
}

//ptr always points to the last blank position and option type and option length is not count in len
void writeOption(unsigned char* buf, int* ptr, unsigned char* payload, unsigned char len, unsigned char type) {
    if(type != DHCP_OPTION_END){
        buf[(*ptr)++] = type;
        buf[(*ptr)++] = len;
        memcpy(buf + *ptr, payload, len);
        *ptr += len;
    }
    else {
        buf[(*ptr)++] = type;
        memcpy(buf + *ptr, payload, len);
        *ptr += len;
    }
}

//ptr always points to the position to be read
unsigned char getOption(unsigned char* srcbuf, int* ptr, unsigned char* dstbuf, unsigned char* type) {
    *type = srcbuf[(*ptr)++];
    if (*type != DHCP_OPTION_END) {
        unsigned char len = srcbuf[(*ptr)++];
        memcpy(dstbuf, srcbuf + *ptr, len);
        *ptr += len;
//        printf("type=%d,len=%d,payload=%x,ptr=%x\n", *type, len, *(int*)dstbuf, *ptr - len - 2);
        return len;
    }
    else {
        memcpy(dstbuf, srcbuf + *ptr, 1);
        *ptr += 1;
        return 1;
    }
}

//ipAddr is wangluoxu
int discover_processer(int sock,struct dhcp_packet recv_packet, struct sockaddr_in servAddr, int addr_len, unsigned char c, int* ipAddr ){
    unsigned char type;
    unsigned char recv_buf[1024-240];
    unsigned char send_buf[1024];
    int readptr = 0;
    unsigned char len = getOption(recv_packet.opt_data, &readptr, recv_buf, &type);
    struct dhcp_opt_msgtype opt_msgtype;
    struct dhcp_opt_serverid opt_serverID;
    struct dhcp_opt_ipaddrleas opt_ipaddrleas;
    //classID = htonl(classID);

    memset(&send_packet, 0, sizeof(send_packet));
    send_packet.header.bp_op = BOOTP_REPLY;
    send_packet.header.bp_htype = 1;
    send_packet.header.bp_hlen = 6;
    send_packet.header.bp_hops = 0;
    send_packet.header.bp_xid = recv_packet.header.bp_xid;
    send_packet.header.bp_secs = 0;
    send_packet.header.bp_clntipaddr = inet_addr("0.0.0.0");

    int ip;
    send_packet.header.bp_yipaddr = (c == DHCP_MSG_ACK ? *ipAddr : (ip = getAvailableIPAddr()));
    if (c != DHCP_MSG_ACK)
        *ipAddr = ip;
    send_packet.header.bp_servipaddr = 0;//htonl(inet_addr(if_ens33.ifr_addr.sa_data));
    send_packet.header.bp_gipaddr = htonl(inet_addr("0.0.0.0"));
    int magicNum = htonl(0x63825363);
    memcpy(send_packet.header.bp_magic_num, &magicNum, sizeof(int));
    memcpy(send_packet.header.bp_chaddr, recv_packet.header.bp_chaddr, 16);
    //send_packet.header.bp_chaddr = recv_packet.header.bp_chaddr;
    //writeOption(send_packet.opt_data, 0, , 3, );
    int ptr = 0;
    writeOption(send_packet.opt_data, &ptr, (unsigned char *)(&c),1 , opt_msgtype.dhcp_msgtype );
    writeOption(send_packet.opt_data, &ptr, (unsigned char *) &(serverID), 4, opt_serverID.dhcp_serverid );
    writeOption(send_packet.opt_data, &ptr, (unsigned char *) &(leaseTime), 4, opt_ipaddrleas.dhcp_ipleastime);
    writeOption(send_packet.opt_data, &ptr, (unsigned char *) &(subnet), 4, DHCP_OPTION_NETMASK);
    writeOption(send_packet.opt_data, &ptr, (unsigned char *) &(router), 4, DHCP_OPTION_ROUTER);
    writeOption(send_packet.opt_data, &ptr, (unsigned char *) &(dns), 4, DHCP_OPTION_DNS);
    writeOption(send_packet.opt_data, &ptr, classID, 10, DHCP_OPTION_CLASSID);
    writeOption(send_packet.opt_data, &ptr, 0, 0, DHCP_OPTION_END);
    return ptr;
}

int main(int argc, char* argv[]) {
   //unsigned char classID[10] = {'2','0','1','4','2','1','3'};
    int id = 2014213129;
    printf("%d", id);
    srand(time(0));
    flease = fopen("dhcp.lease", "a");
    int i = 1;
    printf("%d\n",sizeof(dhcp_header));
    strcpy(if_ens33.ifr_name,INT_NAME);
    socklen_t len = sizeof(i);

    if((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
        printf("socket() failed.\n");
    }

    setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&i,len);//allow socket to broadcast
    if(setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (char*)&if_ens33, sizeof(if_ens33)) < 0)//set socket to interface INT_NAME
    {
        printf("bind socket to %s error\n", INT_NAME);
    }

    memset(&servAddr, 0, sizeof(servAddr)); //zero out structure
    servAddr.sin_family = AF_INET; //Internet addr family ipv4
    servAddr.sin_port = htons(SERVER_PORT); //server port
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); //server IP address 0.0.0.0
    if(bind(sock,(struct sockaddr *)&servAddr,sizeof(servAddr))< 0 ){
        printf("bind() failed.\n");
    }

    clntAddr.sin_addr.s_addr = inet_addr("255.255.255.255"); //client IP address 255.255.255.255
    clntAddr.sin_port = htons(CLIENT_PORT);
    clntAddr.sin_family = AF_INET;
    //unsigned char buf[1024];

    readConfig("../dhcp.config");
    while (1) {
        int addr_len = sizeof(servAddr);
        int length = recvfrom(sock, &recv_packet, sizeof(dhcp_packet), 0, (struct sockaddr *)&servAddr, (socklen_t *) &addr_len);
        printf("recv() returns\n");
//        for (int i = 0; i < 1024; i++)
//          printf("%x",recv_packet[i]);
//        printf("\n");
        if(recv_packet.opt_data[2] == DHCP_MSG_DICOVER){
            int ipaddr;
            int size = discover_processer(sock, recv_packet, servAddr, addr_len, DHCP_MSG_OFFER, &ipaddr);
            sendto(sock, &send_packet, sizeof(dhcp_header) + size, 0, (struct sockaddr *)&clntAddr, addr_len);
//            sendFrame(recv_packet.header.bp_chaddr, serverID, ipaddr, sizeof(dhcp_header) + size, (unsigned char*)&send_packet);
            //sendto(sock, "hello", 5, 0, (struct sockaddr *) &clntAddr, addr_len);

        }

        else if(recv_packet.opt_data[2] == DHCP_MSG_REQUST){
            //confirmAndAssignIPAddr()
            int readPtr = 0;
            unsigned char readBuf[100];
            int reqIP = 0;
            printf("xid=%x\n",ntohl(recv_packet.header.bp_xid));
            while (1) {
                memset(readBuf, 0, 100);
                unsigned char type;
                getOption(recv_packet.opt_data, &readPtr, readBuf, &type);
                if (type == DHCP_OPTION_END)
                    break;
                else if (type == 0x32) {
                    reqIP = *(int *) readBuf;
//                    in_addr a;
//                    a.s_addr = reqIP;
//                    printf("%s", inet_ntoa(a));
                    break;
                }
            }
            if (reqIP == 0)
                reqIP = recv_packet.header.bp_clntipaddr;
            int result = confirmAndAssignIPAddr(reqIP, recv_packet.header.bp_chaddr);
            int size = discover_processer(sock, recv_packet, servAddr, addr_len, result == 1 ? DHCP_MSG_ACK : DHCP_MSG_NACK, &reqIP);
//            send_packet.header.bp_op = DHCP_MSG_ACK;
            sendto(sock, &send_packet, sizeof(dhcp_header) + size, 0, (struct sockaddr *)&clntAddr, addr_len);
//            sendFrame(recv_packet.header.bp_chaddr, serverID, reqIP, sizeof(dhcp_header) + size, (unsigned char*)&send_packet);
//            FILE *flease = fopen("dhcp.lease", "a");
//            fprintf(send_packet.header.bp_chaddr, sizeof(send_packet.header.bp_chaddr[16]),1 ,flease);
//            fwrite(&send_packet.header.bp_yipaddr,sizeof(send_packet.header.bp_yipaddr),1 , flease);
//            fwrite(&leaseTime, sizeof(leaseTime),1 , flease);
//            fclose(flease);
        }

        else if(recv_packet.opt_data[2] == DHCP_MSG_INFORM){
            int ipaddr = recv_packet.header.bp_clntipaddr;
            discover_processer(sock, recv_packet, servAddr, addr_len, DHCP_MSG_ACK, &ipaddr);
//            send_packet.header.bp_op = DHCP_MSG_ACK;
            sendto(sock, &send_packet, sizeof(send_packet), 0, (struct sockaddr *)&clntAddr, addr_len);
        }
        else if (recv_packet.opt_data[2] == DHCP_MSG_RELEASE) {
            //
            int ipAddr = recv_packet.header.bp_clntipaddr;
            deassignIPAddr(ipAddr, '0');
        }
        else
            sendto(sock, "error", 5, 0, (struct sockaddr *) &clntAddr, addr_len);
        fflush(flease);
    }

}
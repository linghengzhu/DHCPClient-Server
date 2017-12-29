#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <error.h>
#include <net/route.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#define DHCP_MTU_MAX	1024
#define DHCP_FIXED_NON_OPTION	236
#define DHCP_OPTION_LEN	DHCP_MTU_MAX - DHCP_FIXED_NON_OPTION

typedef int bool;
typedef unsigned char ne_u8;
typedef unsigned short ne_u16;
typedef unsigned long ne_u32;
#define DHCP_MESSAGE_TYPE	0x53
#define DHCP_END	0xff
#define DHCP_PADDING	0x00
#define OPT_LEN	1

struct dhcp_packet {
	ne_u8 op;
	ne_u8 htype;
	ne_u8 hlen;
	ne_u8 hops;
	ne_u32 xid;
	ne_u16 secs;
	ne_u16 flags;
	ne_u8 ciaddr[4];
	ne_u8 yiaddr[4];
	ne_u8 siaddr[4];
	ne_u8 giaddr[4];
	ne_u8 chaddr[16];
	ne_u8 sername[64];
	ne_u8 file[128];
	ne_u8 option[DHCP_OPTION_LEN];
};

struct dhcp_options {
	ne_u8 len[256];
  ne_u8 val[256][10];
};

void send_udp_discover(int sockfd, struct sockaddr_in *addr);
void send_udp_request(int sockfd, struct sockaddr_in *addr, char *servaddr, char *ipaddr);
void send_udp_release(int sockfd, struct sockaddr_in *addr, char *servaddr, char *ipaddr);
void send_udp_inform(int sockfd, struct sockaddr_in *addr, char *ipaddr);
void send_udp_renew(int sockfd, struct sockaddr_in *addr, char *ipaddr);
void writeLine(char *line);

int setIfAddr(char *ifname, char *Ipaddr, char *mask,char *gateway);
void readOption(struct dhcp_options *options, char *buf);
int timeval_subtract(struct   timeval*   result,   struct   timeval*   x,   struct   timeval*   y);   

#define DHCP_PACKET_LENGTH	sizeof(struct dhcp_packet)

int main(int argc, char *argv[]) {
	int sock;
	struct sockaddr_in udpServAddr;
	int sin_size;
	struct sockaddr_in udpCliAddr;
	ne_u8 gateway[4];
	ne_u8 mask[4];
	ne_u8 ipaddr[4];
	ne_u8 servaddr[4];
	struct in_addr gateways;
	struct in_addr masks;
	struct in_addr ipaddrs;
	struct in_addr servaddrs;
	struct dhcp_packet *dhcp;
	struct dhcp_options optionOffer;
	struct dhcp_options optionAck;
	struct dhcp_options optionInform;
	struct dhcp_options optionRenew;
	int T1,T2,lease;
	int judge = 0;
	int markloop2 = 0;
	
	struct timeval start,stop,diff;   
  gettimeofday(&start,0); 
	
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
		printf("socket() failed.\n");
	int i=1;
	socklen_t len = sizeof(i);
	setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&i,len);
	
	struct ifreq if_eth1;
	strcpy(if_eth1.ifr_name,"eth1");
	if(setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (char*)&if_eth1, sizeof(if_eth1))<0) {
		printf("bind socket to eth1 error\n");
	}
	
	bzero(&udpServAddr,sizeof(udpServAddr));
	bzero(&udpCliAddr,sizeof(udpCliAddr));
	udpServAddr.sin_family = AF_INET;
	udpServAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
	udpServAddr.sin_port = htons(67);
	udpCliAddr.sin_family = AF_INET;
	udpCliAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	udpCliAddr.sin_port = htons(68);

	if (bind(sock, (struct sockaddr *)&udpCliAddr, sizeof(struct sockaddr)) == -1) {
		printf("bind error\n");
		exit(1);
	}
	
	struct timeval tv_out;
  tv_out.tv_sec = 2;//等待5秒
  tv_out.tv_usec = 0;
	setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&tv_out, sizeof(tv_out));
	
	char buffer[1024];
	struct sockaddr_in tmpaddr;
	int tempa;
	int markloop = 0;
	
	//读文件
  FILE *fp;   
  if((fp = fopen("client.conf","r")) == NULL)
  {   
      fp = fopen("client.conf","wb");
  }
  else {
  fread(&judge, sizeof(int), 1, fp);   
  if(judge!=0) {
	  fread(&lease, sizeof(int), 1, fp);
	  fread(servaddr, sizeof(ne_u8), 4, fp);
	  fread(ipaddr, sizeof(ne_u8), 4, fp);
  }
	}
  fclose(fp);      
	
	while(1) {
	if(strcmp(argv[1],"--default")==0 || strcmp(argv[1],"--interact")==0) {
	udpServAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
	send_udp_discover(sock, &udpServAddr);
	
  sin_size=sizeof(struct sockaddr_in);  
  
	while(1) {
	memset(buffer, 0, 1024);
	int tmplen = recvfrom(sock, buffer, 1024, 0, (struct sockaddr *)&tmpaddr, &sin_size);
	dhcp = (struct dhcp_packet *)buffer;
	ne_u8 *tmp = NULL;
	tmp = buffer + DHCP_FIXED_NON_OPTION + 6;
	if(tmplen<0) {
		markloop = 1;
		break;
	}
	else
		markloop = 0;
	if(*tmp !=2)
		continue;
	break;
	}
	if(markloop) {
		sleep(10);
		continue;
	}
	memcpy(ipaddr, dhcp->yiaddr, 4);
	memcpy(&tempa, ipaddr, 4);
  ipaddrs.s_addr = tempa;
	readOption(&optionOffer, buffer);
	memcpy(mask, optionOffer.val[1], 4);
	memcpy(&tempa, mask, 4);
  masks.s_addr = tempa;
	memcpy(gateway, optionOffer.val[3], 4);
	memcpy(&tempa, gateway, 4);
  gateways.s_addr = tempa;
	memcpy(servaddr, optionOffer.val[54], 4);
	memcpy(&tempa, servaddr, 4);
  servaddrs.s_addr = tempa;
	memcpy(&T1, optionOffer.val[58], 4);
	memcpy(&T2, optionOffer.val[59], 4);
	memcpy(&lease, optionOffer.val[51], 4);
	lease = ntohl(lease);
	
	send_udp_request(sock, &udpServAddr, servaddr, ipaddr);
	
	while(1) {
	memset(buffer, 0, 1024);
	recvfrom(sock, buffer, 1024, 0, (struct sockaddr *)&tmpaddr, &sin_size);
	dhcp = (struct dhcp_packet *)buffer;
	readOption(&optionAck, buffer);
	if(*(optionAck.val[53]) != 5 && *(optionAck.val[53]) != 6)
		continue;
	int i;
	for(i=0;i<4;i++) {		
		if(*((optionAck.val[54])+i) != *(servaddr+i))
			break;
	}
	if(i!=4)
		continue;
	break;
	}
	
	if(*(optionAck.val[53]) == 5) {
		printf("ACK\n");
		writeLine("discover ACK");
		setIfAddr("eth1", inet_ntoa(ipaddrs), inet_ntoa(masks), inet_ntoa(gateways));
    if ((fp=fopen("client.conf","wb"))==NULL)         //打开指定文件，如果文件不存在则新建该文件  
    {  
        printf("Open Failed.\n");  
        return;  
    }   
    judge = 1;
    fwrite(&judge, sizeof(int), 1, fp);
    fwrite(&lease, sizeof(int), 1, fp);
	  fwrite(servaddr, sizeof(ne_u8), 4, fp);
	  fwrite(ipaddr, sizeof(ne_u8), 4, fp);
    fclose(fp);                            
    
	}
	else {
		printf("NACK\n");
		writeLine("discover NAK");
	}
	if(strcmp(argv[1],"--default")==0)
		break;
	else {
		gettimeofday(&stop,0);   
		timeval_subtract(&diff,&start,&stop); 
		while(lease*0.5>(diff.tv_sec)) {
			gettimeofday(&stop,0);   
			timeval_subtract(&diff,&start,&stop); 
		}
		printf("T1 expires\n");
		writeLine("T1 expires");
		strcpy(argv[1], "--renew");
		markloop2 = 1;
		continue;
	}
	}
	else if(strcmp(argv[1],"--renew")==0) {
		if(argc == 3) {
			int tempad;
			tempad = inet_addr(argv[2]);
			memcpy(ipaddr,&tempad,4);
		}
		if(!markloop2)
			gettimeofday(&start,0);
		memcpy(&tempa, servaddr, 4);
  	servaddrs.s_addr = tempa;
		udpServAddr.sin_addr.s_addr = servaddrs.s_addr;
		send_udp_renew(sock, &udpServAddr, ipaddr);
		udpServAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
		int recvlen;
		while(1) {
		memset(buffer, 0, 1024);
		recvlen = recvfrom(sock, buffer, 1024, 0, (struct sockaddr *)&udpServAddr, &sin_size);
		gettimeofday(&stop,0);   
		timeval_subtract(&diff,&start,&stop); 
		if(recvlen>0) {
			dhcp = (struct dhcp_packet *)buffer;
			readOption(&optionAck, buffer);
			if(*(optionAck.val[53]) != 5 && *(optionAck.val[53]) != 6)
				continue;
			int i;
			for(i=0;i<4;i++) {		
				if(*((optionAck.val[54])+i) != *(servaddr+i))
					break;
			}
			if(i!=4)
				continue;
			break;
		}
		if(markloop2) {
    if((int)(lease*0.875)<(diff.tv_sec)) {
    	printf("T2 expires\n");
    	writeLine("T2 expires");
			break;
    }
  	}
    else {
    if((int)(lease*0.375)<(diff.tv_sec)) {
    	printf("T2 expires\n");
    	writeLine("T2 expires");
			break;
    }
  	}
		}
		if(recvlen<0) {
			send_udp_request(sock, &udpServAddr, servaddr, ipaddr);
			memset(buffer, 0, 1024);
			int tmplen1;
			int mark=0;
			while(1) {
			tmplen1 = recvfrom(sock, buffer, 1024, 0, (struct sockaddr *)&tmpaddr, &sin_size);
			gettimeofday(&stop,0);   
  		timeval_subtract(&diff,&start,&stop);  
  		if(tmplen1>0) {
  			break;
  			mark = 1;
  		}
			if(markloop2) {
	      if(lease<(diff.tv_sec)) {
					strcpy(argv[1], "--default");
					printf("Lease expires\n");
					writeLine("Lease expires");
					break;
	      }
	    }
      else {
	      if(lease*0.5<(diff.tv_sec)) {
					strcpy(argv[1], "--default");
					printf("Lease expires\n");
					writeLine("Lease expires");
					break;
	      }
	    }
			}
			if(mark) {
				break;
			}
		}
		else
		if(*(optionAck.val[53]) == 6) {
			writeLine("renew NAK");
			strcpy(argv[1], "--default");
		}
		else {
			writeLine("renew ACK");
			if(markloop2) {
				gettimeofday(&start,0);
				while(1) {
				gettimeofday(&stop,0);   
				timeval_subtract(&diff,&start,&stop); 
				if(lease*0.5<(diff.tv_sec)) {
					printf("T1 expires\n");
					writeLine("T1 expires");
					break;
				}
				}
				continue;
			}
			else {
				break;
			}
		}
	}
	else if(strcmp(argv[1],"--inform")==0) {
		
		memcpy(&tempa, servaddr, 4);
  	servaddrs.s_addr = tempa;
		udpServAddr.sin_addr.s_addr = servaddrs.s_addr;
  	memcpy(&tempa, ipaddr, 4);
		
		send_udp_inform(sock, &udpServAddr, ipaddr);
		memset(buffer, 0, 1024);
		recvfrom(sock, buffer, 1024, 0, (struct sockaddr *)&udpServAddr, &sin_size);
		dhcp = (struct dhcp_packet *)buffer;
		readOption(&optionInform, buffer);
		writeLine("inform ACK");
		break;
	}
	else if(strcmp(argv[1],"--release")==0) {
		
		memcpy(&tempa, servaddr, 4);
  	servaddrs.s_addr = tempa;
		udpServAddr.sin_addr.s_addr = servaddrs.s_addr;
		
		send_udp_release(sock, &udpServAddr, servaddr, ipaddr);
		writeLine("release");
		sleep(3);
		setIfAddr("eth1", "0.0.0.0", "0.0.0.0", "0.0.0.0");
		
		FILE *fp1;                                  
    if ((fp1=fopen("client.conf","wb"))==NULL)       
    {  
        printf("Open Failed.\n");  
        return;  
    }   
    judge = 0;
    fread(&judge, sizeof(int), 1, fp);
    fclose(fp1);                             
		
		break;
	}
	else {
		printf("wrong use!\n");
		exit(0);
	}
//	sleep(5);
	}
}

void send_udp_discover(int sockfd, struct sockaddr_in *addr)
{
	char buffer[1024];
	memset(buffer, 0, 1024);
	struct dhcp_packet *dhcp;
	ne_u8 cli_id[11] = {50,48,49,52,50,49,51,49,51,51};
	ne_u8 hostname[32] = {0};
	ne_u8 *op = NULL;
	time_t t;
	srand((unsigned) time(&t));
	int i;
	
	ne_u8 s_mac[6] = {0x08,0x00,0x27,0xd3,0xfd,0xae};
	
	gethostname(hostname, sizeof(hostname));

	dhcp = (struct dhcp_packet *)buffer;
	dhcp->op = 1;
	dhcp->htype = 1;
	dhcp->hlen = 6;
	dhcp->hops = 0;
	int aa = rand();
	dhcp->xid = ntohl(aa);
	dhcp->secs = 0;
	dhcp->flags = htons(0x8000);
	memcpy(dhcp->chaddr, s_mac, 6);

	op = dhcp->option;
	int op_num = 0;
	

	//start option flag
	op[0] = 0x63;
	op[1] = 0x82;
	op[2] = 0x53;
	op[3] = 0x63;
	

	op_num = 4;
  op[op_num++] = 53;
  op[op_num++] = 1;
  op[op_num++] = 1;
  
  op[op_num++] = 60;
  op[op_num++] = 10;
  memcpy((op + op_num), cli_id, 10);
  op_num += 10;

  op[op_num++] = 12;
  op[op_num++] = strlen(hostname);
  memcpy((op + op_num), hostname, strlen(hostname));
  op_num += strlen(hostname);

  op[op_num++] = 55;//Parameter request list
  op[op_num++] = 7;
  op[op_num++] = 1;
  op[op_num++] = 3;
  op[op_num++] = 6;
  op[op_num++] = 51;
	op[op_num++] = 54;
	op[op_num++] = 58;
	op[op_num++] = 59;

	op[op_num++] = 0xff;

  if(op_num < 64) {
      for(i = op_num; i < 64; ++i) {
          op[i] = 0;
      }
      op_num = 64;
  }

	sendto(sockfd, buffer, 300, 0, (struct sockaddr *)addr, sizeof(struct sockaddr));
}

void send_udp_request(int sockfd, struct sockaddr_in *addr, 
	char *servaddr, char *ipaddr)
{
	char buffer[1024];
	memset(buffer, 0, 1024);
	struct dhcp_packet *dhcp;
	ne_u8 cli_id[10] = {50,48,49,52,50,49,51,49,51,51};
	ne_u8 hostname[32] = {0};
	ne_u8 *op = NULL;
	time_t t;
	srand((unsigned) time(&t));
	int i;
	
	ne_u8 s_mac[6] = {0x08,0x00,0x27,0xd3,0xfd,0xae};
	
	gethostname(hostname, sizeof(hostname));

	dhcp = (struct dhcp_packet *)buffer;
	dhcp->op = 1;
	dhcp->htype = 1;
	dhcp->hlen = 6;
	dhcp->hops = 0;
	int aa = rand();
	dhcp->xid = ntohl(aa);
	dhcp->secs = 0;
	dhcp->flags = htons(0x8000);
	memcpy(dhcp->chaddr, s_mac, 6);

	op = dhcp->option;
	int op_num = 0;
	

	//start option flag
	op[0] = 0x63;
	op[1] = 0x82;
	op[2] = 0x53;
	op[3] = 0x63;
	

	op_num = 4;
  op[op_num++] = 53;
  op[op_num++] = 1;
  op[op_num++] = 3;
  
  op[op_num++] = 50;
  op[op_num++] = 4;
  memcpy((op + op_num), ipaddr, 4);
  op_num += 4;
  
  op[op_num++] = 54;
  op[op_num++] = 4;
  memcpy((op + op_num), servaddr, 4);
  op_num += 4;
  
  op[op_num++] = 60;
  op[op_num++] = 10;
  memcpy((op + op_num), cli_id, 10);
  op_num += 10;

  op[op_num++] = 12;
  op[op_num++] = strlen(hostname);
  memcpy((op + op_num), hostname, strlen(hostname));
  op_num += strlen(hostname);

  op[op_num++] = 55;//Parameter request list
  op[op_num++] = 7;
  op[op_num++] = 1;
  op[op_num++] = 3;
  op[op_num++] = 6;
  op[op_num++] = 51;
	op[op_num++] = 54;
	op[op_num++] = 58;
	op[op_num++] = 59;

	op[op_num++] = 0xff;

  if(op_num < 64) {
      for(i = op_num; i < 64; ++i) {
          op[i] = 0;
      }
      op_num = 64;
  }

	sendto(sockfd, buffer, 300, 0, (struct sockaddr *)addr, sizeof(struct sockaddr));
}

void send_udp_release(int sockfd, struct sockaddr_in *addr, char *servaddr, char *ipaddr) {
	char buffer[1024];
	memset(buffer, 0, 1024);
	struct dhcp_packet *dhcp;
	ne_u8 hostname[32] = {0};
	ne_u8 *op = NULL;
	time_t t;
	srand((unsigned) time(&t));
	int i;
	
	ne_u8 s_mac[6] = {0x08,0x00,0x27,0xd3,0xfd,0xae};
	
	gethostname(hostname, sizeof(hostname));

	dhcp = (struct dhcp_packet *)buffer;
	dhcp->op = 1;
	dhcp->htype = 1;
	dhcp->hlen = 6;
	dhcp->hops = 0;
	int aa = rand();
	dhcp->xid = ntohl(aa);
	dhcp->secs = 0;
	dhcp->flags = htons(0x0000);
	memcpy(dhcp->chaddr, s_mac, 6);
	
	memcpy(dhcp->ciaddr, ipaddr, 4);

	op = dhcp->option;
	int op_num = 0;
	

	//start option flag
	op[0] = 0x63;
	op[1] = 0x82;
	op[2] = 0x53;
	op[3] = 0x63;
	

	op_num = 4;
  op[op_num++] = 53;
  op[op_num++] = 1;
  op[op_num++] = 7;
  
  op[op_num++] = 54;
  op[op_num++] = 4;
  memcpy((op + op_num), servaddr, 4);
  op_num += 4;
  
  op[op_num++] = 12;
  op[op_num++] = strlen(hostname);
  memcpy((op + op_num), hostname, strlen(hostname));
  op_num += strlen(hostname);

	op[op_num++] = 0xff;

  if(op_num < 64) {
      for(i = op_num; i < 64; ++i) {
          op[i] = 0;
      }
      op_num = 64;
  }

	sendto(sockfd, buffer, 300, 0, (struct sockaddr *)addr, sizeof(struct sockaddr));
}

void send_udp_inform(int sockfd, struct sockaddr_in *addr, char *ipaddr) {
	char buffer[1024];
	memset(buffer, 0, 1024);
	struct dhcp_packet *dhcp;
	ne_u8 hostname[32] = {0};
	ne_u8 *op = NULL;
	time_t t;
	srand((unsigned) time(&t));
	int i;
	
	ne_u8 s_mac[6] = {0x08,0x00,0x27,0xd3,0xfd,0xae};
	
	gethostname(hostname, sizeof(hostname));

	dhcp = (struct dhcp_packet *)buffer;
	dhcp->op = 1;
	dhcp->htype = 1;
	dhcp->hlen = 6;
	dhcp->hops = 0;
	int aa = rand();
	dhcp->xid = ntohl(aa);
	dhcp->secs = 0;
	dhcp->flags = htons(0x0000);
	memcpy(dhcp->chaddr, s_mac, 6);
	
	memcpy(dhcp->ciaddr, ipaddr, 4);

	op = dhcp->option;
	int op_num = 0;
	

	//start option flag
	op[0] = 0x63;
	op[1] = 0x82;
	op[2] = 0x53;
	op[3] = 0x63;
	

	op_num = 4;
  op[op_num++] = 53;
  op[op_num++] = 1;
  op[op_num++] = 8;
  
  op[op_num++] = 12;
  op[op_num++] = strlen(hostname);
  memcpy((op + op_num), hostname, strlen(hostname));
  op_num += strlen(hostname);
  
  op[op_num++] = 55;//Parameter request list
  op[op_num++] = 7;
  op[op_num++] = 1;
  op[op_num++] = 3;
  op[op_num++] = 6;
  op[op_num++] = 51;
	op[op_num++] = 54;
	op[op_num++] = 58;
	op[op_num++] = 59;

	op[op_num++] = 0xff;

  if(op_num < 64) {
      for(i = op_num; i < 64; ++i) {
          op[i] = 0;
      }
      op_num = 64;
  }

	sendto(sockfd, buffer, 300, 0, (struct sockaddr *)addr, sizeof(struct sockaddr));
}

void send_udp_renew(int sockfd, struct sockaddr_in *addr, char *ipaddr) {
	char buffer[1024];
	memset(buffer, 0, 1024);
	struct dhcp_packet *dhcp;
	ne_u8 cli_id[10] = {50,48,49,52,50,49,51,49,51,51};
	ne_u8 hostname[32] = {0};
	ne_u8 *op = NULL;
	time_t t;
	srand((unsigned) time(&t));
	int i;
	
	ne_u8 s_mac[6] = {0x08,0x00,0x27,0xd3,0xfd,0xae};
	
	gethostname(hostname, sizeof(hostname));

	dhcp = (struct dhcp_packet *)buffer;
	dhcp->op = 1;
	dhcp->htype = 1;
	dhcp->hlen = 6;
	dhcp->hops = 0;
	int aa = rand();
	dhcp->xid = ntohl(aa);
	dhcp->secs = 0;
	dhcp->flags = htons(0x0000);
	memcpy(dhcp->chaddr, s_mac, 6);

	op = dhcp->option;
	int op_num = 0;
	
	memcpy(dhcp->ciaddr, ipaddr, 4);

	//start option flag
	op[0] = 0x63;
	op[1] = 0x82;
	op[2] = 0x53;
	op[3] = 0x63;
	

	op_num = 4;
  op[op_num++] = 53;
  op[op_num++] = 1;
  op[op_num++] = 3;
  
  op[op_num++] = 60;
  op[op_num++] = 10;
  memcpy((op + op_num), cli_id, 10);
  op_num += 10;

  op[op_num++] = 12;
  op[op_num++] = strlen(hostname);
  memcpy((op + op_num), hostname, strlen(hostname));
  op_num += strlen(hostname);

  op[op_num++] = 55;//Parameter request list
  op[op_num++] = 7;
  op[op_num++] = 1;
  op[op_num++] = 3;
  op[op_num++] = 6;
  op[op_num++] = 51;
	op[op_num++] = 54;
	op[op_num++] = 58;
	op[op_num++] = 59;

	op[op_num++] = 0xff;

  if(op_num < 64) {
      for(i = op_num; i < 64; ++i) {
          op[i] = 0;
      }
      op_num = 64;
  }

	sendto(sockfd, buffer, 300, 0, (struct sockaddr *)addr, sizeof(struct sockaddr));
}

void readOption(struct dhcp_options *options, char *buf) {
	
	memset(options, 0, sizeof(struct dhcp_options));
	ne_u8 *tmp = buf + DHCP_FIXED_NON_OPTION;
	tmp += 4;
	while(1) {
		if(*tmp == 0xff) {
			break;
		}
		
		memcpy(&(options->len[(int)(*tmp)]), tmp+1, 1);
		memcpy(options->val[(int)(*tmp)], tmp+2, (int)(*(tmp+1)));
		tmp++;
		tmp += *tmp + 1;
	}
}

int setIfAddr(char *ifname, char *Ipaddr, char *mask, char *gateway)
{
    int fd;
    int rc;
    struct ifreq ifr; 
    struct sockaddr_in *sin;
    struct rtentry  rt;
    
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0)
    {
            perror("socket   error");     
            return -1;     
    }
    memset(&ifr,0,sizeof(ifr)); 
    strcpy(ifr.ifr_name,ifname); 
    sin = (struct sockaddr_in*)&ifr.ifr_addr;     
    sin->sin_family = AF_INET;  
   
    //ipaddr
    if(inet_aton(Ipaddr,&(sin->sin_addr)) < 0)   
    {     
        perror("inet_aton   error");     
        return -2;     
    }    
    
    if(ioctl(fd,SIOCSIFADDR,&ifr) < 0)   
    {     
        perror("ioctl   SIOCSIFADDR   error");     
        return -3;     
    }

    //netmask
    if(inet_aton(mask,&(sin->sin_addr)) < 0)   
    {     
        perror("inet_pton   error");     
        return -4;     
    }    
    if(ioctl(fd, SIOCSIFNETMASK, &ifr) < 0)
    {
        perror("ioctl");
        return -5;
    }

    //gateway
    memset(&rt, 0, sizeof(struct rtentry));
    memset(sin, 0, sizeof(struct sockaddr_in));
    sin->sin_family = AF_INET;
    sin->sin_port = 0;
    if(inet_aton(gateway, &sin->sin_addr)<0)
    {
       printf ( "inet_aton error\n" );
    }
    memcpy ( &rt.rt_gateway, sin, sizeof(struct sockaddr_in));
    ((struct sockaddr_in *)&rt.rt_dst)->sin_family=AF_INET;
    ((struct sockaddr_in *)&rt.rt_genmask)->sin_family=AF_INET;
    rt.rt_flags = RTF_GATEWAY;
    if (ioctl(fd, SIOCADDRT, &rt)<0)
    {
        perror( "ioctl(SIOCADDRT) error in set_default_route\n");
        close(fd);
        return -1;
    }
    close(fd);
    return rc;
}

/**   
    *   计算两个时间的间隔，得到时间差   
    *   @param   struct   timeval*   resule   返回计算出来的时间   
    *   @param   struct   timeval*   x             需要计算的前一个时间   
    *   @param   struct   timeval*   y             需要计算的后一个时间   
    *   return   -1   failure   ,0   success   
**/   
int   timeval_subtract(struct   timeval*   result,   struct   timeval*   x,   struct   timeval*   y)   
{   
      int   nsec;   
  
      if   (   x->tv_sec>y->tv_sec   )   
                return   -1;   
  
      if   (   (x->tv_sec==y->tv_sec)   &&   (x->tv_usec>y->tv_usec)   )   
                return   -1;   
  
      result->tv_sec   =   (   y->tv_sec-x->tv_sec   );   
      result->tv_usec   =   (   y->tv_usec-x->tv_usec   );   
  
      if   (result->tv_usec<0)   
      {   
                result->tv_sec--;   
                result->tv_usec+=1000000;   
      }   
  
      return   0;   
} 

void writeLine(char *line) {
	FILE *fp;
	if ((fp=fopen("client.log","a+"))==NULL) 
	{  
	    printf("Open Failed.\n");  
	    return;  
	} 
	time_t t;
	time(&t);
	fprintf(fp,"%s", ctime(&t));
	fprintf(fp,"%s\n",line);
	fclose(fp); 
}
//
// hl2 metis UDP relay between port 1024 and another port (1026)
#define FHL2EVERSION 	("1.0.404")	// 2025-01-02  rhn@hotpaw.com
// from u3r.c
// clang hl2_relay.c -lpthread -lm -o hl2_relay
// cc -DPI_LINUX hl2_relay.c -lpthread -lm -o hl2_relay
// 
// ./hl2_relay -a 10.0.1.118 -p 1026  [ -tb 80 -rb 40 ]

int packets_for_hl2 =  0;

// #define PI_LINUX	(1)

#define PORT1       (  1026 )           // default server port for SDR app
#define HL2PORT     (  1024 )           // Hermes Lite 2 UDP/IP port number
#define IPADDR2     ( 0x7f000001L )     // 127.0.0.1  == localhost
#define IPSTR_0     ( "127.0.0.1" )     // 
// #define IPADDR2  ( 0x0a000176 )      // 10.0.1.118 == hl2
// #define IPSTR_0  ( "10.0.1.118" )    // 

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <sys/types.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <sys/time.h>

#include <signal.h>
#include <errno.h>

#include <sys/stat.h>

#ifdef PI_LINUX
#include <sys/ioctl.h>
#include <net/if.h>
#endif

#ifdef nodef123
#endif  //  nodef123

// foo6 u3r

int       debugFlag    =     0;

int	  tb_levelmax  =    80;
int	  rb_levelmax  =    40;
long int  sleep_uS     =  2625; // for 381/S

char      ip_str[      32]  =  { 0, 0 };
char      port1_ip_str[32]  =  { 0, 0 };
char      port2_ip_str[32]  =  { 0, 0 };
int       hl2_started_flag  =  0;
uint32_t  hl2_ipAddr   =  0;
uint32_t  port1_ipAddr =  0;
uint32_t  port2_ipAddr =  0;
int	  server_port  =  1026;
int	  hl2_port     =  HL2PORT;	// (1024)

int	  sampleRate   =  48000;
int	  numSlices    =  1;

// UDP data handling

int       fd1  = -1;	// to send data to port 1 server client
int       fd2s = -1;	// to send data to HL2

int sendToPort1Enable  =  0;
int sendToPort2Enable  =  0;

char                    client_name_buf1[256] = { 0, 0 };
int                     listener1Port =  0;
struct sockaddr_in6     my_addr1;
struct sockaddr_in6     client_addr1;	// client/sender address
static unsigned char    rcvDataBuf1[ 2048];	// 1032 needed
int                     port1ListenerRunning =  0;

struct sockaddr_in      hl2_sockAddr;

char                    client2_name_buf[256] = { 0, 0 };
char                    dest2_name_buf[  256] = { 0, 0 };
int                     listener2Port =  0;
struct sockaddr_in6     my_addr2;
struct sockaddr_in6     client2_addr;	// client/sender sendto address
static unsigned char    rcvDataBuf2[ 2048 ];	// 1032 needed
int                     port2ListenerRunning =  0;

struct sockaddr_in 	dest2_addr;

// circular fifo/buffers

int data1032Lim = 1024;
unsigned char data1032_to_HL2[1032*1025];
int data1032_to_HL2in_idx =  0;
int data1032_to_HL2outidx =  0;

unsigned char data1032FromHL2[1032*1025];
long int data1032FromHL2in_idx =  0;
long int data1032FromHL2outidx =  0;

// debug variables

int hl2_ep6           =  0;	// port2 errors	detected
int hl2_rcvSeqNum     =  0;
int client_rcvSeqNum  =  0;
int clientErrors      =  0;	// port1 errors detected

int fromClient1CountA =  0;
int fromClient1CountD =  0;
int fromClient2CountA =  0;
int fromClient2CountD =  0;
int toClient1CountD   =  0;
int toClient2CountD   =  0;
double   t0_1            =  0.0;
double   t0_2            =  0.0;
long int  bytesSentToHL2    =  0;
long int  bytesSentToClient =  0;

//

int sendDataToPort1(unsigned char *buf, int len);
int sendDataToPort2(unsigned char *buf, int len);

void flush_1032fifos()
{
    data1032_to_HL2in_idx =  0;
    data1032_to_HL2outidx =  0;
    data1032FromHL2in_idx =  0;
    data1032FromHL2outidx =  0;
}

float rate_check2(int flag, int b, double drate) {
    struct timeval  tvalue0;
    // long int  bytesSent =  0;
    double    t3        =  0.0;
    double    t2        =  0.0;
    double    dt        =  0.0;
	
    if ((flag == 5) || (t0_1 == 0.0)) {
        struct timeval  tvalue1;
        gettimeofday(&tvalue1, NULL);
        t0_1 = (  (double)(tvalue1.tv_usec) / 1000000.0 )
                + (double)(tvalue1.tv_sec);
        bytesSentToClient =  0;
    }
    if ((flag == 6) || (t0_2 == 0.0)) {
        struct timeval  tvalue2;
        gettimeofday(&tvalue2, NULL);
        t0_2 = (  (double)(tvalue2.tv_usec) / 1000000.0 )
                + (double)(tvalue2.tv_sec);
        bytesSentToHL2    =  0;
    }
    gettimeofday(&tvalue0, NULL);
    t2 =    ( (double)(tvalue0.tv_usec) / 1000000.0 )
            + (double)(tvalue0.tv_sec) ;
    if (flag == 1) {
        bytesSentToClient +=  b;
        dt =  t2 - t0_1;
    }
    if (flag == 2) {
        bytesSentToHL2    +=  b;
        dt =  t2 - t0_2;
    }
    float ahead  =  0.0;
    float behind =  0.0;
    if (dt != 0.0) {
        int b  =  bytesSentToHL2;
	if (flag == 1) {
	    b  =  bytesSentToClient;
	}
        double arate =  (double)b / dt;	// actual
        ahead  =   arate - drate;	// is actual faster than desired
        behind =  -ahead;
	if (ahead == 0.0) { behind = 0.0; }
        // printf("bytes %d dt %lf rates %lf %lf\n", b, dt, arate, drate);
    }
    return(behind);	// if behind, speed up
}  //  rate_check2()

void store1032FromHL2(unsigned char *dbuf)
{
    // memcpy dest src n bytes
    memcpy(&data1032FromHL2[1032*data1032FromHL2in_idx], dbuf, 1032);
    data1032FromHL2in_idx += 1;
    if (data1032FromHL2in_idx >= data1032Lim) {
	data1032FromHL2in_idx = 0;
    }
}

int  get1032FromHL2(unsigned char *dbuf)
{
    if (data1032FromHL2outidx == data1032FromHL2in_idx) {
        return(0);
    }
    memcpy(dbuf, &data1032FromHL2[1032*data1032FromHL2outidx], 1032);
    data1032FromHL2outidx += 1;
    if (data1032FromHL2outidx >= data1032Lim) {
	data1032FromHL2outidx = 0;
    }
    return(1032);
}

int levelOf1032FromHL2()
{
	int n = data1032FromHL2in_idx - data1032FromHL2outidx ;
	if (n >= data1032Lim) { n = 0; }
	if (n <  0) { n += data1032Lim; }
	return(n);
}

void store1032_to_HL2(unsigned char *dbuf)
{
    // memcpy dest src n bytes
    memcpy(&data1032_to_HL2[1032*data1032_to_HL2in_idx], dbuf, 1032);
    data1032_to_HL2in_idx += 1;
    if (data1032_to_HL2in_idx >= data1032Lim) {
	data1032_to_HL2in_idx = 0;
    }
}

int  get1032_to_HL2(unsigned char *dbuf)
{
    if (data1032_to_HL2outidx == data1032_to_HL2in_idx) {
        return(0);
    }
    memcpy(dbuf, &data1032_to_HL2[1032*data1032_to_HL2outidx], 1032);
    data1032_to_HL2outidx += 1;
    if (data1032_to_HL2outidx >= data1032Lim) {
	data1032_to_HL2outidx = 0;
    }
    return(1032);
}

int levelOf1032_to_HL2()
{
	int n = data1032_to_HL2in_idx - data1032_to_HL2outidx ;
	if (n >= data1032Lim) { n = 0; }
	if (n <  0) { n += data1032Lim; }
	return(n);
}

// MARK: ------

int sendDataToPort1(unsigned char *dbuf, int bufLen)
{
    int status = 0;
    if (fd1 < 0) { return(0); }
    else {
        socklen_t len6      =  sizeof(client_addr1);
	status = sendto( fd1, &dbuf[0], bufLen,
                         0,
			 (struct sockaddr *)&client_addr1, 
                         len6 );
    }
    if (status > 0) {
        if (0 && toClient1CountD < 4) {
            int n = levelOf1032FromHL2();
	    printf("%d bytes sent to port 1, level = %d\n", status, n);
	    toClient1CountD += 1;
	}
    }
    return(status);
}

void ip_to_string(uint32_t ip, char *buffer) {
    unsigned char bytes[4];
    bytes[0] = ip >> 24;
    bytes[1] = (ip >> 16) & 0xFF;
    bytes[2] = (ip >> 8) & 0xFF;
    bytes[3] = ip & 0xFF;

    sprintf(buffer, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
}

void displayClient1Name() 
{
        inet_ntop( AF_INET6,
	           &(client_addr1.sin6_addr),
	           client_name_buf1, 127 );
        client_name_buf1[127] = 0;
	uint32_t ip1 =  ntohs(client_addr1.sin6_port);
        printf("Received  command from client1 : %s #%d \n", 
	        &client_name_buf1[0], ip1);
}

int handleDataFromPort1(unsigned char *dbuf, int n)
{
    if ((n == 64) || (fromClient1CountD < 4)) {
        // printf("%d bytes received on port1 0x", n);
        int lvl1 =  levelOf1032FromHL2();
	for (int i=0;i<4;i++) {
	    // printf("%02X", dbuf[i]);
	}
	// printf(" , lvl %d \n", lvl1);
        fromClient1CountD += 1;
    }
    if (n <= 64 && client_name_buf1[0] == 0) {
        displayClient1Name() ;
    }
    if (fromClient1CountA < 4) {
        // displayClient1Name() ;
        fromClient1CountA += 1;
    }
    if (fd2s != -1) { 
        if (n == 1032) {	// 
	    int syncErr =  (   (dbuf[11 - 3] != 0x7F)
		            || (dbuf[11 - 2] != 0x7F)
		            || (dbuf[11 - 1] != 0x7F) );
	    if (syncErr) { 
		clientErrors += 1;
		if (debugFlag > 2) {
		    printf("port1 input syncErr\n");
		}
	    } else {
	        int rcvSeqNum  = (  (dbuf[4] << 24) 
	                          | (dbuf[5] << 16) 
	                          | (dbuf[6] <<  8) 
	                          | (dbuf[7]      ) );
	        if (rcvSeqNum != (client_rcvSeqNum + 1)) {
		    clientErrors += 1;
		    if (debugFlag > 2) {
		        printf("port1 input seq err %d\n", rcvSeqNum);
		    }
	        }
	        client_rcvSeqNum =  rcvSeqNum;
	    }

            // update sample rate and number of slices
            store1032_to_HL2(dbuf);
            packets_for_hl2 += 1;
        } else {
            // check for start stop
	    sendDataToPort2(dbuf, n);
        }
    }
    return(0);
}

void *receiveLoop1(void *param)
{
    int port1 = listener1Port;

    memset( &my_addr1, 0, sizeof(my_addr1) );
    // bzero((char *) &my_addr1, sizeof(my_addr1));

    if ( (fd1 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0 ) {
        perror( "socket failed" );
        return(NULL);
    }

    int rr = 1;
    setsockopt(fd1, SOL_SOCKET, SO_REUSEADDR,
               (char *)&rr, sizeof(int));

    my_addr1.sin6_flowinfo    =  0;
    my_addr1.sin6_family      =  AF_INET6;
    my_addr1.sin6_port        =  htons( port1 );
    my_addr1.sin6_addr        =  in6addr_any;

    if ( bind(fd1, (struct sockaddr *)&my_addr1, sizeof(my_addr1)) < 0 ) {
        perror( "bind failed" );
        return(NULL);
    }

    // printf("waiting for messages on port %d \n", port);

    port1ListenerRunning  =  1;

    while (port1ListenerRunning > 0) {
        int        flags    =  0;
	int	   length   =  0;
	socklen_t addr6Len  =  sizeof(client_addr1);
	bzero(rcvDataBuf1, 1032);
	length = recvfrom( fd1, rcvDataBuf1, 1032, flags,
			(struct sockaddr *)&client_addr1, &addr6Len);
        if ( length < 0 ) {
            perror( "recvfrom failed" );
            port1ListenerRunning  =  0;
            break;
        } else if (length > 0) {
            unsigned char *db = rcvDataBuf1;
            handleDataFromPort1(db, length);
	}
    }
    close( fd1 );
    printf("port1 %d closed \n", port1);
    return(param);
}  // receiveLoop1()

long int param1[32] = { 0 , 0 };

void startThread1(int port1)
{
    listener1Port = port1;
    pthread_t udp_send_thread;
    if (1) {
        void *param =  (void *)&param1[0];
        int r = pthread_create( &udp_send_thread,
                            NULL ,
                            receiveLoop1,
                            (void *)param
                );
        if (r < 0) {
            printf("could not create thread1");
        } else {
	    if (debugFlag > 1) {
                printf("thread1 started \n");
	    }
        }
    }
}

// MARK: --------------

void displayClient2Name() 
{
        // &(client2_addr.sin6_addr),
        // struct sockaddr_in 	dest2_addr;
        inet_ntop( AF_INET,
	           &(dest2_addr.sin_addr),
	           dest2_name_buf, 127 );
        dest2_name_buf[127] = 0;
	uint32_t ip2 =  ntohs(dest2_addr.sin_port);
        printf("Received UDP data from client2 : %s #%d \n", 
	        &dest2_name_buf[0], ip2);
}

int handleDataFromPort2(unsigned char *dbuf, int n)
{
    if ((n == 64) || (fromClient2CountD < 4)) {
        // printf("%d bytes received on port 2\n", n);
        fromClient2CountD += 1;
    }
    if (dest2_name_buf[0] == 0) {
        displayClient2Name() ;
    }
    if (fromClient2CountA < 2) {
        // displayClient2Name() ;
        fromClient2CountA += 1;
    }
    if (fd1 != -1) { 
        if (n == 1032) {		// 4032
	    int syncErr =  (   (dbuf[11 - 3] != 0x7F)
		            || (dbuf[11 - 2] != 0x7F)
		            || (dbuf[11 - 1] != 0x7F) );
	    if (syncErr) { 
	        hl2_ep6 += 1;
		if (debugFlag > 2) {
		    printf("port2 input syncErr\n");
		}
	    } else {
	        int rcvSeqNum  = (  (dbuf[4] << 24) 
	                          | (dbuf[5] << 16) 
	                          | (dbuf[6] <<  8) 
	                          | (dbuf[7]      ) );
	        if (rcvSeqNum != (hl2_rcvSeqNum + 1)) {
	            hl2_ep6 += 1;
		    if (debugFlag > 2) {
		        printf("port2 input seq err %d\n", rcvSeqNum);
		    }
	        }
	        hl2_rcvSeqNum =  rcvSeqNum;
	    }

            store1032FromHL2(dbuf);
            int lvl1 =  levelOf1032FromHL2(); 	// data to send to port 1
	    // printf("lvl1 = %d\n",lvl1);
        } else {
            sendDataToPort1(dbuf, n);
        }
    }
    return(0);
}

void *receiveLoop2(void *param)
{
    int port2 = listener2Port;

    memset( &my_addr2, 0, sizeof(my_addr2) );
    // bzero((char *) &my_addr2, sizeof(my_addr2));

    // int rr = 1;

    my_addr2.sin6_flowinfo    =  0;
    my_addr2.sin6_family      =  AF_INET6;
    my_addr2.sin6_port        =  htons( port2 );
    my_addr2.sin6_addr        =  in6addr_any;

    memset( &hl2_sockAddr, 0, sizeof(hl2_sockAddr) );
    hl2_sockAddr.sin_family = AF_INET;
    // hl2_sockAddr.sin_addr.s_addr = htonl( hl2_ipAddr );
    // hl2_sockAddr.sin_port = htons( 0L );
    hl2_sockAddr.sin_port = htons( HL2PORT );
    hl2_sockAddr.sin_addr.s_addr = htonl( INADDR_ANY );
    socklen_t addrLen2  =  sizeof(hl2_sockAddr);

    // printf("waiting for data on 2nd port %d \n", port2);

    port2ListenerRunning  =  1;

    while (port2ListenerRunning > 0) {
        int        flags    =  0;
	int	   length   =  0;
	// socklen_t addr6Len  =  sizeof(client2_addr);
	socklen_t addrLen2  =  sizeof(struct sockaddr_in);
	socklen_t addrLen   =  sizeof(dest2_addr);
	bzero(rcvDataBuf2, 1032);

        // printf("blocking on port 2 rcv\n");

	length = recvfrom( fd2s, rcvDataBuf2, 1032, flags,
                        (struct sockaddr *)&dest2_addr, &addrLen) ;

        // printf("received something on port 2\n");

	if ( length < 0 ) {
            perror( "recvfrom failed" );
            port2ListenerRunning  =  0;
            break;
        } else if (length > 0) {
            unsigned char *db = rcvDataBuf2;
            handleDataFromPort2(db, length);
	}
    }
    // close( fd2x );
    printf("port 2 %d closed \n", port2);
    return(param);
}  // receiveLoop2()

unsigned char  mySendBuf2[2048];

int setupPort2(uint32_t ipAddr, int port2s)
{

    if ( (fd2s = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket 2s failed");
        return(-1);
    }

    struct linger ling = {1,0};
    int rr = 1;
    setsockopt(fd2s, SOL_SOCKET, SO_REUSEADDR,
            (char *)&rr, sizeof(int));
    setsockopt(fd2s, SOL_SOCKET, SO_LINGER,
            (char *)&ling, sizeof(ling));

    int addrLen = sizeof(dest2_addr);

    memset( &dest2_addr, 0, sizeof(dest2_addr) );
    dest2_addr.sin_family = AF_INET;
    dest2_addr.sin_port = htons( port2s );
    // uint32_t    ipAddr  =  0x7f000001;  // 127.0.0.1 == localhost
    // dest2_addr.sin_addr.s_addr = htonl( 0x0a000176 );  // 10.0.1.118
    dest2_addr.sin_addr.s_addr = htonl( ipAddr ); 

    return(0);
}

int sendDataToPort2(unsigned char *buf, int len)
{
    if (fd2s == -1) { return(0); }
    int addrLen = sizeof(dest2_addr);
    socklen_t addrLen2  =  sizeof(struct sockaddr_in);
      
    int r = sendto( fd2s, buf, len, 0,
                    (struct sockaddr *)&dest2_addr, addrLen) ;

    if (r > 0) {
        if (0 && toClient2CountD < 4) {
            int n = levelOf1032_to_HL2();
	    printf("%d bytes sent to port 2, level = %d\n", r, n);
            toClient2CountD += 1;
        }
	if (   (len == 64)
	    && (buf[0] == 0xef)
	    && (buf[1] == 0xfe)
	    && (buf[2] == 0x04) ) {
	        if ((buf[3] & 0x01) != 0) {
	            // start command
                    if (hl2_started_flag == 0) {
	                printf("hl2 streaming started\n");
                        packets_for_hl2   =  0;
		    }
                    hl2_started_flag =  1;
	        } else {
	            // stop command
                    if (hl2_started_flag != 0) {
	                printf("hl2 streaming stopped\n");
		    }
                    hl2_started_flag =  0;
		    if (debugFlag > 0) {
                        int lvl1 =  levelOf1032FromHL2();  // to port 1
                        int lvl2 =  levelOf1032_to_HL2();
		        printf( "lvl1 %d lvl2 %d err1 %d err2 %d\n",
		    	        lvl1, lvl2, clientErrors, hl2_ep6 );
		    }
		    // reset fifos and debug variables
                    flush_1032fifos();
                    sendToPort1Enable =  0;
                    sendToPort2Enable =  0;
                    hl2_rcvSeqNum     =  0;
                    client_rcvSeqNum  =  0;
                    hl2_ep6           =  0;
                    clientErrors      =  0;
                    memset( client_name_buf1, 0, 256);
                    memset( dest2_name_buf  , 0, 256);
	        }
	} else
	if (   (len == 1032)
	    && (buf[0] == 0xef)
	    && (buf[1] == 0xfe) 
	    && (buf[1] == 0x01) 
	    && (buf[1] == 0x02) ) {
    	    int C0_addr =  buf[ 11] >> 1;
	    if (C0_addr == 0) {
	        int slices =  0x00ff & (buf[14] >> 3);
		int r0     =  0x03 & buf[12];
		int srate  =  48000;
		if (     r0 == 0) { srate =   48000;}
		else if (r0 == 1) { srate =   96000;}
		else if (r0 == 2) { srate =  192000;}
		else if (r0 == 3) { srate =  384000;}

		if (srate != sampleRate) {
		    printf("sample rate now %d\n", srate);
		}
		if (slices != numSlices) {
		    printf("number of slices now %d\n", slices);
		}
                sampleRate       =  srate;
                numSlices        =  slices;
	    }
	}

    } else {
            int n = levelOf1032_to_HL2();
	    printf("send to port 2 failure %d, level = %d\n", r, n);
    }
    return(r);
}

long int param2[32] = { 0 , 0 };

void startThread2(int port2)
{
    listener2Port = port2;
    pthread_t udp_send_thread;
    if (1) {
        void *param =  (void *)&param2[0];
        int r = pthread_create( &udp_send_thread,
                            NULL ,
                            receiveLoop2,
                            (void *)param
                );
        if (r < 0) {
            printf("could not create thread2");
        } else {
	    if (debugFlag > 1) {
                printf("thread2 started \n");
	    }
        }
    }
}

void stopThread2()
{
    if (port2ListenerRunning > 0) {
        port2ListenerRunning = 0;
    }
}

// utility functions

uint32_t getDecimalValueOfIPV4_String(const char* ipAddress, int mod)
{           
    uint8_t ipbytes[4]={};
    int    i =  0;
    int8_t j =  3;
    while (ipAddress+i && i<strlen(ipAddress)) {
        char digit = ipAddress[i];
        if (isdigit(digit) == 0 && digit != '.') {
            return 0;
        }            
        j = (digit=='.') ? (j-1) : j;
        ipbytes[j]= ipbytes[j]*10 + atoi(&digit);

        i++;
    }
   
    uint32_t a =             ipbytes[0];
    uint32_t b =  ( uint32_t)ipbytes[1] <<  8;
    uint32_t c =  ( uint32_t)ipbytes[2] << 16;
    uint32_t d =  ( uint32_t)ipbytes[3] << 24;
    if (mod > 0) { a = mod; }   //  for testing
    return (a + b + c + d);
}

struct sigaction    sigact, sigign;
static void sighandler(int signum);

static void sighandler(int signum)
{   
    fprintf(stderr, "Signal caught, exiting!\n");
    fflush(stderr);         
    port1ListenerRunning = 0;
    port2ListenerRunning = 0;
    exit(-1);
}

void handle_args(int argc, char **argv)  
{
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            printf("udp relay version %s\n", FHL2EVERSION	);
            // printUsage();
            exit(0);
        }
        int arg = 3;
        for (arg=3; arg<=argc; arg+=2) {
            // printf("%d >%s<\n", arg, argv[arg-2]);
            if (strcmp(argv[arg-2], "-debug") == 0) {
                int k           =  atoi(argv[arg-1]);
                debugFlag       =  k;
            }
	    if (strcmp(argv[arg-2], "-x") == 0) {
                float x         =  atof(argv[arg-1]);
		// unused
            }
            if (strcmp(argv[arg-2], "-k") == 0) {
                int k           =  atoi(argv[arg-1]);
		// unused
            }
            if (strcmp(argv[arg-2], "-p") == 0) {
                int k           =  atoi(argv[arg-1]);
                server_port     =  k;
		// printf("listening on port %d\n", server_port);
            }
            if (strcmp(argv[arg-2], "-p2") == 0) {
                int k           =  atoi(argv[arg-1]);
                hl2_port        =  k;
		printf("hl2 port %d\n", hl2_port);
            }
            if (strcmp(argv[arg-2], "-a") == 0) {
                char *s         =  argv[arg-1];
                hl2_ipAddr      =  getDecimalValueOfIPV4_String(s, -1);
                strncpy(ip_str, s, 30); ip_str[31] = 0;
		printf("looking for HL2 at %s\n", s);
            }
            if (strcmp(argv[arg-2], "-tb") == 0) {
                int k           =  atoi(argv[arg-1]);
		if (k > 2 && k < 1000) {
                    tb_levelmax =  k;
		}
            }
            if (strcmp(argv[arg-2], "-rb") == 0) {
                int k           =  atoi(argv[arg-1]);
		if (k > 2 && k < 1000) {
                    rb_levelmax =  k;
		}
            }
        }
    }
}

int min_level_2         =  0;
double t0_level_2       =  0.0;
int    pc_level_2       =  0;

int toPort2_HighCount   =  0;
int toPort2_Low_Count   =  0;

void debug_rate_print(long int loops) {
    int n     =  (loops % 381);
    int lvl1 =  levelOf1032FromHL2(); // data to in buffer be sent to port 1
    int lvl2 =  levelOf1032_to_HL2(); // data in buffer to be sent to HL2
    if (n ==   0) {
        double t1 =  0.0;
        struct timeval  tvalue1;
        gettimeofday(&tvalue1, NULL);
        t1   = (  (double)(tvalue1.tv_usec) / 1000000.0 )
                + (double)(tvalue1.tv_sec);
        min_level_2 =  1000;
        t0_level_2  =  t1;
	pc_level_2  =  packets_for_hl2;
        toPort2_HighCount =  0;
        toPort2_Low_Count =  0;
    } else {
        if ( lvl2 < min_level_2 ) {
            min_level_2 = lvl2 ;
	}
    }
    if (n == 380) {
        double t1 =  0.0;
	struct timeval  tvalue1;
        gettimeofday(&tvalue1, NULL);
        t1   = (  (double)(tvalue1.tv_usec) / 1000000.0 )
                + (double)(tvalue1.tv_sec);
	double dt   =  t1 - t0_level_2;
	int    np =  packets_for_hl2 - pc_level_2 ;
	if (dt > 0.0) {
            int h = toPort2_HighCount; 
            int u = toPort2_Low_Count;
	    float  rate =  (float)np / dt;
	    printf("%6ld rate %f min level2 %2d %2d %2d HU %2d %2d\n", 
	        loops, rate, min_level_2, lvl2, lvl1, h, u);
	}
    }
    // long int bytesSentToHL2 
    // double rate   =  1032.0 * (48000.0 / 126.0) ;
    // double behind =  1000.0 * rate_check2(2, 0, rate) ; // mS
    // printf("*** %5d level %d %d behind %f\n", n,lvl1,lvl2,behind);
}

int send1032_to_port1_IfNeeded()	// to client SDR app
{
    unsigned char dbuf1[1500];
    double drate1  =  1032.0 * numSlices * ((double)sampleRate / 126.0) ;
    int n    =  1032;
    int r    =     0;

    int lvl1 =  levelOf1032FromHL2(); 	// data to send to port 1

    if (lvl1 <= 0) {
        struct timeval  tvalue2;
        gettimeofday(&tvalue2, NULL);
        t0_1 = (  (double)(tvalue2.tv_usec) / 1000000.0 )
                + (double)(tvalue2.tv_sec);
        bytesSentToClient =  0;
	if (lvl1 <= 0) {
            sendToPort1Enable =  0;
	}
    }
    if (lvl1 > 2) {
		// int	  tb_levelmax  =    40;
		// int	  rb_levelmax  =    40;
        if (sendToPort1Enable == 0) {
	    // printf("enabling send to port 1\n");
	    rate_check2(5, 0, drate1) ; // 1+4 == init
	}
	if (lvl1 >= rb_levelmax) {
            sendToPort1Enable =  1;
	}
    }
    if (sendToPort1Enable > 0 && lvl1 > 0) {
        int fromHL2 =  get1032FromHL2(&dbuf1[0]);
        lvl1 -=  1;
        if (fromHL2 == 1032) {
            sendDataToPort1(&dbuf1[0], n);
            bytesSentToClient +=  1032;
	    r += 1;
	    while (lvl1 >= rb_levelmax) {
                toPort2_HighCount += 1;
                fromHL2 =  get1032FromHL2(&dbuf1[0]);
                lvl1 -=  1;
                if ( fromHL2  == 1032) {
                    sendDataToPort1(&dbuf1[0], fromHL2);
                    bytesSentToClient +=  1032;
		}
	    }
	} 
	if (1 && lvl1 > 0) {
            double rate   =  1032.0 * (48000.0 / 126.0) ;
	    double behind =  rate_check2(1, 0, rate) ;
	    if (lvl1 > 0 /* && lvl1 < (rb_levelmax-2) */ && behind > 0.0) {
                    toPort2_Low_Count += 1;
                    int fromHL2   =  get1032FromHL2(&dbuf1[0]);
		    lvl1 -= 1;
                    if ( fromHL2  == 1032) {
                        sendDataToPort1(&dbuf1[0], fromHL2);
                        bytesSentToClient +=  1032;
		    }
  // printf("rate: speed up sending to client1 %d %lf\n", lvl1, behind);
	    }
	}
    }
    return(r);
}

int send1032_to_port2_IfNeeded() 	// to HL2
{
    unsigned char dbuf2[1500];
    double drate2  =  1032.0 * (48000.0 / 126.0) ;
    int n    =  1032;
    int r    =     0;

    int lvl2 = levelOf1032_to_HL2();

    if (lvl2 <= 0) {
        struct timeval  tvalue2;
        gettimeofday(&tvalue2, NULL);
        t0_2 = (  (double)(tvalue2.tv_usec) / 1000000.0 )
                + (double)(tvalue2.tv_sec);
        bytesSentToHL2    =  0;
	if (lvl2 <= 0) {
            sendToPort2Enable =  0;
	}
    }
    if (lvl2 > 2) {
        if (sendToPort2Enable == 0) {
	    rate_check2(6, 0, drate2) ; // 2+4 == init
	    // printf("enabling send to port 2\n");
	}
	if (lvl2 >= tb_levelmax) {
            sendToPort2Enable =  1;
	}
    }
    if (sendToPort2Enable > 0) {
        if (lvl2 > 0) {
            int toHL2   =  get1032_to_HL2(&dbuf2[0]);
            lvl2 -= 1;
            if ( toHL2  == 1032) {
                sendDataToPort2(&dbuf2[0], n);
                bytesSentToHL2 +=  1032;
	        r += 2;
	        while (lvl2 > tb_levelmax) {
                    toHL2   =  get1032_to_HL2(&dbuf2[0]);
                    lvl2 -= 1;
                    if ( toHL2  == 1032) {
                        sendDataToPort2(&dbuf2[0], toHL2);
                        bytesSentToHL2 +=  1032;
		    }
	        }
            }
	} else if (lvl2 > 0) {
            double rate   =  1032.0 * (48000.0 / 126.0) ;
	    double behind =  rate_check2(2, 0, rate) ;
	    if (1 && lvl2 > 0 && lvl2 < (tb_levelmax-2) && behind > 0.0) {
                    int toHL2   =  get1032_to_HL2(&dbuf2[0]);
		    lvl2 -= 1;
                    if ( toHL2  == 1032) {
                        sendDataToPort2(&dbuf2[0], toHL2);
                        bytesSentToHL2 +=  1032;
		    }
		    // printf("rate: speed up sending to hl2\n");
	    }
	}
    }
    return(r);
}

void init_stuff()
{
    sigact.sa_handler =  sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags   =  0;        
    sigaction(SIGINT,  &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL); 
    sigaction(SIGQUIT, &sigact, NULL); 
    #ifdef __APPLE__            
    signal(SIGPIPE, SIG_IGN);  
    #else    
    sigign.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sigign, NULL);
    #endif

    memset( &my_addr1, 0, sizeof(my_addr1) );
    memset( &client_addr1, 0, sizeof(client_addr1) );
    memset( &rcvDataBuf1[0], 0, 2048 );
    memset( &my_addr2, 0, sizeof(my_addr2) );
    memset( &client2_addr, 0, sizeof(client2_addr) );
    memset( &rcvDataBuf2[0], 0, 2048 );
}

int main( int argc, char **argv )
{
    int       port1   =  PORT1;			// local server port
    uint32_t  ipAddr2 =  IPADDR2;		// hl2 ip addr
    int       port2   =  HL2PORT;		// hl2 port

    init_stuff();

    printf("v%s ", FHL2EVERSION ); 
    handle_args(argc, argv);
    port1 =  server_port;
    port2 =  hl2_port;

    if (hl2_ipAddr != 0) {
        ipAddr2 =  hl2_ipAddr;
    } else {
	printf("looking for HL2 at default IP %s\n", IPSTR_0); 
    }

    setupPort2(ipAddr2, port2);		// set up port to send data to HL2

    startThread1(port1);		// waits for cmd data on port1
    printf("listening on port %d\n", port1);
    usleep(1000);
    startThread2(port2);		// waits for HL2 data on port2
    printf("waiting for hl2 at port %d\n", port2);
    usleep(34*1000);

    // send any UDP data packets of size 1032, if needed, either direction
    long int loops =  0;
    while ( port1ListenerRunning > 0 ) {
        send1032_to_port1_IfNeeded();
        send1032_to_port2_IfNeeded();
        usleep(sleep_uS + 0);  // sleep_uS = 2625; // 1/381th of a Second
	if ( 1 && debugFlag > 1 && hl2_started_flag ) { 
	    debug_rate_print(loops); 
	}
	loops += 1;
    }	// exits on port 1 UDP receive failures
    return(0);
}

// ToDo: print rate and fifo level for debug
// Copyright 2025 Ronald H Nicholson, Jr
//   (re)Distribution allowed under MPL 1.1 
//   Distribution under MPL 1.1 is Incompatible With Secondary Licenses
// eof

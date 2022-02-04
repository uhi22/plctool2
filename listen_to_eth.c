

/* Inspiration von 
 * https://www.opensourceforu.com/2015/03/a-guide-to-using-raw-sockets/
 * 
 * Change Log
 *   2022-01-17 Uwe
 *    - neu angelegt
 *    - umgeschrieben als Non-blocking mittels poll()
 *    - Auswertung ethernet-Protokoll zur Detektion von HomePlug
 *   2022-01-19 Uwe
 *    - Headerfiles restrukturiert, nur EIN projektspezifisches Header-File
 *    - Netzwerkkey (NMK) aus CM_SLAC_MATCH.CNF extrahieren und mit
 *      CM_SET_KEY senden
 *   2022-01-19 Uwe
 *    - Bugfix: zu kurze CM_SET_KEY korrigiert
 *    - Optimierung: Statusmeldung seltener, so dass wir die Console nicht zumüllen
 *   2022-01-24 Uwe
 *    - Bugfix: CM_SET_KEY.REQ.PID von 0 auf 4 geändert laut ISO.
 *    - Bugfix: Ungewollte 3 Byte padding am Ende der CM_SET_KEY.REQ entfernt
 *    - Bugfix: CM_SET_KEY.REQ.NID: statt fix mit "xxxxxx" zu befüllen, nehmen wir
 *        laut ISO das CM_SLAC_MATCH.CNF.NID.
 *   2022-02-01 Uwe
 *    - Feature: Tasteneinlesen vorbereitet.
 *    - Verbesserung: Saubere Dekodierung der MMTYPE
 *   2022-02-04 Uwe
 *    - Feature: Logging to file
 *    - Improvement: two sockets, one for transmit, one for receive. This helps
 *      to have a clean concept.
 *    - Improvement: binding the rx socket to eth0, to avoid seeing frames from
 *      other interfaces.
 *    - Bugfix: poll-descriptor dynamically initialized with correct socket descriptor
 *    - Decoding of CM_SET_KEY.CNF
 * 
 * 
 * 
 * Erprobungen
 *  2022-01-19/WestPark HPC Säule 4
 *    - Kopplung mit Drahtschlaufe eher schlecht. Wir sehen nur sporadisch
 *      alle Frames der Säule. Der einmal empfangene Slac-match wurde
 *      leider wegen SW-Bug in einen zu kurzen Set_key übersetzt. Todo:
 *      Ankopplung optimieren, so dass wir die Säule stabil hören.
 * 
 *  2022-01-24/morgens/Westpark HPC Säule 2
 *    - Kopplung nun direkt vom CP (im Motorraum beim OBC von der grünen Ader abgegriffen)
 *    - Fliegender Aufbau im Beifahrer-Fußraum, inkl. Spannungwandler 12V auf 230V, der
 *      stark stört. Es kommen viele, aber nicht alle SLAC-Botschaften an.
 *    - Falls wir SLAC_MATCH.conf empfangen, senden wir das SetKey. Der Devolo
 *      bestätigt das positiv, aber wir sehen trotzdem keine weitere Kommunikation.
 *
 *  2022-01-24/nachmittags/Hagebau TripeCharger
 *    - mit den "Bugfixes" meldet der Devolo cm_set_key.resp=negative.
 *    - überraschenderweise sendet auch der Ioniq  auf der Allgemein-MAC 04 65 65 FF FF FF FF eine
 *       cm_set_key.resp, und zwar positiv (nicht immer. Einmal beobachtet beim zweiten Autocharge-Versuch)
 *    - Rückbau der 3-byte-padding -> immer noch negativ.
 *    - explizite Addressierung des Devolo statt Broadcast -> immer noch negativ.
 *    - Rückbau auf PID=0 -> immer noch negativ.
 * 
 * 
 * 
 * 
 * Hinweise
 *  - Der Raspberry versucht, auf dem eth0 eine Internetverbindung zu bekommen. Es wäre
 *     sinnvoll, ihm das abzugewöhnen.
 *     Schritt 1: ARP aus. Laut https://superuser.com/questions/399517/disabling-the-arp-protocol-in-linux-box
 *        Disable ARP protocol: sudo ifconfig eth0 -arp 
 *        Enable ARP protocol:  sudo ifconfig eth0 arp
 *     Schritt 2: DHCP aus:
 *        sudo nano /etc/dhcpcd.conf
 *        add the line
 *          denyinterfaces eth0
 *        to tell the DHCP daemon to completely ignore eth0
 *        ABER: Das eth0 verschwindet so komplett. Auch im Wireshark weg. Seltsam.
 * 
 * Todos
 *   [ ] Untersuchung, wann und warum das cm_set_key nicht funktioniert
 *     - unter welchen Umständen wird es positiv oder negativ beantwortet?
 *     - welchen Unterschied macht mehrfaches Senden der gleichen Anfrage?
 *     - kann man mit get_key den NMK wieder auslesen?
 *     - muss die NID vor dem set_key auf andere Weise gesetzt werden?
 *   [x] The poll() does not receive all frames.
 *   [x] The poll() receives the frames from ALL interfaces. We should
 *        sort out everything which is not from eth0. -> bind()
 * 
 * 
 * */


#include<stdio.h>
#include<malloc.h>
#include<string.h>
#include<signal.h>
#include<stdbool.h>
#include<sys/socket.h>
#include<sys/types.h>
#include <sys/select.h>
#include <termios.h>

#include<linux/if_packet.h>
#include<netinet/in.h>		 
#include<netinet/if_ether.h>    // for ethernet header
#include<netinet/ip.h>		// for ip header
#include<netinet/udp.h>		// for udp header
#include<netinet/tcp.h>
#include<arpa/inet.h>           // to avoid warning at inet_ntoa

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>

#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <poll.h>

#include "plc_homeplug.h" /* all types and definitions related to homeplug/etc */


int blExit=0;
  
/***************************************************************
 * Terminal handling
 * */
struct termios orig_termios;

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    //cfmakeraw(&new_termios);
  new_termios.c_lflag &= ~ICANON;
  new_termios.c_lflag &= ~ECHO;
  new_termios.c_lflag &= ~ISIG;
  new_termios.c_cc[VMIN] = 0;
  new_termios.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
} 

/*********************************************************************/
/* Log File handling */
FILE* hLogFile;
char str1000[1000];
char strTmp[1000];

void printToLogAndScreen(char *s) {
	printf("%s\n", s);
	fprintf(hLogFile, "%s\n", s);
	fflush(hLogFile);
}

/*********************************************************************/

int total,nHomePlug,icmp,igmp,other,iphdrlen;
int nPollSuccess, nPollNothing, nMainLoops;
int nHpSlacMatchCnf, nHpGetSwVersion, nSetKey;

struct sockaddr_in source,dest;
int sock_fd_rx; /* the socket file descriptor for reception */
int sock_fd_tx; /* the socket file descriptor for transmission */
char ifName[IFNAMSIZ] = "eth0";
struct ifreq if_idx; /* index of the interface */
struct ifreq if_mac; /* MAC adress of the interface */
struct sockaddr_ll socket_address_tx; /* socket address info */
struct sockaddr socket_address_rx;
struct pollfd mypollfd;

#define RECEIVE_BUFFER_SIZE 65536
unsigned char receivebuffer[RECEIVE_BUFFER_SIZE];
#define TRANSMIT_BUFFER_SIZE 65536
unsigned char transmitbuffer[TRANSMIT_BUFFER_SIZE];
char myNMK[SLAC_NMK_LEN] = "hallo";
char myNID[SLAC_NID_LEN] = "1234567";

void sendSetKeyRequest(void) {
	printToLogAndScreen("sending SetKeyRequest");
	struct ethhdr *eh = (struct ethhdr *) transmitbuffer;
    struct cm_set_key_request *cmskr = (struct cm_set_key_request *) transmitbuffer;
    int tx_len;
    
    /* Construct the Ethernet header */
	memset(transmitbuffer, 0, TRANSMIT_BUFFER_SIZE);
	/* Ethernet header */
	eh->h_source[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
	eh->h_source[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
	eh->h_source[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
	eh->h_source[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
	eh->h_source[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
	eh->h_source[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];	
	eh->h_dest[0] = MY_DEST_MAC0;
	eh->h_dest[1] = MY_DEST_MAC1;
	eh->h_dest[2] = MY_DEST_MAC2;
	eh->h_dest[3] = MY_DEST_MAC3;
	eh->h_dest[4] = MY_DEST_MAC4;
	eh->h_dest[5] = MY_DEST_MAC5;

	/* Ethernet protocol type */
	eh->h_proto = htons(ETH_P_HPAV); /* Homeplug protocol 0x88e1 */

	/* Fill the Homeplug packet data */
	cmskr->homeplug.MMV = HOMEPLUG_MMV; /* the homeplug version */
	cmskr->homeplug.MMTYPE = HTOLE16(CM_SET_KEY | MMTYPE_REQ);
	//cmskr->homeplug.MMTYPE = HTOLE16(CM_GET_KEY | MMTYPE_REQ);
	cmskr->homeplug.FMSN = 0;
	cmskr->homeplug.FMID = 0;
	cmskr->KEYTYPE = SLAC_CM_SETKEY_KEYTYPE;
	cmskr->MYNOUNCE = 0;
	cmskr->YOURNOUNCE = 0;
	cmskr->PID = 4; /* Laut ISO15118-3 fest auf 4, "HLE protocol" */
	cmskr->PRN = 0;
	cmskr->PMN = 0;
	cmskr->CCOCAP = 0; /* Welche CCo-Capability wäre
						 richtig? Das wireshark interpretiert 00 als
					    "station", das passt. */
					    
	/* Woher die Netzwerk-ID nehmen? 
	   Antwort: Laut ISO aus der CM_SLAC_MATCH.CNF.NID */
	memcpy (cmskr->NID, myNID, sizeof (cmskr->NID));	
	cmskr->NEWEKS = SLAC_CM_SETKEY_EKS; /* 1 according to ISO */
	memcpy (cmskr->NEWKEY, myNMK, sizeof (cmskr->NEWKEY));	
	
	/* The message length */
	tx_len = sizeof(struct cm_set_key_request);
	
	/* Send packet */
	if (sendto(sock_fd_tx, transmitbuffer, tx_len, 0, (struct sockaddr*)&socket_address_tx, sizeof(struct sockaddr_ll)) < 0) {
	    perror("sendto failed");
	}
	nSetKey++;	
}


void sendGetKeyRequest(void) {
	printToLogAndScreen("sending GetKeyRequest");
	struct ethhdr *eh = (struct ethhdr *) transmitbuffer;
    struct cm_get_key_request *gkr = (struct cm_get_key_request *) transmitbuffer;
    int tx_len;
    
    /* Construct the Ethernet header */
	memset(transmitbuffer, 0, TRANSMIT_BUFFER_SIZE);
	/* Ethernet header */
	eh->h_source[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
	eh->h_source[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
	eh->h_source[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
	eh->h_source[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
	eh->h_source[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
	eh->h_source[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];	
	eh->h_dest[0] = MY_DEST_MAC0;
	eh->h_dest[1] = MY_DEST_MAC1;
	eh->h_dest[2] = MY_DEST_MAC2;
	eh->h_dest[3] = MY_DEST_MAC3;
	eh->h_dest[4] = MY_DEST_MAC4;
	eh->h_dest[5] = MY_DEST_MAC5;

	/* Ethernet protocol type */
	eh->h_proto = htons(ETH_P_HPAV); /* Homeplug protocol 0x88e1 */

	/* Fill the Homeplug packet data */
	gkr->homeplug.MMV = HOMEPLUG_MMV; /* the homeplug version */
	gkr->homeplug.MMTYPE = HTOLE16(CM_GET_KEY | MMTYPE_REQ);
	gkr->homeplug.FMSN = 0;
	gkr->homeplug.FMID = 0;
	gkr->RequestType = 0; /* 0= direct */
	gkr->RequestedKeyType = HOMEPLUG_KEYTYPE_NMK; /* only "NMK" is permitted over the H1 interface */
	gkr->MYNOUNCE = 0;
	gkr->PID = 4; /* Laut ISO15118-3 fest auf 4, "HLE protocol" */
	gkr->PRN = 0;
	gkr->PMN = 0;
					    
	/* Woher die Netzwerk-ID nehmen? 
	   Antwort: Laut ISO aus der CM_SLAC_MATCH.CNF.NID */
	memcpy (gkr->NID, myNID, sizeof (gkr->NID));	
	
	/* The message length */
	tx_len = sizeof(struct cm_get_key_request);
	
	/* Send packet */
	if (sendto(sock_fd_tx, transmitbuffer, tx_len, 0, (struct sockaddr*)&socket_address_tx, sizeof(struct sockaddr_ll)) < 0) {
	    perror("sendto failed");
	}
}



void printTheNMK(void) {
 int i;
 char sLong[1000];
  sprintf(sLong, "NMK=");
  for (i=0; i<SLAC_NMK_LEN; i++) {
	sprintf(strTmp, "%02x ", myNMK[i]);
	strcat(sLong, strTmp);
  }
  printToLogAndScreen(sLong);
}

void extractNmkFromMatchResponse(void) {
	struct cm_slac_match_confirm *matchconfirm = (struct cm_slac_match_confirm *) receivebuffer;
	sprintf(str1000, "Extracting the NMK from slac_match of EV %2x:%2x:%2x:%2x:%2x:%2x and EVSE %2x:%2x:%2x:%2x:%2x:%2x",
	   matchconfirm->MatchVarField.PEV_MAC[0],
	   matchconfirm->MatchVarField.PEV_MAC[1],
	   matchconfirm->MatchVarField.PEV_MAC[2],
	   matchconfirm->MatchVarField.PEV_MAC[3],
	   matchconfirm->MatchVarField.PEV_MAC[4],
	   matchconfirm->MatchVarField.PEV_MAC[5],
	   matchconfirm->MatchVarField.EVSE_MAC[0],
	   matchconfirm->MatchVarField.EVSE_MAC[1],
	   matchconfirm->MatchVarField.EVSE_MAC[2],
	   matchconfirm->MatchVarField.EVSE_MAC[3],
	   matchconfirm->MatchVarField.EVSE_MAC[4],
	   matchconfirm->MatchVarField.EVSE_MAC[5]);
	memcpy(myNMK, matchconfirm->MatchVarField.NMK , SLAC_NMK_LEN);
	printToLogAndScreen(str1000);
	printTheNMK();
}

void extractNidFromMatchResponse(void) {
	struct cm_slac_match_confirm *matchconfirm = (struct cm_slac_match_confirm *) receivebuffer;
	printToLogAndScreen("Extracting the NID");
	memcpy(myNID, matchconfirm->MatchVarField.NID , SLAC_NID_LEN);
}

void decodeCM_SET_KEY__CNF(void) {
	struct cm_set_key_confirm *skc = (struct cm_set_key_confirm *) receivebuffer;
	uint8_t result = skc->RESULT;
	if (result == 0) {
		sprintf(strTmp, "RESULT ok");
	} else {
		sprintf(strTmp, "RESULT FAIL %d", result);
	}
	sprintf(str1000, "Decoding CM_SET_KEY__CNF %s", strTmp);
	printToLogAndScreen(str1000);	
}

void decodeCM_GET_KEY__CNF(void) {
	struct cm_get_key_confirm *gkc = (struct cm_get_key_confirm *) receivebuffer;
	uint8_t result = gkc->RESULT;
	if (result == 0) {
		sprintf(strTmp, "RESULT ok");
	} else {
		sprintf(strTmp, "RESULT FAIL %d", result);
	}
	sprintf(str1000, "Decoding CM_GET_KEY__CNF %s", strTmp);
	printToLogAndScreen(str1000);	
}

void processHomeplugFrame(void) {
	struct homeplug_hdr *hph = (struct homeplug_hdr*)(receivebuffer+sizeof(struct ethhdr));
	uint16_t mmtype = hph->MMTYPE;
	uint8_t mmSubType = mmtype & 3;/* lower two bits defining the REQ/CNF/IND/RSP */
	char strSubType[10];
	char strMainType[50];
	switch (mmSubType) {
		case MMTYPE_REQ:
			sprintf(strSubType, "REQ");
			break;
		case MMTYPE_CNF:
			sprintf(strSubType, "CNF");
			break;
		case MMTYPE_IND:
			sprintf(strSubType, "IND");
			break;
		case MMTYPE_RSP:
			sprintf(strSubType, "RSP");
			break;
		default:
			sprintf(strSubType, "???");
	}
	switch (mmtype & 0xfffc) { /* upper 14 bits */
	  case CM_SLAC_PARAM:
	    sprintf(strMainType, "CM_SLAC_PARAM");
	    break;
	  case CM_MNBC_SOUND:
	    sprintf(strMainType, "CM_MNBC_SOUND");
	    break;
	  case CM_START_ATTEN_CHAR:
	    sprintf(strMainType, "CM_START_ATTEN_CHAR");
	    break;	    
	  case CM_ATTEN_CHAR:
	    sprintf(strMainType, "CM_ATTEN_CHAR");
	    break;
	  case CM_SLAC_MATCH:
	    sprintf(strMainType, "CM_SLAC_MATCH");
	    break;
	  case CM_GET_DEVICE_SW_VERSION:
	    sprintf(strMainType, "CM_GET_DEVICE_SW_VERSION");
	    nHpGetSwVersion++;
	    break;
	  case CM_SET_KEY:
	    sprintf(strMainType, "CM_SET_KEY");
	    break;
	  case CM_GET_KEY:
	    sprintf(strMainType, "CM_GET_KEY");
	    break;
	  default:
	    sprintf(strMainType, "MMTYPE %4x\n", mmtype);
	}
	sprintf(str1000, "processing Homeplug frame %s.%s", strMainType, strSubType);
	printToLogAndScreen(str1000);
	switch (mmtype) { /* For reaction, we need to check the full 16 bit mmtype */    
	  case CM_SLAC_MATCH + MMTYPE_CNF:
	     /* This is the interesting point: Take the NID and NMK from SLAC_MATCH confirmation message,
	      * and create a SET_KEY with this NID and NMK. */
	    nHpSlacMatchCnf++;
	    extractNmkFromMatchResponse();
	    extractNidFromMatchResponse();
	    sendSetKeyRequest();
	    break;
	  case CM_SET_KEY + MMTYPE_CNF:
	    //printToLogAndScreen("Received CM_SET_KEY confirmation");
	    decodeCM_SET_KEY__CNF();
	    break;
	  case CM_GET_KEY + MMTYPE_CNF:
	    //printToLogAndScreen("Received CM_GET_KEY confirmation");
	    decodeCM_GET_KEY__CNF();
	    break;
	}	
	  
}


void data_process(int buflen) {
	struct ethhdr *ethernetheader = (struct ethhdr*)(receivebuffer);
	total++;
	switch (ntohs(ethernetheader->h_proto))
	{
		case ETH_P_HPAV: /* it is a Homeplug ethernet frame */
			nHomePlug++;
			//printf("h_proto= Homeplug\n");
			processHomeplugFrame();
			break;
		case ETH_P_IP:
			printf("IP");
			break;
		default:
			++other;
			printf("Other, h_proto=0x%4x\n", ethernetheader->h_proto);
	}
}

int initializeTheSockets(void) {
	//struct ifreq ifr;
	struct sockaddr_ll sll;
	
	/* open a raw socket for reception*/
	sock_fd_rx=socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)); 
	if(sock_fd_rx<0) {
		perror("could not open the socket for reception");
		printf("Try to run as root, sudo ./listen_to_eth\n");
		return -1;
	}
	/* open a raw socket for transmission */
	sock_fd_tx=socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)); 
	if(sock_fd_tx<0) {
		perror("could not open the socket for transmission");
		printf("Try to run as root, sudo ./listen_to_eth\n");
		return -1;
	}
	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sock_fd_tx, SIOCGIFINDEX, &if_idx) < 0) {
	    perror("SIOCGIFINDEX");
	    return -1;
	}
	//printf("iface index is %d\n", if_idx.ifr_ifindex);
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sock_fd_tx, SIOCGIFHWADDR, &if_mac) < 0) {
	    perror("SIOCGIFHWADDR");
	    return -1;
	}
	
	/* bind the receive socket to eth0, otherwise it receives data
	 * from all network interfaces.
	 https://stackoverflow.com/questions/21660868/unable-to-bind-raw-socket-to-interface
	 * setsockopt(sock_fd_rx, SOL_SOCKET, SO_BINDTODEVICE ... does not work
	 * for raw sockets. Instead, use bind(). */
	bzero(&sll , sizeof(sll));
	sll.sll_family = AF_PACKET; 
	sll.sll_ifindex = if_idx.ifr_ifindex;
	sll.sll_protocol = htons(ETH_P_ALL);	
	if((bind(sock_fd_rx, (struct sockaddr *)&sll , sizeof(sll))) ==-1) {
		perror("bind: ");
		return -1;
	} 
	printf("binding done of %s which is index %d\n",  ifName, if_idx.ifr_ifindex);
     
	/* Construct the address information for later use in the transmit function */
	/* Index of the network device */
	socket_address_tx.sll_ifindex = if_idx.ifr_ifindex;
	/* Address length*/
	socket_address_tx.sll_halen = ETH_ALEN;
	/* Destination MAC */
	socket_address_tx.sll_addr[0] = MY_DEST_MAC0;
	socket_address_tx.sll_addr[1] = MY_DEST_MAC1;
	socket_address_tx.sll_addr[2] = MY_DEST_MAC2;
	socket_address_tx.sll_addr[3] = MY_DEST_MAC3;
	socket_address_tx.sll_addr[4] = MY_DEST_MAC4;
	socket_address_tx.sll_addr[5] = MY_DEST_MAC5;	
	
	/* descriptor for the poll() for polling the receive socket */
	mypollfd.fd = sock_fd_rx;
	mypollfd.events = POLLIN;
	mypollfd.revents = 0;
	
	return 0; /* success */
}


void processTheKey(unsigned char c) {
	switch (c) {
		case 'x':
		case 27: /* ESC */
		case 3: /* Ctrl C */
			blExit=1;
			break;
		case 's':
			sendSetKeyRequest();
			break;
		case 'g':
			sendGetKeyRequest();
			break;
	}
}

int main() {
  unsigned char c=0;
  int saddr_len,buflen;

    memset(receivebuffer,0,RECEIVE_BUFFER_SIZE);
	hLogFile=fopen("log.txt","a"); /* open for appending */
	if(!hLogFile) {
		printf("unable to open log.txt\n");
		printf("Try to run as root, sudo ./listen_to_eth\n");
		return -1;
	}
	printToLogAndScreen("starting. Press x to exit.");
	//printTheNMK();
	
	if (initializeTheSockets()<0) {
		printToLogAndScreen("init sockets failed. Stopping.");
		return -1;
	}

	set_conio_terminal_mode(); /* to react on each key press */
	printf("entering main loop\n");
    while (!blExit) {

		nMainLoops++;
		/*----- Polling and processing of the ethernet frames -----*/
		signed status = poll (&mypollfd, 1, 1); /* one file descriptor, one millisecond timeout */
		if ((status < 0) && (errno != EINTR)) {
			printf("can't poll, %d", errno);
			return (-1);
		}
		if (status > 0) {
			nPollSuccess++;
			saddr_len=sizeof socket_address_rx;
			buflen=recvfrom(sock_fd_rx,receivebuffer,RECEIVE_BUFFER_SIZE,0,&socket_address_rx,(socklen_t *)&saddr_len);
			//printf("poll success status %d, len %d\n", status, buflen);
			if(buflen<0) {
				printf("error in reading recvfrom function\n");
				return -1;
			}
			data_process(buflen);
		} else {
			nPollNothing++; 
		}	
		/*----- Status reporting from time to time -----*/
		if ((nMainLoops %10000)==0) {
			sprintf(str1000, "mainloops %5d, nPollSuccess %5d, nHomePlug: %5d,  Other: %5d  Total: %5d  SlacMatchCnf: %5d  GetSwVersion: %5d  SetKey: %5d",
			nMainLoops, nPollSuccess, nHomePlug, other,total, nHpSlacMatchCnf, nHpGetSwVersion, nSetKey);
	        printToLogAndScreen(str1000);
		}
		/*----- Polling and processing of keyboard -----*/
	    if (kbhit()) {
			/* A key was pressed */
		    c=getch();
		    sprintf(str1000, "Taste gedrückt: %02x %c", c, c);
		    printToLogAndScreen(str1000);
		    processTheKey(c);
 	    }
	}

	close(sock_fd_rx);
	close(sock_fd_tx);
	printToLogAndScreen("Terminating normally.");
	fclose(hLogFile);

}

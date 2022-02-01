

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


FILE* log_txt;
int total,nHomePlug,icmp,igmp,other,iphdrlen;
int nPollSuccess, nPollNothing, nMainLoops;
int nHpSlacMatchCnf, nHpGetSwVersion, nSetKey;

struct sockaddr saddr;
struct sockaddr_in source,dest;
int sock_fd; /* the socket file descriptor */
char ifName[IFNAMSIZ] = "eth0";
struct ifreq if_idx; /* index of the interface */
struct ifreq if_mac; /* MAC adress of the interface */

#define RECEIVE_BUFFER_SIZE 65536
unsigned char receivebuffer[RECEIVE_BUFFER_SIZE];
#define TRANSMIT_BUFFER_SIZE 65536
unsigned char transmitbuffer[TRANSMIT_BUFFER_SIZE];
char myNMK[SLAC_NMK_LEN] = "hallo";
char myNID[SLAC_NID_LEN] = "1234567";

void sendSetKeyRequest(void) {
	printf("sending SetKeyRequest\n");
	//struct ifreq if_mac; /* for retrieving the physical MAC address */
	struct ethhdr *eh = (struct ethhdr *) transmitbuffer;
    struct cm_set_key_request *cmskr = (struct cm_set_key_request *) transmitbuffer;
    struct sockaddr_ll socket_address;
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
	
	/* Construct the address information */
	/* Index of the network device */
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	/* Address length*/
	socket_address.sll_halen = ETH_ALEN;
	/* Destination MAC */
	socket_address.sll_addr[0] = MY_DEST_MAC0;
	socket_address.sll_addr[1] = MY_DEST_MAC1;
	socket_address.sll_addr[2] = MY_DEST_MAC2;
	socket_address.sll_addr[3] = MY_DEST_MAC3;
	socket_address.sll_addr[4] = MY_DEST_MAC4;
	socket_address.sll_addr[5] = MY_DEST_MAC5;	
	/* Send packet */
	if (sendto(sock_fd, transmitbuffer, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0) {
	    //printf("Send failed, errno %d\n", errno);
	    perror("sendto failed");
	}
	nSetKey++;	
}


void printTheNMK(void) {
 int i;
 char sLong[1000];
 char sTmp[10];
  sprintf(sLong, "NMK=");
  for (i=0; i<SLAC_NMK_LEN; i++) {
	sprintf(sTmp, "%02x ", myNMK[i]);
	strcat(sLong, sTmp);
  }
  printf("%s\n", sLong);
}

void extractNmkFromMatchResponse(void) {
	struct cm_slac_match_confirm *matchconfirm = (struct cm_slac_match_confirm *) receivebuffer;
	printf("Extracting the NMK from slac_match of EV %2x:%2x:%2x:%2x:%2x:%2x and EVSE %2x:%2x:%2x:%2x:%2x:%2x\n",
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
	printTheNMK();
}

void extractNidFromMatchResponse(void) {
	struct cm_slac_match_confirm *matchconfirm = (struct cm_slac_match_confirm *) receivebuffer;
	printf("Extracting the NID\n");
	memcpy(myNID, matchconfirm->MatchVarField.NID , SLAC_NID_LEN);
}

void processHomeplugFrame(void) {
	printf("processing Homeplug frame ");
	struct homeplug_hdr *hph = (struct homeplug_hdr*)(receivebuffer+sizeof(struct ethhdr));
	uint16_t mmtype = hph->MMTYPE;
	switch (mmtype) { /* lower two bits defining the REQ/CNF/IND/RSP */
	  case CM_SLAC_PARAM + MMTYPE_REQ:
	    printf("CM_SLAC_PARAM.REQ\n");
	    break;
	  case CM_SLAC_PARAM + MMTYPE_CNF:
	    printf("CM_SLAC_PARAM.CNF\n");
	    break;
	  case CM_MNBC_SOUND + MMTYPE_IND:
	    printf("CM_MNBC_SOUND.IND\n");
	    break;
	  case CM_START_ATTEN_CHAR + MMTYPE_IND:
	    printf("CM_START_ATTEN_CHAR.IND\n");
	    break;	    
	  case CM_ATTEN_CHAR + MMTYPE_IND:
	    printf("CM_ATTEN_CHAR.IND\n");
	    break;
	  case CM_ATTEN_CHAR + MMTYPE_RSP:
	    printf("CM_ATTEN_CHAR.RSP\n");
	    break;
	  case CM_SLAC_MATCH + MMTYPE_REQ:
	    printf("CM_SLAC_MATCH.REQ\n");
	    break;
	  
	  case CM_GET_DEVICE_SW_VERSION + MMTYPE_REQ:
	    printf("CM_GET_DEVICE_SW_VERSION.REQ\n");
	    nHpGetSwVersion++;
	    sendSetKeyRequest(); /* just for testing */
	    break;
	  case CM_SLAC_MATCH + MMTYPE_CNF:
	     /* This is the interesting point: Take the NID and NMK from SLAC_MATCH confirmation message,
	      * and create a SET_KEY with this NID and NMK. */
	    printf("CM_SLAC_MATCH.CNF\n");
	    nHpSlacMatchCnf++;
	    extractNmkFromMatchResponse();
	    extractNidFromMatchResponse();
	    sendSetKeyRequest();
	    break;
	  case CM_SET_KEY + MMTYPE_CNF:
	    printf("CM_SET_KEY.CNF\n");
	    break;
	  default:
		printf("MMTYPE = %4x\n", mmtype);
	}	
	  
}


void data_process(int buflen)
{
	struct ethhdr *ethernetheader = (struct ethhdr*)(receivebuffer);
	total++;
	switch (ntohs(ethernetheader->h_proto))
	{
		case ETH_P_HPAV: /* it is a Homeplug ethernet frame */
			nHomePlug++;
			//printf("h_proto= Homeplug\n");
			processHomeplugFrame();
			break;

		default:
			++other;
			//printf("h_proto=%4x\n", ethernetheader->h_proto);
	}
}

int initializeTheSocket(void) {
	/* open a raw socket */
	sock_fd=socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)); 
	if(sock_fd<0)
	{
		perror("could not open the socket");
		printf("Try to run as root, sudo ./listen_to_eth\n");
		return -1;
	}
	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sock_fd, SIOCGIFINDEX, &if_idx) < 0) {
	    perror("SIOCGIFINDEX");
	    return -1;
	}
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sock_fd, SIOCGIFHWADDR, &if_mac) < 0) {
	    perror("SIOCGIFHWADDR");
	    return -1;
	}
	return 0; /* success */
}

int main()
{

	int saddr_len,buflen;

	memset(receivebuffer,0,RECEIVE_BUFFER_SIZE);

	log_txt=fopen("log.txt","w");
	if(!log_txt)
	{
		printf("unable to open log.txt\n");
		return -1;

	}
	printTheNMK();
	printf("starting .... \n");
	if (initializeTheSocket()<0) {
		return -1;
	}

	while(1)
	{
		nMainLoops++;
		struct pollfd pollfd =
	    {
			sock_fd,
			POLLIN,
			0
		};
		signed status = poll (&pollfd, 1, 1); /* one file descriptor, one millisecond timeout */
		if ((status < 0) && (errno != EINTR))
		{
			printf("can't poll, %d", errno);
			return (-1);
		}
		if (status > 0)
		{
			nPollSuccess++;
			saddr_len=sizeof saddr;
			buflen=recvfrom(sock_fd,receivebuffer,RECEIVE_BUFFER_SIZE,0,&saddr,(socklen_t *)&saddr_len);

			if(buflen<0)
			{
				printf("error in reading recvfrom function\n");
				return -1;
			}
			//fflush(log_txt);
			data_process(buflen);
		} else {
			nPollNothing++; 
		}	
		if ((nMainLoops %10000)==0) {
			printf("mainloops %5d, nPollSuccess %5d, nHomePlug: %5d,  Other: %5d  Total: %5d  SlacMatchCnf: %5d  GetSwVersion: %5d  SetKey: %5d\n",
			nMainLoops, nPollSuccess, nHomePlug, other,total, nHpSlacMatchCnf, nHpGetSwVersion, nSetKey);
		}

	}

	close(sock_fd);// use signals to close socket 
	printf("DONE!!!!\n");

}

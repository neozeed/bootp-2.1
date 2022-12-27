/*	defs.h	1.1	01/08/86	*/


/*
 * typedefs
 */
typedef	long		iaddr_t;	/* internet address type */
typedef	struct ether_addr eaddr_t;	/* ether address type */
typedef	char *		caddr_t;	/* "core" address */
typedef	unsigned char	u_char;		/* unsigned types */
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;
typedef	unsigned short	ushort;		/* sys III compat */
typedef unsigned short	n_short;
typedef unsigned long	n_time;

#if !defined(lowendian)
/*
 * Macros for number representation conversion.
 */
#define	ntohl(x)	(x)
#define	ntohs(x)	(x)
#define	htonl(x)	(x)
#define	htons(x)	(x)
#endif


#define isdigit(c) (c >= '0' && c <= '9')
#define	isspace(c) (c == ' ')
#define	isxdigit(c) (isdigit(c) || (c >= 'a' && c <= 'f'))
#define	islower(c) (c >= 'a' && c <= 'z')


#include "../h/globram.h"
#include "../tftp/inet.h"
#include "../tftp/tftp.h"
#include "../tftp/bootp.h"
#include "../tftp/ether.h"


/*
 * Interface functions, one set per hardware type.
 */
struct iffun {
	int	(*iff_init)();	/* initialize interface */
	int	(*iff_read)();	/* read from interface */
	int	(*iff_write)();
	char	iff_name[32];	/* printable name of interface */
};


/*
 * Interface configuration, one per possible board.
 */
struct ifconfig {
	struct iffun *ifc_fun;	/* associated functions */
	int	ifc_addr;	/* hardware address */
	int	ifc_addr2;	/* another hardware address, if needed */
};


/*
 * Bootstrap globals live at the end of memory, and can be referenced
 * through system global 'bglobptr' (defined below), which
 * points to this structure.
 */
struct bglob {
	/*
	 * the first few fields would be good candidates 
	 * to copy in from EAPROM.
	 */
	iaddr_t	myiaddr;	/* my internet address */
	iaddr_t	gateiaddr;	/* gateway ip addr */
	iaddr_t	serveiaddr;	/* server ip addr */
	eaddr_t	myeaddr;	/* my ethernet address */
	int	loadat;		/* load address */
	struct	ifconfig *ifptr;/* current interface */
	u_char	file[64];	/* file name to boot */
	u_char	sname[64];	/* server name */
				/* end EAPROM area */
	/*
	 * next fields allow a program just loaded to re-call PROM
	 * bootp or tftp, e.g. to load an ethertip configuration file.
	 */
	struct ifconfig *ifconfigp; /* points to PROM ifconfig */
	int	(*bootp)();	/* bootp entry point */
	int	(*tftpread)();	/* tftp read entry point */
	int	(*tftpwrite)();	/* tftp write (dump) entry */

	iaddr_t	arpiaddr;	/* arp cache: ip addr */
	eaddr_t	arpeaddr;	/* arp cache: ether addr */
	u_char	arpvalid;	/* arp cache: valid flag */
	u_char	arpwait;	/* arp cache: ip packet waiting to go out */
	u_char	pupsw;		/* -[p]up switch */
	u_char	dumpsw;		/* -[d]ump switch */
	u_char	helpsw;		/* -[h]elp switch */
	u_char	xxxxsw;
	int	loadsize;	/* size of load image */
	int	xid;		/* transaction id */
	int	rand;		/* random number (for backoff) */
	int	mask;		/* backoff mask */
	int	timer;		/* receive timer (decremented by driver) */
	short	vaxpad;		/* (for vax structure alignment) */
	struct ether_header eh;	/* ether header (contiguous with below) */
	struct ip ih;		/* ip header */
	struct udphdr uh;	/* udp header */
	u_char	wbuf[256];	/* udp data (write buffer) */
	u_char	rbuf[SEGSIZE+128]; /* read buffer */
};

struct rbufhdr {		/* can overlay rbuf */
	short  vaxpad;		/* for vax structure alignment */
	struct ether_header eh;
	struct ip ih;
	struct udphdr uh;
	struct tftphdr th;
};

#define	bglobptr	( (struct bglob *) (GlobPtr->MemorySize - 2048) )
#define	rbufhdrsize	( sizeof (struct rbufhdr) - 2 )
#define	ifp	g->ifptr	/* interface pointer */
#define	MASK1SEC	0x3FF	/* backoff mask (1023) of about
				   one second worth of ms. clock ticks */
#define	MASKMAX		(((MASK1SEC+1)*16)-1)	/* max mask (16 seconds)*/
#define	MASKMIN		(((MASK1SEC+1)*2)-1)	/* min mask (2 seconds)*/
#define	MSCLOCK		(GlobPtr->RefrCnt)	/* millisecond clock */

/*	boott.c	1.1	12/17/85	*/

/*
 * BOOTP/TFTP prom bootstrap.
 *
 * history
 * 12/17/85	croft	created.
 */


#include "../tftp/defs.h"


/*
 * Interface functions and configuration.  This allows the bootstrap
 * to find all connected interfaces.
 */
extern ecinit(), ecread(), ecwrite();
extern ilinit(), ilread(), ilwrite();

struct iffun ecfun = { ecinit, ecread, ecwrite, "3com 3c400" };
struct iffun ilfun = { ilinit, ilread, ilwrite, "Interlan NI3210" };

struct ifconfig ifconfig[] = {
	&ecfun, 0x1f2000, 0,
	&ecfun, 0x1f4000, 0,
	&ecfun, 0x1f6000, 0,
	&ecfun, 0x1f8000, 0,
	&ilfun, 0x1f0500, 0x142000,
	&ilfun, 0x1f0510, 0x144000,
	&ilfun, 0x1f0520, 0x146000,
	&ilfun, 0x1f0530, 0x148000,
	0, 0, 0
};

/*
 */

struct ether_addr ether_broadcast = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };



/*
 * TFTP bootstrap.
 *
 * Called from ROM monitor with the string argument set to the command line.
 * The string can take the forms "filename" or "host:filename" or
 * "host:" or "?" or NULL.  "?" causes the program to prompt for 
 * information.  A NULL string has the same effect as a power-up boot.
 *
 * The command line string can also contain switches preceeding the
 * filename;  these are one or more letters, prefixed by a '-'.
 *
 * Since this code runs out of ROM, no formal globals can be used;  instead
 * a structure at the end of memory 'bglob' is setup for this purpose.  The 
 * macro 'bglobptr' can be used by subroutines to find this area.
 */
tftpboot(s)
	register char *s;
{
	register struct bglob *g;
	register char *cp;
	iaddr_t ia;
	char line[128];
	char promptflag = 0;
	extern int loadat, bcopy();
	int bootp(), tftpread();

	/* setup global area */

	if ((int)bglobptr + sizeof *g > GlobPtr->MemorySize) {
		printf("oops");
		return (0);
	}
	g = bglobptr;
	bzero((caddr_t)g, sizeof *g);
	g->loadat = loadat;	/* user code normally starts at 0x1000 */
	g->bootp = bootp;
	g->tftpread = tftpread;
	g->ifconfigp = ifconfig;

	printf("IP/BOOTP/TFTP Bootstrap.  (3/13/86, Stanford SUMEX)\n");
	while (*s == ' ') s++;
	while (*s == '-') {
		s++;
		switch (*s) {
		case 'p':	/* PUP boot */
			g->pupsw++;
			break;

		case 'h':	/* 'help', I'm not registered */
			g->helpsw++;
			break;

		case 'i':	/* select particular interface */
			s++;
			if (isdigit(*s)) {
				ifp = &ifconfig[*s&7];
				if (ifinit(ifp) == 0)
					return (0);
			} else {
				printif();  /* print available interfaces */
				return (0);
			}
			break;

		case 'd':	/* memory dump (TFTP WRQ) */
			g->dumpsw++;
			break;

		default:
			printf("valid switches: -pUP -hELP -dUMP -iNTERFACE\n");
			return (0);
		}
		while (*s && *s != ' ') s++;
		while (*s == ' ') s++;
	}

	if (ifp == 0) {
#ifdef ENETBOOT		/* 3 megabit ethernet */
		if (g->pupsw || iflocate() == 0) {
			extern enbootload();
			printf("Trying PUP 3mb.\n");
			return(enbootload(s));
		}
#else
		if (iflocate() == 0) {
			printf("No ethernet interfaces.\n");
			goto abort;
		}
#endif ENETBOOT
	}

	/*
	 * Check the command line for arguments or prompt for them.
	 */
	if (*s == '?') {	/* prompt for args */
		promptflag++;
		printf("A 'null reply' uses the default shown in brackets [].\n");
		getiaddr("Server address (or name) [broadcast] >", 
			&g->serveiaddr, g->sname);
		g->gateiaddr = g->serveiaddr;
		getiaddr("Gateway address [server] >", &g->gateiaddr, 0);
		getiaddr("My address [set by BOOTP server] >", &g->myiaddr, 0);
		printf("File name to load [determined by BOOTP server] >");
		gets(line);
		if (line[0])
			strcpy(g->file, line);
		printf("Load address [0x1000] >");
		gets(line);
		if ((ia = inet_addr(line)) >= 0x1000)
			g->loadat = ia;
	} else if (*s) {	/* parse command line */
		for (cp = s ; *cp ; cp++)
			if (*cp == ':')
				break;
		if (*cp) {	/* if hostname present */
			*cp = 0;
			cp++;
			strcpy(g->file, cp);
			if ((ia = inet_addr(s)) != -1)	/* was number */
				g->serveiaddr = ia;
			else
				strcpy(g->sname, s);
		} else {
			strcpy(g->file, s);
		}
	}
retry:
	/* pick a random transaction ID based on etheraddr & ms clock */
	g->xid = *(int *)&g->myeaddr.ether_addr_octet[2] ^ MSCLOCK;
	g->rand = g->xid;	/* also use as random seed */

	/*
	 * If we know the filename to boot, the server IP address,
	 * and my own IP address, then we can skip the BOOTP phase.
	 */
	if (g->file[0] == 0 || g->serveiaddr == 0 || g->myiaddr == 0)
		if (bootp() == 0)
			goto abort;	/* bootp failed */

	/*
	 * Do the TFTP, then call 'setup' to process the b.out header.
	 */
	if (tftpread(bcopy) == 0) {	/* if it fails, try another interface */
		printf("\nTFTP failed, retrying.\n");
		if (iflocate() == 0) {	/* same old interface */
			int oldclock;
			printf("Delaying 60 seconds.\n");
			oldclock = MSCLOCK; 
			while ((MSCLOCK - oldclock) < 60000);
		}
		goto retry;
	}
	printf("\n[Done]\n");
	return (setup(g->loadat, g->loadsize));
abort:
	return (0);
}


/*
 * BOOTP protocol.
 */
bootp()
{
	register struct bglob *g = bglobptr;
	register struct bootp *bh = (struct bootp *)g->wbuf;
	register struct bootp *rbh;
	int count, firstclock;
	int trys = 0;
	iaddr_t ia;

	/*
	 * setup the UDP and BOOTP headers.
	 */
	printf("Sending BOOTPs: ");
	g->uh.uh_sport = htons(IPPORT_BOOTPC);
	g->uh.uh_dport = htons(IPPORT_BOOTPS);
	g->uh.uh_ulen = htons(sizeof g->uh + sizeof (struct bootp));
	g->uh.uh_sum = 0;
	bzero(bh, sizeof *bh);
	bh->bp_op = BOOTREQUEST;
	bh->bp_htype = ARPHRD_ETHER;
	bh->bp_hlen = sizeof (struct ether_addr);
	bh->bp_xid = g->xid;
	bh->bp_ciaddr = g->myiaddr;	/* if known */
	bcopy((caddr_t)&g->myeaddr, bh->bp_chaddr, 6);
	strcpy(bh->bp_sname, g->sname);
	strcpy(bh->bp_file, g->file);
	if (g->helpsw) {
		register struct vend *vh = (struct vend *)bh->bp_vend;
		bcopy(VM_STANFORD, vh->v_magic, 4);
		vh->v_flags = htonl(VF_HELP);
	}

	ia = (g->serveiaddr ? g->serveiaddr : INADDR_BROADCAST);
	settimer();
	sendip(ia,ia,sizeof g->ih + sizeof g->uh + sizeof *bh);
	firstclock = MSCLOCK;
	/*
	 * Receive replies until we get a good one.
	 */
	for (;;) {
		if ((count = receive()) == 0) {	/* if timeout */
			if ((++trys & 7) == 0 && iflocate()) {
				/* time to try another interface */
				firstclock = MSCLOCK;
				bcopy((caddr_t)&g->myeaddr, bh->bp_chaddr,
					sizeof g->myeaddr);
			}
			bh->bp_secs = (MSCLOCK - firstclock) >> 10;
			settimer();
			sendip(ia,ia,sizeof g->ih + sizeof g->uh + sizeof *bh);
			continue;
		}
		if (count < (sizeof (struct ether_header) + sizeof (struct ip)
		    + sizeof (struct udphdr) + sizeof (struct bootp)))
			continue;
		if (checkudp(sizeof (struct ip) + sizeof (struct udphdr)
		    + sizeof (struct bootp), IPPORT_BOOTPC) == 0)
			continue;
		rbh = (struct bootp *)(g->rbuf + sizeof (struct ether_header)
		    + sizeof (struct ip) + sizeof (struct udphdr));
		if (rbh->bp_xid != g->xid || rbh->bp_op != BOOTREPLY
		    || bcmp(rbh->bp_chaddr, (caddr_t)&g->myeaddr,
		    sizeof g->myeaddr) != 0)
			continue;
		/* it's for us! */
		break;
	}
	g->myiaddr = rbh->bp_yiaddr;
	g->serveiaddr = rbh->bp_siaddr;
	g->gateiaddr = rbh->bp_giaddr;
	if (g->gateiaddr == 0)
		g->gateiaddr = g->serveiaddr;
	strcpy(g->file, rbh->bp_file);
	return(1);
}


/*
 * TFTP read request (RRQ) file g->file from host g->serveiaddr
 * and copy it into RAM at g->loadat.  'copy' argument is usually
 * 'bcopy', but there is a provision for RAM code to call tftpread in PROM.
 * For example, a just-loaded program (ethertip) may want to TFTP
 * read a port configuration file upon startup.  It does this by 
 * setting copy = the address of its own copy routine which will be called
 * after each buffer is received.
 */
tftpread(copy)
	int (*copy)();
{
	register struct bglob *g = bglobptr;
	register struct tftphdr *th = (struct tftphdr *)g->wbuf;
	int mytid, histid;
	int sendcount, recvcount;
	int trys, block;
	register struct rbufhdr *r;
	register u_char *cp;

	/*
	 * setup the UDP and TFTP headers.
	 */
	mytid = (g->xid & 0x3FFF);
	r = (struct rbufhdr *)&g->rbuf[-2]; /* skip short pad */
restart:
	mytid++;
	g->uh.uh_sport = htons(mytid);
	g->uh.uh_dport = htons(IPPORT_TFTP);
	g->uh.uh_sum = 0;
	th->th_opcode = htons(RRQ);
	strcpy(th->th_stuff, g->file);	/* set filename and mode */
	for (cp = (u_char *)th->th_stuff ; *cp ; cp++);
	cp++;
	strcpy(cp, "octet");
	sendcount = (cp - g->wbuf) + 6;
	g->uh.uh_ulen = htons(sendcount + sizeof g->uh);
	printf("\nTFTP read file %s\nMy address: ", g->file);
	printiaddr(g->myiaddr);
	printf("  Server: ");
	printiaddr(g->serveiaddr);
	printf("  Gateway: ");
	printiaddr(g->gateiaddr);
	putchar('\n');
	trys = block = 0;
	cp = (u_char *)g->loadat;
	g->loadsize = 0;
	g->mask = MASKMIN;	/* short initial timeout */
	/*
	 * send "RRQ" or "ACK" until entire file is read.
	 */
	for (;;) {
		if (++trys > 7) {
			if (block == 0)	/* if never started */
				return (0); /* try something else */
			printf("\nServer timeout(?)  Retrying.\n");
			goto restart;	/* maybe server timed out */
		}
		g->timer = 0;
		while (receive());	/* flush any queued packets */
		settimer();	/* start g->timer */
		sendip(g->serveiaddr, g->gateiaddr,
		    sendcount + sizeof g->uh + sizeof g->ih);
		/*
		 * receive until a good reply comes in.
		 */
		for (;;) {
			recvcount = receive();
			if (recvcount == 0)	/* if receive timeout */
				break;
			if (recvcount < rbufhdrsize
			    || checkudp(rbufhdrsize - sizeof r->eh, mytid) == 0
			    || r->ih.ip_src != g->serveiaddr)
				continue;
			if (block && r->uh.uh_sport != histid)
				continue;
			switch (ntohs(r->th.th_opcode)) {
			default:
				continue;

			case ERROR:
				printf("\nTFTP error.  Code=%d, String=%s\n",
				    ntohs(r->th.th_code), r->th.th_msg);
				return (0);

			case DATA:
				break;
				/* fall thru and break out of receive loop */
			}
			break;
		}
		if (recvcount == 0)
			continue;	/* if timeout, resend */
		if (ntohs(r->th.th_block != (block+1)))
			continue;	/* if not expected block, resend */
		putchar('-');
		if (block == 0) {	/* sending 1st ACK */
			sendcount = sizeof th->th_opcode + sizeof th->th_block;
			th->th_opcode = htons(ACK);
			histid = r->uh.uh_sport;
			g->uh.uh_dport = histid;
			g->uh.uh_ulen = htons(sendcount + sizeof g->uh);
		}
		block++;
		th->th_block = htons(block);
		trys = 0;	/* reset trys and timer counts */
		g->mask = MASKMIN;
		if ((recvcount=ntohs(r->uh.uh_ulen)
		    - sizeof r->uh - sizeof r->th) > 0) {
			(*copy)(&r->th.th_data, cp, recvcount);
			cp += recvcount;
			g->loadsize += recvcount;
		}
		if (recvcount < SEGSIZE) {  /* last block, send final ACK */
			sendip(g->serveiaddr, g->gateiaddr,
			    sendcount + sizeof g->uh + sizeof g->ih);
			break;	/* and return */
		}
	}
	return (1);	/* success */
}
		

/*
 * Send the IP packet in bglob.ih.  Perform ARP if necessary.
 * 'dstaddr' is the address to be used in the IP header,
 * 'gateaddr' is the immediate destination address.
 * 'count' is the length of the IP header plus data.
 */
sendip(dstaddr, gateaddr, count)
	iaddr_t dstaddr,gateaddr;
	int count;
{
	register struct bglob *g = bglobptr;

	g->ih.ip_v = IPVERSION;
	g->ih.ip_hl = sizeof (struct ip) >> 2;
	g->ih.ip_len = htons(count);
	g->ih.ip_id++;
	g->ih.ip_ttl = MAXTTL;
	g->ih.ip_p = IPPROTO_UDP;
	g->ih.ip_src = g->myiaddr;
	g->ih.ip_dst = dstaddr;
	g->ih.ip_sum = 0;
	g->ih.ip_sum = in_cksum((caddr_t)&g->ih, sizeof (struct ip));
	g->eh.ether_shost = g->myeaddr;
	g->eh.ether_type = htons(ETHERTYPE_IPTYPE);
	if (gateaddr == INADDR_BROADCAST)
		g->eh.ether_dhost = ether_broadcast;
	else if (gateaddr == g->arpiaddr && g->arpvalid)
		g->eh.ether_dhost = g->arpeaddr;
	else {
		sendarp(ARPOP_REQUEST, &gateaddr, 0);
		printf("(ARPreq)");
		g->arpiaddr = gateaddr;
		g->arpwait = 1;
		return;
	}
	(*ifp->ifc_fun->iff_write)(ifp, (caddr_t)&g->eh,
		count + sizeof g->eh);
	putchar('.');
}


/*
 * Send an ARP packet, 'tpa' is the target protocol address,
 * 'tha' is the target hardware address, 'req' is the
 * request type.
 */
sendarp(req, tpa, tha)
	iaddr_t	*tpa;
	eaddr_t	*tha;
{
	register struct bglob *g = bglobptr;
	struct ap {	/* arp packet */
		short  vaxpad;		/* for vax struct alignment */
		struct ether_header eh;
		struct ether_arp ah;
	} ap;
	register struct ether_arp *ea = &ap.ah;

	ap.eh.ether_shost = g->myeaddr;
	ap.eh.ether_dhost = (tha ? *tha : ether_broadcast);
	ap.eh.ether_type = htons(ETHERTYPE_ARPTYPE);
	bzero((caddr_t)ea, sizeof *ea);
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IPTYPE);
	ea->arp_hln = sizeof (eaddr_t);
	ea->arp_pln = sizeof (iaddr_t);
	ea->arp_op = htons(req);
	arp_sha(ea) = g->myeaddr;
	if (g->myiaddr == 0)
		return;		/* impossible ... */
	arp_spa(ea) = g->myiaddr;
	arp_tpa(ea) = *tpa;
	if (tha)
		arp_tha(ea) = *tha;
	(*ifp->ifc_fun->iff_write)(ifp, (caddr_t)&ap.eh, sizeof ap - 2);
}


/*
 * Receive next packet into g->rbuf.  Device driver 'read' routine
 * counts down delay in g->timer.  If an ARP reply packet is received,
 * we send any pending IP packet, then continue receiving.
 *
 * We finally return if a packet is received or the driver timer
 * goes off.  Return value is receive count or 0 if timeout.
 * Filter out any non-IP packets;  remove any IP options if present.
 */
receive()
{
	register struct bglob *g = bglobptr;
	register struct ether_header *eh;
	register struct ether_arp *ea;
	register struct ip *ip;
	int count;
	int options;

	for (;;) {
		if ((count = (*ifp->ifc_fun->iff_read)(ifp, g->rbuf,
		    sizeof g->rbuf, &g->timer)) == 0)
			return(0);	/* timeout */
		if (count < sizeof *eh)
			continue;
		eh = (struct ether_header *)g->rbuf;
		switch (ntohs(eh->ether_type)) {
		case ETHERTYPE_ARPTYPE:
			if (count < (sizeof *eh + sizeof *ea))
				continue;
			break;	/* break switch and handle ARP */

		case ETHERTYPE_IPTYPE:
			if (count < (sizeof *eh + sizeof *ip))
				continue;
			ip = (struct ip *)&eh[1];
			if (in_cksum((caddr_t)ip, ip->ip_hl<<2) != 0)
				continue;
			if (ip->ip_v != IPVERSION
			    || (ntohs(ip->ip_off) & 0x3FFF))
				continue;
			if ((options = (ip->ip_hl<<2) - sizeof *ip) > 0) {
				/* discard options */
				bcopy((caddr_t)&ip[1] + options,
				 (caddr_t)&ip[1], count-options-sizeof *ip);
				ip->ip_len = htons(ntohs(ip->ip_len)-options);
				count -= options;
			}
			return (count);

		default:
			continue;
		}

		/*
		 * we've received an ARP.
		 */
		ea = (struct ether_arp *)(g->rbuf + sizeof *eh);
		if (ntohs(ea->arp_hrd) != ARPHRD_ETHER
		    || ntohs(ea->arp_pro) != ETHERTYPE_IPTYPE
		    || ea->arp_hln != sizeof (eaddr_t)
		    || ea->arp_pln != sizeof (iaddr_t))
			continue;
		if (ntohs(ea->arp_op) == ARPOP_REPLY) {
			if (g->arpwait == 0
			    || arp_tpa(ea) != g->myiaddr
			    || arp_spa(ea) != g->arpiaddr)
				continue;
			g->arpeaddr = arp_sha(ea);
			g->arpwait = 0;
			g->arpvalid = 1;
			g->eh.ether_dhost = g->arpeaddr;
			(*ifp->ifc_fun->iff_write)(ifp, (caddr_t)&g->eh,
			    ntohs(g->ih.ip_len) + sizeof g->eh);
			putchar('.');
		} else if (ntohs(ea->arp_op) == ARPOP_REQUEST) {
			if (g->myiaddr == 0 || arp_tpa(ea) != g->myiaddr)
				continue;
			sendarp(ARPOP_REPLY, &arp_spa(ea), &arp_sha(ea));
			printf("(ARPrep)");
		}
	}
}


/*
 * Locate an interface to use for booting.  Returns 0 if failure.
 * Picks a new interface in rotating order each time it is called.
 * Returns nonzero (true) if a NEW interface has been found.
 */
iflocate()
{
	register struct bglob *g = bglobptr;
	struct ifconfig *oldifp;
	register int first = 0;
	int i;

	if (ifp == 0) {
		oldifp = ifp = &ifconfig[0];
		first++;
	} else {
		oldifp = ifp;
		ifp++;
		if (ifp->ifc_addr == 0) /* if end of list */
			ifp = &ifconfig[0];
	}
	for (;;) {
		if (ifinit(ifp)) {
			if (first || ifp != oldifp) {
				g->myiaddr = 0;
				g->mask = MASKMIN;
				return(1);
			}
			return(0);
		}
		ifp++;
		if (ifp->ifc_addr == 0) /* if end of list */
			ifp = &ifconfig[0];
		if (ifp == oldifp)
			return(0);	/* tried 'em all */
	}
}


/*
 * Setup the retry timer value (g->timer) as a random number
 * anded by the current backoff mask (g->mask).  Update the mask
 * and random number.
 *
 * Timer value is always >= MASKMIN and <= MASKMAX.
 */
settimer()
{
	register struct bglob *g = bglobptr;
	register int m = g->mask;
	register t;

	/* generate the next random number */
	g->rand = ((g->rand*1103515245 + 12345) & 0x7FFFFFFF);
	if ((t = (g->rand & m)) <= MASKMIN)
		t = MASKMIN;
	if (m < MASKMAX)
		g->mask = ((m<<1) | 1);
	g->timer = t;
}


/*
 * Check an incoming IP/UDP datagram in g->rbuf.
 * Returns 0 if bad.
 */
checkudp(miniplen, dport)
{
	register struct bglob *g = bglobptr;
	register struct rbufhdr *r;

	r = (struct rbufhdr *)&g->rbuf[-2]; /* skip short pad */
	if ( ntohs(r->ih.ip_len) < miniplen
	    || r->ih.ip_p != IPPROTO_UDP)
		return(0);
	if (g->myiaddr && r->ih.ip_dst != g->myiaddr)
		return(0);
	if (r->uh.uh_dport != htons(dport)
	    || ntohs(r->uh.uh_ulen) < (miniplen - sizeof r->ih))
		return(0);
	return(1);
}


/*
 * Print an internet address in dot form.
 */
printiaddr(ia)
	iaddr_t ia;
{
	ia = ntohl(ia);
	printf("%d.%d.%d.%d", (ia>>24)&0xFF, (ia>>16)&0xFF,
		(ia>>8)&0xFF, ia&0xFF);
}


/*
 * Print available interfaces.
 */
printif()
{
	register struct ifconfig *ifc;
	u_char eaddr[8];
	register i;
	register n;

	printf("Available 10MB interfaces:\n");
	i = 0;
	for (ifc = &ifconfig[0] ; ifc->ifc_addr ; ifc++) {
		printf("%d:\t", i++);
		ifinit(ifc);
	}
}


/*
 * Initialize interface at 'ifc'.  Returns true if 
 * initialization completed successfully.
 */
ifinit(ifc)
	register struct ifconfig *ifc;
{
	register struct bglob *g = bglobptr;
	u_char eaddr[8];
	register n;

	printf("%s at (%x,%x)", ifc->ifc_fun->iff_name,
		ifc->ifc_addr, ifc->ifc_addr2);
	if ((*ifc->ifc_fun->iff_init)(ifc, eaddr)) {
		printf(" etheraddr ");
		for (n = 0 ; n < 5 ; n++)
			printf("%x:", eaddr[n]);
		printf("%x\n", eaddr[5]);
		bcopy(eaddr, (caddr_t)&g->myeaddr, 6);
		return (1);
	} else {
		printf(" not found\n");
		return (0);
	}
}


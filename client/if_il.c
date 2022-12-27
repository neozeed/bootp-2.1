/*	il.h	1.0	84/09/06	*/

/*
 * Interlan NI3210 (Intel 82586) driver.
 *
 * history
 * 09/06/84	croft	created.
 * 02/05/85	croft	recover after bogus 'all ones' packets
 * 03/01/86	croft	standalone boot version.
 */


#include "../tftp/defs.h"
#undef ifp

#define	INTIL		0x0	/* interrupt level for il */
#define	splil()		0	/* asm("movw #02700, sr"); */
#define	splx()
#define	WAITCMD()	while (scb->sc_cmd)

#define	NRFD		12	/* number of receive frame descriptors */
#define	RBSZ		320	/* receive buffer size */


struct csregs {			/* command and status registers */
	u_char	cs_2;		/* command/status 2 */
	u_char	cs_1;	
	u_char	cs_xxx[6];	/* unused */
	u_short	cs_bar;		/* DMA buffer address register */
	u_short	cs_mar[2];	/* ..  memory   ..      ..     */
	u_short	cs_count;	/* DMA count */
};

/* cs_1 bits */
#define	CS_DO		0x80	/* start DMA */
#define	CS_DONE		0x40	/* DMA done */
#define	CS_TWORDS	0x20	/* DMA command, transfer words */
#define	CS_TSWAP	0x10	/*  ..  ..	swap bytes */
#define	CS_TWRITE	0x08	/*  ..  ..	write */
#define	CS_TREAD	0	/*  ..  ..	read */
#define	CS_INTA		0x07	/* DMA int level */

/* cs_2 bits */
#define	CS_CA		0x80	/* channel attention (W), interrupt (R) */
#define	CS_ONLINE	0x40	/* online */
#define	CS_SWAP		0x20	/* swap bytes */
#define	CS_IEDMA	0x10	/* enable DMA ints */
#define	CS_NRESET	0x08	/* not reset */
#define	CS_INTB		0x07	/* interrupt level */

#define	CS(bits)	CSOFF(bits|CS_ONLINE) /* bits to set in cs_2 */
#define	CSOFF(bits)	cs->cs_2 = (bits|CS_NRESET|INTIL)


/*
 * Transmit/receive buffer descriptor.
 */
struct bd {
	u_short	bd_count;	/* data count */
	u_short	bd_next;	/* link to next */
	u_short	bd_buf;		/* buffer pointer */
	u_short	bd_bufhi;
	u_short	bd_size;	/* buffer size (rbd only) */
};

/* bd flags */
/* in bd_count */
#define	BD_EOF		0x8000	/* end of frame */
#define	BD_F		0x4000	/* filled by 82586 */
#define	BD_COUNT	0x3FFF	/* count field */
/* in bd_size */
#define	BD_EL		0x8000	/* end of list */


/*
 * Command block / receive frame descriptor.
 */
struct cb {
	u_short	cb_status;	/* status */
	u_short	cb_cmd;		/* command */
	u_short	cb_link;	/* link to next */
	u_short	cb_param;	/* parameters here and following */
};

/* cb bits */
/* status */
#define	CB_C		0x8000	/* complete */
#define	CB_B		0x4000	/* busy */
#define	CB_OK		0x2000	/* ok */
#define	CB_ABORT	0x1000	/* aborted */
/* command */
#define	CB_EL		0x8000	/* end of list */
#define	CB_S		0x4000	/* suspend */
#define	CB_I		0x2000	/* interrupt (CX) when done */
/* action commands */
#define	CBC_IASETUP	1	/* individual address setup */
#define	CBC_CONFIG	2	/* configure */
#define	CBC_TRANS	4	/* transmit */


/*
 * System control block, plus some driver static structures.
 */
struct scb {
	u_short	sc_status;	/* status */
	u_short	sc_cmd;		/* command */
	u_short	sc_clist;	/* command list */
	u_short	sc_rlist;	/* receive frame list */
	u_short	sc_crcerrs;	/* crc errors */
	u_short	sc_alnerrs;	/* alignment errors */
	u_short	sc_rscerrs;	/* resource errors (lack of rfd/rbd's) */
	u_short	sc_ovrnerrs;	/* overrun errors (mem bus not avail) */
				/* end of 82586 registers */

	caddr_t	ss_base;	/* used by STOA macro */
	struct cb *ss_rfdhead;	/* receive frame descriptors, head */
	struct cb *ss_rfdtail;	/* receive frame descriptors, tail */
	struct bd *ss_rbdhead;	/* receive buffer descriptors, head */
	struct bd *ss_rbdtail;	/* receive buffer descriptors, tail */

	struct cb sc_cb;	/* general command block */
	u_char	sc_data[1024];	/* transmit buffer/general data */
	struct bd sc_tbd;	/* transmit buffer descriptor */
	struct cb sc_rfd[NRFD];	/* receive frame descriptors */
	struct bd sc_rbd[1];	/* first receive buffer descriptor */
};

/* status bits */
#define	SC_CX		0x8000	/* command executed */
#define	SC_FR		0x4000	/* frame received */
#define	SC_CNR		0x2000	/* cmd unit not ready */
#define	SC_RNR		0x1000	/* rec  .. */
#define	SC_CUS		0x700	/* command unit status field */
#define	SC_RUS		0x70	/* receive unit status field */
#define	SC_RESET	0x80	/* software reset */

/* command unit status */
#define	CUS_IDLE	0x0	/* idle */
#define	CUS_SUSP	0x100	/* suspended */
#define	CUS_READY	0x200	/* ready */

/* receive unit status */
#define	RUS_IDLE	0x0	/* idle */
#define	RUS_SUSP	0x10	/* suspended */
#define	RUS_NORES	0x20	/* no resources */
#define	RUS_READY	0x40	/* ready */

/* command unit commands */
#define	CUC_START	0x100	/* start */
#define	CUC_ABORT	0x400	/* abort */

/* receive unit commands */
#define	RUC_START	0x10	/* start */
#define	RUC_ABORT	0x40	/* abort */

#define	STOA(type,off)	((type)(scb->ss_base+(off)))/* scb offset to addr */
#define	ATOS(addr)	((int)(addr))	/* addr to scb offset */

#define	SCBLEN		0x2000	/* length of scb and structures */
#define	ISCPOFF		SCBLEN-18 /* offset of ISCP/SCP */


/*
 * System configuration pointer (ISCP and SCP) at end of memory.
 */
short	iliscp[] = {
	1,			/* busy */
	0,			/* scb at offset 0, base 0 */
	0,
	0,
	0,			/* 16 bit bus */
	0,
	0,
	ISCPOFF,		/* iscp address */
	0
};


/*
 * Configuration command data (from page 4-5 of NI3210 manual).
 */
short	ilconfig[] = {
	0,	CB_EL|CBC_CONFIG,
	0,	0x080c,
	0x2e00,	0x6000,
	0xf200,	0x0000,
	0x0040
};




/*
 * Initialize interface.
 */
ilinit(ifp, eaddr)
	struct ifconfig *ifp;
	char *eaddr;
{
	register struct	csregs *cs = (struct csregs *)ifp->ifc_addr;
	register struct	scb *scb = (struct scb *)ifp->ifc_addr2;
	int s;
	register struct cb *rfd;
	register struct	bd *rbd;
	short *iscp = (short *)((caddr_t)scb + ISCPOFF);

	s = splil();
	if (!ProbeAddress((caddr_t)cs) || !ProbeAddress((caddr_t)scb))
		return (0);
	/*
	 * setup initial SCB
	 */
	cs->cs_1 = INTIL;
	cs->cs_2 = INTIL;
	bzero((caddr_t)scb, SCBLEN);
	scb->ss_base = (caddr_t)((int)scb & 0xFF0000);
	bcopy((caddr_t)iliscp, iscp, sizeof iliscp);
	CSOFF(0);
	CSOFF(CS_CA);
	while ((cs->cs_2 & CS_CA) == 0);	/* until interrupt asserted */
	if (*iscp != 0 || scb->sc_status != (SC_CX|SC_CNR))
		return (0);	/* sanity check */
	scb->sc_cmd = (SC_CX|SC_CNR);	/* acknowledge */
	CSOFF(CS_CA);
	while ((cs->cs_2 & CS_CA) != 0);	/* until interrupt drops */
	bzero(iscp, sizeof iliscp);
	/*
	 * configure and iasetup
	 */
	bcopy((caddr_t)ilconfig, (caddr_t)&scb->sc_cb, sizeof ilconfig);
	scb->sc_clist = ATOS(&scb->sc_cb);
	ilcmd(ifp);	/* configure */
	scb->sc_cb.cb_cmd = (CB_EL|CBC_IASETUP);
	/* odd bcopy count forces byte xfer;  src/dst already swapped. */
	bcopy((caddr_t)&cs->cs_bar, (caddr_t)&scb->sc_cb.cb_param, 7);
	CSOFF(CS_SWAP);
	bcopy((caddr_t)&scb->sc_cb.cb_param, eaddr, 6);
	CSOFF(0);
	ilcmd(ifp);	/* iasetup */
	scb->sc_cb.cb_cmd = (CB_EL|CB_I|CBC_TRANS);  /* leave setup for xmit */
	scb->sc_cb.cb_param = ATOS(&scb->sc_tbd);
	scb->sc_tbd.bd_buf = ATOS(scb->sc_data);
	/*
	 * setup receive unit
	 */
	for (rfd = &scb->sc_rfd[0] ; rfd < &scb->sc_rfd[NRFD] ; rfd++ ) {
		rfd->cb_link = ATOS(rfd+1);
		rfd->cb_param = 0xFFFF;
	}
	rfd--;
	scb->ss_rfdtail = rfd;
	rfd->cb_cmd = CB_EL;
	rfd->cb_link = ATOS(&scb->sc_rfd[0]);
	scb->ss_rfdhead = rfd = &scb->sc_rfd[0];
	rfd->cb_param = ATOS(&scb->sc_rbd[0]);
	for (rbd = &scb->sc_rbd[0] ;
	    (caddr_t)(rbd+1) + RBSZ < (caddr_t)(scb) + SCBLEN ;
	    rbd = (struct bd *)((caddr_t)rbd + sizeof *rbd + RBSZ) ) {
		rbd->bd_next = ATOS((caddr_t)(rbd+1)+RBSZ);
		rbd->bd_size = RBSZ;
		rbd->bd_buf = ATOS(rbd+1);
	}
	rbd = (struct bd *)((caddr_t)rbd - sizeof *rbd - RBSZ);
	scb->ss_rbdtail = rbd;
	scb->ss_rbdhead = &scb->sc_rbd[0];
	rbd->bd_next = ATOS(&scb->sc_rbd[0]);
	rbd->bd_size |= BD_EL;
	scb->sc_rlist = ATOS(&scb->sc_rfd[0]);
	WAITCMD();
	scb->sc_cmd = (SC_CX|SC_CNR|SC_FR|SC_RNR|RUC_START);
	CS(CS_CA);
	splx(s);
}


/*
 * Read packet. Count down the ms. delay in *timer and
 * return if timeout (*timer < 0).
 */
ilread(ifp, buf, bcount, timer)
	struct ifconfig *ifp;
	u_char *buf;
	int *timer;
{
	register struct	csregs *cs = (struct csregs *)ifp->ifc_addr;
	register struct	scb *scb = (struct scb *)ifp->ifc_addr2;
	int s,scbstatus;
	register struct cb *rfd;
	register struct bd *rbd;
	int time;
	u_char *cp;
	int free, count;

	/* spl used to guard against NI3210 edge vs. level trig. intr. */
	s = splil();
	time = MSCLOCK;
wait:
	while ((scbstatus = (scb->sc_status & (SC_FR|SC_RNR))) == 0) {
		if (MSCLOCK - time >= *timer) {
			*timer -= (MSCLOCK - time);
			return (0);
		}
	}
	WAITCMD();
	scb->sc_cmd = scbstatus;
	CS(CS_CA);	/* ack current interrupts */
	WAITCMD();
	if ((scbstatus & SC_RNR) && 
	    ((rfd = scb->ss_rfdhead)->cb_status & CB_B)) {
		/*
		 * Receive unit not ready, yet still busy on 1st frame!
		 * This is a bogus packet of 'infinite' length and all
		 * ones.  Restart the RU.
		 */
		for (rbd = STOA(struct bd *, rfd->cb_param) ;
		    rbd->bd_count & BD_F ;
		    rbd = STOA(struct bd *, rbd->bd_next) ) {
			rbd->bd_count = 0;
		}
		ilrstart(ifp);
	}
	if ((scbstatus & SC_FR) == 0)
		goto wait;	/* if frame not received */
	/*
	 * receive frame.
	 */
	rfd = scb->ss_rfdhead; 
	if ((rfd->cb_status&CB_C) == 0)
		goto wait;	/* impossible? */
	free = bcount;
	cp = buf;
	for (rbd = STOA(struct bd *, rfd->cb_param) ;
	    rbd->bd_count & BD_F ;
	    rbd = STOA(struct bd *, rbd->bd_next) ) {
		count = (rbd->bd_count & BD_COUNT);
		if (count <= free) {
			CS(CS_SWAP);
			bcopy((caddr_t)(rbd+1), cp, count);
			CS(0);		/* no swap */
			cp += count;
			free -= count;
		} 
		if (rbd->bd_count & BD_EOF)
			break;
		rbd->bd_count = 0;
	}
	rbd->bd_count = 0;
	rbd->bd_size |= BD_EL;
	scb->ss_rbdtail->bd_size &= BD_COUNT; /* clear previous EL */
	scb->ss_rbdtail = rbd;
	scb->ss_rbdhead = STOA(struct bd *, rbd->bd_next);
	count = bcount - free;

	rfd->cb_status = 0;
	rfd->cb_cmd = CB_EL;
	rfd->cb_param = 0xFFFF;
	scb->ss_rfdtail->cb_cmd = 0;	/* clear previous CB_EL */
	scb->ss_rfdtail = rfd;

	rfd = scb->ss_rfdhead = STOA(struct cb *, rfd->cb_link);
	ilrstart(ifp);	/* kick the receive unit */
	*timer -= (MSCLOCK - time);
	splx(s);
	return (count);
}


/*
 * Write (transmit) packet.
 */
ilwrite(ifp, buf, count)
	struct ifconfig *ifp;
	u_char *buf;
{
	register struct	csregs *cs = (struct csregs *)ifp->ifc_addr;
	register struct	scb *scb = (struct scb *)ifp->ifc_addr2;
	int s;
	int time = MSCLOCK + 500;	/* .5 second delay */

	s = splil();
	CS(CS_SWAP);
	bcopy(buf, (caddr_t)&scb->sc_data[0], count);
	CS(0);
	if (count < 64)
		count = 64;
	scb->sc_tbd.bd_count = (count | BD_EOF);
	WAITCMD();
	scb->sc_cmd = (SC_CX|SC_CNR|CUC_START);
	CS(CS_CA);
	WAITCMD();
	while ((scb->sc_status & SC_CX) == 0) {
		if (MSCLOCK - time > 0) {
			char eaddr[6];
			printf("(iljammed)");
			ilinit(ifp, eaddr);
			goto out;
		}
	}
	if ((scb->sc_cb.cb_status & CB_C) == 0
	    || (scb->sc_cb.cb_status & CB_OK) == 0)
		printf("(ilerror)");
out:
	splx(s);
}

	
/*
 * Execute a single command, in scb->sc_cb.
 */
ilcmd(ifp)
	struct ifconfig *ifp;
{
	register struct	csregs *cs = (struct csregs *)ifp->ifc_addr;
	register struct	scb *scb = (struct scb *)ifp->ifc_addr2;

	WAITCMD();
	scb->sc_cmd = (SC_CX|SC_CNR|CUC_START);
	CSOFF(CS_CA);
	WAITCMD();
	while ((scb->sc_status & SC_CNR) == 0);
	scb->sc_cmd = (SC_CX|SC_CNR);	/* ack, clear interr */
	CSOFF(CS_CA);
}


/*
 * Start receive unit, if needed.
 */
ilrstart(ifp)
	struct ifconfig *ifp;
{
	register struct	csregs *cs = (struct csregs *)ifp->ifc_addr;
	register struct	scb *scb = (struct scb *)ifp->ifc_addr2;

	/* ignore if RU already running or less than 2 elements on lists */
	if ((scb->sc_status & SC_RUS) == RUS_READY)
		return;
	if (scb->ss_rfdhead->cb_cmd & CB_EL)
		return;
	if (scb->ss_rbdhead->bd_size & BD_EL)
		return;
	WAITCMD();
	scb->ss_rfdhead->cb_param = ATOS(scb->ss_rbdhead);
	scb->sc_rlist = ATOS(scb->ss_rfdhead);
	scb->sc_cmd = RUC_START;
	CS(CS_CA);
}

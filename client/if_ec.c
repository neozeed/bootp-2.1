/*	if_ec.c	1.1	2/6/86	*/

/*
 * 3COM 3C400 Ethernet driver.
 *
 * history
 * 01/22/86	croft	created.
 */


#include "../tftp/defs.h"
#undef ifp


/*
 * 3Com Ethernet controller registers.
 */
struct ecdevice {
	u_short	ec_csr;		/* control and status */
	u_short	ec_back;	/* backoff value */
	u_char	ec_pad1[0x400-2*2];
	u_char	ec_arom[6];	/* address ROM */
	u_char	ec_pad2[0x200-6];
	u_char	ec_aram[6];	/* address RAM */
	u_char	ec_pad3[0x200-6];
	u_char	ec_tbuf[2048];	/* transmit buffer */
	u_char	ec_abuf[2048];	/* receive buffer A */
	u_char	ec_bbuf[2048];	/* receive buffer B */
};

/*
 * Control and status bits
 */
#define	EC_BBSW		0x8000		/* buffer B belongs to ether */
#define	EC_ABSW		0x4000		/* buffer A belongs to ether */
#define	EC_TBSW		0x2000		/* transmit buffer belongs to ether */
#define	EC_JAM		0x1000		/* Ethernet jammed (collision) */
#define	EC_AMSW		0x0800		/* address RAM belongs to ether */
#define	EC_RBBA		0x0400		/* buffer B older than A */
#define	EC_RESET	0x0100		/* reset controller */
#define	EC_BINT		0x0080		/* buffer B interrupt enable */
#define	EC_AINT		0x0040		/* buffer A interrupt enable */
#define	EC_TINT		0x0020		/* transmitter interrupt enable */
#define	EC_JINT		0x0010		/* jam interrupt enable */
#define	EC_INTPA	0x00ff		/* mask for interrupt and PA fields */
#define	EC_PAMASK	0x000f		/* PA field */

#define	EC_PA		0x0007		/* receive mine+broadcast-errors */

/*
 * Receive status bits
 */
#define	EC_FCSERR	0x8000		/* FCS error */
#define	EC_BROADCAST	0x4000		/* packet was broadcast packet */
#define	EC_RGERR	0x2000		/* range error */
#define	EC_ADDRMATCH	0x1000		/* address match */
#define	EC_FRERR	0x0800		/* framing error */
#define	EC_DOFF		0x07ff		/* first free byte */

#define	ECRDOFF		2		/* packet offset in read buffer */
#define	ECMAXTDOFF	(2048-60)	/* max packet offset (min size) */

/* Macros to set and clear CSR bits */

#define CSRSET(v) ec->ec_csr = (ec->ec_csr & EC_INTPA) | (v)
#define CSRCLR(v) ec->ec_csr = ec->ec_csr & (EC_INTPA & ~(v))


/*
 * Initialize interface.  Returns 0 if failure.
 * Stores our ether address in the 2nd argument.
 */
ecinit(ifp, eaddr)
	struct ifconfig *ifp;
	char *eaddr;
{
	register struct ecdevice *ec = (struct ecdevice *)ifp->ifc_addr;

	if (!ProbeAddress((caddr_t)ec))
		return(0);
	ecreset(ec);
	bcopy(ec->ec_arom, eaddr, 6);
	return (1);
}


/*
 * Reset the interface and load the address RAM.
 * Enable the receive buffers.
 */
ecreset(ec)
	register struct ecdevice *ec;
{
	register i;

	ec->ec_csr = EC_RESET;
	for (i=0 ; i<10 ; i++);
	bcopy(ec->ec_arom, ec->ec_aram, 6);
	CSRSET(EC_AMSW);
	CSRSET(EC_PA | EC_ABSW | EC_BBSW);
}


/*
 * Write (xmit) packet to ether.
 */
ecwrite(ifp, buf, count)
	struct ifconfig *ifp;
	u_char *buf;
{
	register u_char *cp;
	register struct ecdevice *ec = (struct ecdevice *)ifp->ifc_addr;
	short mask = -1, back;
	int time = MSCLOCK + 500;	/* .5 seconds */

	if (count < 60)
		count = 60;
	cp = &ec->ec_abuf[-count];
	bcopy(buf, cp, count);
	*(short *)(ec->ec_tbuf) = cp - (u_char *)ec->ec_tbuf;
	CSRSET(EC_TBSW);
	for (;;) {
		if (MSCLOCK - time > 0) {
			ecreset(ec);
			return;
		}
		if (ec->ec_csr & EC_JAM) {
			mask <<= 1;
			if (mask == 0) {
				ecreset(ec);
				return;
			}
			back = -(MSCLOCK & ~mask);
			if (back == 0)
				back = -(0x5555 & ~mask);
			ec->ec_back = back;
			CSRSET(EC_JAM);
		}
		if ((ec->ec_csr & EC_TBSW) == 0)
			return;
	}
}


/*
 * Read packet from ether.  Count down the ms. delay in *timer
 * and return if timeout (< 0).
 */
ecread(ifp, buf, count, timer)
	struct ifconfig *ifp;
	u_char *buf;
	register int *timer;
{
	register struct ecdevice *ec = (struct ecdevice *)ifp->ifc_addr;
	int time;
	short *sp;
	int xbsw = 0;
	int rcount;

	time = MSCLOCK;
wait:
	CSRSET(xbsw);
	for (;;) {
		if ((ec->ec_csr & EC_ABSW) == 0) {
			sp = (short *)ec->ec_abuf;
			xbsw = EC_ABSW;
			break;
		}
		if ((ec->ec_csr & EC_BBSW) == 0) {
			sp = (short *)ec->ec_bbuf;
			xbsw = EC_BBSW;
			break;
		}
		if (MSCLOCK - time >= *timer) {
			*timer -= (MSCLOCK - time);
			return (0);
		}
	}
	rcount = (*sp & EC_DOFF);
	if (rcount > count)
		rcount = count;
	bcopy((caddr_t)&sp[1], buf, rcount);
	CSRSET(xbsw);
	*timer -= (MSCLOCK - time);
	return (rcount);
}

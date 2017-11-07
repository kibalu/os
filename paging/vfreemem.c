/* vfreemem.c - vfreemem */

#include <conf.h>
#include <kernel.h>
#include <mem.h>
#include <proc.h>
#include <paging.h>
extern struct pentry proctab[];
/*------------------------------------------------------------------------
 *  vfreemem  --  free a virtual memory block, returning it to vmemlist
 *------------------------------------------------------------------------
 */
SYSCALL	vfreemem(block, size)
	struct	mblock	*block;
	unsigned size;
{
	STATWORD ps;
	disable(ps);
	
	if (size == 0 || (unsigned)block < (unsigned)(4096 * NBPG)) {
		restore(ps);
		return(SYSERR);
	}
	
	size = (unsigned)roundmb(size);
	
	struct	mblock	*p, *q;
	p = proctab[currpid].vmemlist;
	q = proctab[currpid].vmemlist->mnext;
	
	if (q == NULL) {
		block->mlen = size;
		block->mnext = NULL;
		p->mnext = block;
	} else {
		while(q != NULL) {
			if((unsigned)block < (unsigned)q) {
				if(((unsigned)block + size) == (unsigned)q) {
					p->mnext = block;
					block->mnext = q->mnext;
					block->mlen = q->mlen + size;
				} else {
					block->mlen = size;
					block->mnext = q;
					p->mnext = block;
				}
			}
			p=q;
			q=q->mnext;
		}
	}
	return(OK);
}

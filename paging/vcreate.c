/* vcreate.c - vcreate */
    
#include <conf.h>
#include <i386.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <paging.h>

/*
static unsigned long esp;
*/

LOCAL	newpid();
/*------------------------------------------------------------------------
 *  create  -  create a process to start running a procedure
 *------------------------------------------------------------------------
 */
SYSCALL vcreate(procaddr,ssize,hsize,priority,name,nargs,args)
	int	*procaddr;		/* procedure address		*/
	int	ssize;			/* stack size in words		*/
	int	hsize;			/* virtual heap size in pages	*/
	int	priority;		/* process priority > 0		*/
	char	*name;			/* name (for debugging)		*/
	int	nargs;			/* number of args that follow	*/
	long	args;			/* arguments (treated like an	*/
					/* array in the code)		*/
{
	STATWORD ps;
	disable(ps);
	
	if (hsize > 256) {
		restore(ps);
		return SYSERR;
	}
	
	int bs_id = get_bsm();
	if(bs_id == SYSERR) {
		restore(ps);
		return SYSERR;
	}
	
	int pid = create(procaddr, ssize, priority, name, nargs, args);
	if(pid == SYSERR){
		restore(ps);
		return SYSERR;
	}
	
	if (bsm_map(pid, 4096, bs_id, hsize) == SYSERR) {
		restore(ps);
		return SYSERR;
	}
	bsm_tab[bs_id].private = 1;
	// Adding the mapping for 4096 virtual address;
	unsigned long virtpage = 4096;
	unsigned long directory_offset = virtpage / 1024;
	
	pd_t* directory_entry = proctab[pid].pdbr + (directory_offset * sizeof(pd_t));
	if(directory_entry->pd_pres == 0) {
		int table_frame;
		int i;
		get_frm(&table_frame);
		frm_tab[table_frame].fr_status = FRM_MAPPED;
		frm_tab[table_frame].fr_pid = pid;
		frm_tab[table_frame].fr_vpno = -1;
		frm_tab[table_frame].fr_refcnt = 0;
		frm_tab[table_frame].fr_type = FR_TBL;
		frm_tab[table_frame].fr_dirty = 0;
	
		for (i = 0; i < 1024; i++) {
			pt_t *pt = (FRAME0 + table_frame) * NBPG + (i * sizeof(pt_t));
			pt->pt_pres = 0;
			pt->pt_write = 1;
			pt->pt_user = 0;
			pt->pt_pwt = 0;
			pt->pt_pcd = 0;
			pt->pt_acc = 0;
			pt->pt_dirty = 0;
			pt->pt_mbz = 0;
			pt->pt_global = 0;
			pt->pt_avail = 0;
			pt->pt_base = 0;		
		}



		if(table_frame == SYSERR){
			kprintf("Unable to allocate frame for the current process. Iniating kill for current process ...");
			kill(pid);
			restore(ps);
			return SYSERR;
		}
		
	  	directory_entry->pd_base = FRAME0 + table_frame;
	  	directory_entry->pd_pres = 1;
	  	directory_entry->pd_write = 1;
		unsigned long entry_index = (unsigned long) directory_entry / 4096;
		entry_index -= FRAME0;
	  	frm_tab[entry_index].fr_refcnt++;
	}
	
	int i;
	for (i = 0; i < hsize; i ++) {
		pt_t *table_entry = directory_entry->pd_base * NBPG + (i * sizeof(pt_t));
		
		////////////////
		if (bsm_tab[bs_id].bs_frame_no[i] == -1) {
			int fr = SYSERR;
			get_frm(&fr);
		
			if (fr == SYSERR) {
				kprintf("Unable to obtain the frame number \n");
				restore(ps);
				return SYSERR;
			}
		
			table_entry->pt_base = FRAME0 + fr;
			bsm_tab[bs_id].bs_frame_no[i] = FRAME0 + fr;
			read_bs(bsm_tab[bs_id].bs_frame_no[i] * NBPG, bs_id, i);
		
			// Add the page to the replacement policy queue
			if (grpolicy() == SC) {
				if (scroot == NULL) {
					scroot = getmem(sizeof(sclist));
					scroot->frame_no = FRAME0 + fr;
					scroot->next = scroot;
					sccur = scroot;
				} else {
					sclist *it = scroot;
					while (it->next != scroot) {
						it = it->next;
					}
					sclist *node = getmem(sizeof(sclist));
					node->frame_no = FRAME0 + fr;
					node->next = scroot;
					it->next = node;
				}
			}
			frm_tab[bsm_tab[bs_id].bs_frame_no[i] - FRAME0].fr_pid = pid;
			frm_tab[bsm_tab[bs_id].bs_frame_no[i] - FRAME0].fr_status = FRM_MAPPED;
			frm_tab[bsm_tab[bs_id].bs_frame_no[i] - FRAME0].fr_type = FR_PAGE;
		} else {
			table_entry->pt_base = bsm_tab[bs_id].bs_frame_no[i];
		}
		
		table_entry->pt_pres = 1;
		table_entry->pt_write = 1;
		
		frm_tab[bsm_tab[bs_id].bs_frame_no[i] - FRAME0].fr_refcnt ++;
		fr_map_t *fr_ptr;
		fr_ptr = &frm_tab[bsm_tab[bs_id].bs_frame_no[i] - FRAME0];
	  
		pid_of_this_fram_list *list = fr_ptr->list;
		int flag = 1;
		while (list->next != NULL) {
			if (list->next->pid == currpid && 
			list->next->vpno == (virtpage + i)) {
				flag = 0;
				break;
			}
			list = list->next;
		}
	  
		if (flag) {
			pid_of_this_fram_list *node = getmem(sizeof(pid_of_this_fram_list));
			node->pid = currpid;
			node->vpno = (virtpage + i);
			node->next = NULL;
			list->next = node;
		}
	}
	write_cr3(proctab[pid].pdbr);
	int temp_pid = currpid;
	currpid = pid;
	
	proctab[pid].vhpno = 4096;
	proctab[pid].vhpnpages = hsize;
	proctab[pid].vmemlist = getmem(sizeof(struct mblock));
	proctab[pid].vmemlist->mlen = 0;
	proctab[pid].vmemlist->mnext = (struct mblock*) (4096 * NBPG);
	proctab[pid].vmemlist->mnext->mlen = hsize * NBPG;
	proctab[pid].vmemlist->mnext->mnext = NULL;
	
	// Address resolution back to the current process
	currpid = temp_pid;
	write_cr3(proctab[currpid].pdbr);
	
	restore(ps);	
	return pid;
	return OK;
}

/*------------------------------------------------------------------------
 * newpid  --  obtain a new (free) process id
 *------------------------------------------------------------------------
 */
LOCAL	newpid()
{
	int	pid;			/* process id to return		*/
	int	i;

	for (i=0 ; i<NPROC ; i++) {	/* check all NPROC slots	*/
		if ( (pid=nextproc--) <= 0)
			nextproc = NPROC-1;
		if (proctab[pid].pstate == PRFREE)
			return(pid);
	}
	return(SYSERR);
}

/* xm.c = xmmap xmunmap */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>


/*-------------------------------------------------------------------------
 * xmmap - xmmap
 *-------------------------------------------------------------------------
 */
bs_map_t bsm_tab[8];

SYSCALL xmmap(int virtpage, bsd_t source, int npages)
{	
	STATWORD ps;
	disable(ps);
	if(virtpage < 4096){
		kprintf("virtpage is smaller than 4096");
		restore(ps);
		return SYSERR;
	}
	if(source < 0){
		kprintf("source is small than 0");
		restore(ps);
		return SYSERR;
	}
	if(source > 7){
		kprintf("source is bigger than 7");
		restore(ps);
		return SYSERR;
	}
	if(npages < 1){
		kprintf("npages is smaller than 1");
		restore(ps);
		return SYSERR;
	}
	if(npages > 256){
		kprintf("npages is bigger than 256");
		restore(ps);
		return SYSERR;
	}
	bs_map_t *ptr = &bsm_tab[source];
	if(ptr -> bs_status == BSM_UNMAPPED || ptr -> private == 1){
		restore(ps);
		return SYSERR;
	}
	if(npages > ptr -> bs_npages){
		restore(ps);
		return SYSERR;
	}
	if(bsm_map(currpid, virtpage, source, npages)){
		restore(ps);
		return OK;
	}else{
		restore(ps);
		return SYSERR;
	}
	unsigned long pd_offset = virtpage /1024;
	unsigned long pt_offset = virtpage%1024;
	pd_t *pd_address = proctab[currpid].pdbr + (pd_offset * sizeof(pd_t));
	if(pd_address->pd_pres == 0){
		int frame_no = SYSERR;
		get_frm(&frame_no);
		if(frame_no == SYSERR){
			kill(currpid);
			restore(ps);
			return SYSERR;
		}
		frm_tab[frame_no].fr_status = FRM_MAPPED;
		frm_tab[frame_no].fr_pid = currpid;
		frm_tab[frame_no].fr_vpno = -1;
		frm_tab[frame_no].fr_refcnt = 0;
		frm_tab[frame_no].fr_type = FR_TBL;
		frm_tab[frame_no].fr_dirty = 0;
		int i = 0;
		for(i = 0; i < 1024; i++){
			pt_t *pt_address = (FRAME0 + frame_no) * NBPG + (i * sizeof(pt_t));
			pt_address->pt_pres = 0;
			pt_address->pt_write = 1;
			pt_address->pt_user = 0;
			pt_address->pt_pwt = 0;
			pt_address->pt_pcd =0;
			pt_address->pt_acc = 0;
			pt_address->pt_dirty = 0;
			pt_address->pt_mbz = 0;
			pt_address->pt_global = 0;
			pt_address->pt_avail = 0;
			pt_address->pt_base = 0;
		}
		pd_address->pd_base = FRAME0 + frame_no;
		pd_address->pd_pres = 1;
		pd_address->pd_write = 1;
		frm_tab[(unsigned long)pd_address/4096 - FRAME0].fr_refcnt++;
	}
	pt_t *pt_address;
	int i = 0;
	for(i = 0; i < npages; i++){
		pt_address = pd_address->pd_base * NBPG + (i + pt_offset) * sizeof(pt_t);
		if(bsm_tab[source].bs_frame_no[i] == -1){
			int cur_no;
			get_frm(&cur_no);
			pt_address->pt_base = FRAME0 + cur_no;
			bsm_tab[source].bs_frame_no[i] = FRAME0 + cur_no;
			read_bs(bsm_tab[source].bs_frame_no[i] * NBPG, source, i);
			if(grpolicy() == SC){
				if(scroot != NULL){
					sclist *list = scroot;
					while(list->next != scroot){
						list = list->next;
					}
					sclist *new = getmem(sizeof(sclist));
					new->frame_no = cur_no * FRAME0;
					new->next = scroot;
					list->next = new;
				}else{
					scroot = getmem(sizeof(sclist));
					scroot->frame_no = cur_no + FRAME0;
					scroot->next = scroot;
					sccur = scroot;
				}
			}
		}else{
			pt_address->pt_base = bsm_tab[source].bs_frame_no[i];
		}
		pt_address->pt_pres = 1;
		pt_address->pt_write = 1;
		frm_tab[bsm_tab[source].bs_frame_no[i] - FRAME0].fr_status = FRM_MAPPED;
		frm_tab[bsm_tab[source].bs_frame_no[i] - FRAME0].fr_refcnt++;
		fr_map_t *fr_ptr;
		fr_ptr = &frm_tab[bsm_tab[source].bs_frame_no[i] - FRAME0];
		pid_of_this_fram_list *curlist;
		curlist = fr_ptr->list;
		while(curlist->next != NULL){
			if(curlist->next->pid == currpid && curlist->next->vpno == (virtpage + i)){
				pid_of_this_fram_list *newlist = getmem(sizeof(pid_of_this_fram_list));
				newlist->pid = currpid;
				newlist->vpno = virtpage + i;
				newlist->next = NULL;
				curlist->next = newlist;
				break;
			}
			curlist = curlist->next;
		}
	}
	write_cr3(proctab[currpid].pdbr);
	restore(ps);
	return OK;
}



/*-------------------------------------------------------------------------
 * xmunmap - xmunmap
 *-------------------------------------------------------------------------
 */
SYSCALL xmunmap(int virtpage)
{
	STATWORD ps;
	disable(ps);
	if(virtpage < 4096){
		kprintf("virtpage smaller than 4096");
		restore(ps);
		return SYSERR;
	}
	if(bsm_unmap(currpid, virtpage)){
		restore(ps);
		write_cr3(proctab[currpid].pdbr);
		return OK;
	}else{
		restore(ps);
		return SYSERR;
	}
}

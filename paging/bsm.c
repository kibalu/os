/* bsm.c - manage the backing store mapping*/

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

/*-------------------------------------------------------------------------
 * init_bsm- initialize bsm_tab
 *-------------------------------------------------------------------------
 */
bs_map_t bsm_tab[8];
SYSCALL init_bsm()
{
	int i = 0;
	for(i = 0; i < 8; i++){
		bsm_tab[i].bs_status = BSM_UNMAPPED;
		bsm_tab[i].bs_pid = -1;
		bsm_tab[i].bs_vpno = -1;
		bsm_tab[i].bs_npages = 256;
		bsm_tab[i].bs_sem = -1;
		bsm_tab[i].private = 0;
		int j = 0;
		for(j = 0; j < 256; j++){
			bsm_tab[i].bs_frame_no[j] = -1;
		}
	}
	return OK;
}

/*-------------------------------------------------------------------------
 * get_bsm - get a free entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL get_bsm(int* avail)
{
	STATWORD ps;
	disable(ps);
	int i = 0;
	for (i = 0; i < 8; i ++) {
		if (bsm_tab[i].bs_status == BSM_UNMAPPED) {
			*avail = i;
			restore(ps);
			return OK;
		}
	}
	
	restore(ps);
	return SYSERR;
}


/*-------------------------------------------------------------------------
 * free_bsm - free an entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL free_bsm(int i)
{
	STATWORD ps;
	disable(ps);
	if(i < 0 || i > 7 ){
		restore(ps);
		return SYSERR;
	}
	bsm_tab[i].bs_status = BSM_UNMAPPED;
	bsm_tab[i].bs_pid = -1;
	bsm_tab[i].bs_vpno = 0;
	bsm_tab[i].bs_npages = 256;
	bsm_tab[i].bs_sem = 0;
	bsm_tab[i].private = 0;
	int j = 0;
	for(j = 0; j < 256; j++){
		bsm_tab[i].bs_frame_no[j] = -1;
	}
	restore(ps);
	return OK;
}

/*-------------------------------------------------------------------------
 * bsm_lookup - lookup bsm_tab and find the corresponding entry
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_lookup(int pid, long vaddr, int* store, int* pageth)
{
	STATWORD ps;
	disable(ps);
	struct pentry *pptr = &proctab[pid];
	bs_no *list = pptr->bshead;
	if(list == NULL){
		restore(ps);
		return SYSERR;
	}
	unsigned long vpnumber = vaddr/4096;
	while(list != NULL){
		int bsid = list->bs_id;
		if(bsm_tab[bsid].bs_vpno <= vpnumber && (bsm_tab[bsid].bs_vpno + bsm_tab[bsid].bs_npages > vpnumber) && bsm_tab[bsid].bs_status == BSM_MAPPED){
			*store = bsid;
			*pageth = vpnumber - bsm_tab[bsid].bs_vpno;
			restore(ps);
			return OK;
		}
		list = list->next;
	}
	restore(ps);
	return SYSERR;
}


/*-------------------------------------------------------------------------
 * bsm_map - add an mapping into bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_map(int pid, int vpno, int source, int npages)
{
	STATWORD ps;
	disable(ps);
	if(vpno < 4096 || source <0 || source > 7 || npages > 256 || bsm_tab[source].private == 1){
		return SYSERR;
	}
	struct pentry *pptr;
	pptr = &proctab[pid];
	if(bsm_tab[source].bs_status == BSM_MAPPED){
		if(bsm_tab[source].bs_vpno == -1){
			bsm_tab[source].bs_vpno = vpno;
		}
		if(bsm_tab[source].bs_pid == -1){
			bsm_tab[source].bs_pid = pid;
		}	
		bsm_tab[source].private = 0;
	}else if(bsm_tab[source].bs_status == BSM_UNMAPPED){
		bsm_tab[source].bs_pid = pid;
		bsm_tab[source].bs_npages = npages;
		bsm_tab[source].bs_vpno = vpno;
		bsm_tab[source].bs_npages = npages;
		bsm_tab[source].private = 0;
	}
	if(pptr->bshead == NULL){
		//bs_no *list = getmem(sizeof(bs_no));
		pptr->bshead = getmem(sizeof(bs_no));
		bs_no *new = getmem(sizeof(bs_no));
		new->bs_id = source;
		new->next = NULL;
		pptr->bshead = new;
		
	}else{
		int flag = 1;
		bs_no *list = pptr->bshead;
		while(list!=NULL){
			if(list->bs_id == source){
				flag = 0;
			}
			if(list->next == NULL){
				break;
			}else{
				list = list->next;
			}
		}
		if(flag == 1){
			bs_no *new = getmem(sizeof(bs_no));
			new->bs_id = source;
			new->next = NULL;
			list->next = new;
		}
	}
	restore(ps);
	return OK;
}



/*-------------------------------------------------------------------------
 * bsm_unmap - delete an mapping from bsm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_unmap(int pid, int vpno, int flag)
{
	STATWORD ps;
	disable(ps);
	int store;
	int pageth;
	bsm_lookup(pid, vpno * NBPG, &store, &pageth);
	if (flag) {
		proctab[pid].vmemlist = NULL;
	} else {
		unsigned long pd_offset = vpno / 1024;
		unsigned long pt_offset = vpno % 1024;
		
		pd_t *pd_address = proctab[pid].pdbr + (pd_offset * sizeof(pd_t));
		
		int i;
		for (i = 0; i < bsm_tab[store].bs_npages; i ++) {
			pt_t *pt_address = 4096 * pd_address->pd_base  + ((pt_offset + i) * sizeof(pt_t));
			
			int frame = pt_address->pt_base - FRAME0;
			if (frame < 1024) {
				fr_map_t *fr = &frm_tab[frame];
				pid_of_this_fram_list *list = fr->list;
				if (frm_tab[frame].fr_status == FRM_MAPPED && list->next == NULL) {
					free_frm(frame);
				} else {

					pt_address->pt_acc = 0;
					pt_address->pt_avail = 0;
					pt_address->pt_base = 0;
					pt_address->pt_dirty = 0;
					pt_address->pt_global = 0;
					pt_address->pt_mbz = 0;
					pt_address->pt_pcd = 0;
					pt_address->pt_pres = 0;
					pt_address->pt_pwt = 0;
					pt_address->pt_write = 1;
					pt_address->pt_user = 0;
					unsigned long frame_no = (unsigned long)pt_address /4096 - FRAME0;
					frm_tab[frame_no].fr_refcnt --;
					if (frm_tab[frame_no].fr_refcnt == 0) {
						frm_tab[frame_no].fr_dirty = 0;
						frm_tab[frame_no].fr_pid = -1;
						frm_tab[frame_no].fr_refcnt = 0;
						frm_tab[frame_no].fr_status = FRM_UNMAPPED;
						frm_tab[frame_no].fr_type = FR_PAGE;
						frm_tab[frame_no].fr_vpno = -1;
				
						pd_address->pd_pres = 0;
						pd_address->pd_write = 1;
						pd_address->pd_user = 0;
						pd_address->pd_pwt = 0;
						pd_address->pd_pcd = 0;
						pd_address->pd_acc = 0;
						pd_address->pd_mbz = 0;
						pd_address->pd_fmb = 0;
						pd_address->pd_global = 0;
						pd_address->pd_avail = 0;
						pd_address->pd_base = 0;
					}
				}
			}
			
			// Remove frame entry from the frm_tab
			fr_map_t *fr = &frm_tab[frame];
			pid_of_this_fram_list *list = fr->list;
			
			while (list->next != NULL) {
				if (list->next->vpno == vpno && list->next->pid == pid) {
					pid_of_this_fram_list *delete_node = list->next;
					list->next = list->next->next;

					freemem(delete_node, sizeof(pid_of_this_fram_list));
					break;
				}
				list = list->next;
			}
		}
		pd_address->pd_pres = 0;
		pd_address->pd_write = 1;
		pd_address->pd_user = 0;
		pd_address->pd_pwt = 0;
		pd_address->pd_pcd = 0;
		pd_address->pd_acc = 0;
		pd_address->pd_mbz = 0;
		pd_address->pd_fmb = 0;
		pd_address->pd_global = 0;
		pd_address->pd_avail = 0;
		pd_address->pd_base = 0;
	}
	
	//bsm_tab[store].bs_ref_count --;
	
	// bs_no *p, *q;
	// struct pentry *pptr;
	// pptr = &proctab[pid];
	// p = pptr->bshead;
	
	// if (p->bs_id == store && 
	// p->bs_map.bs_pid == pid && 
	// p->bs_map.bs_vpno == vpno) {
	// 	pptr->bshead = p->next;
	// 	freemem(p, sizeof(bs_list));
	// } else {
	// 	q = p->next;
	// 	int flag = 1;
	// 	while (q->next != NULL) {
	// 		if (q->bs_id == store && 
	// 		q->bs_map.bs_pid == pid && 
	// 		q->bs_map.bs_vpno == vpno) {
	// 			p->next = q->next;
	// 			flag = 0;
	// 			freemem(q, sizeof(bs_list));
	// 			break;
	// 		}
	// 		p = p->next;
	// 		q = q->next;
	// 	}
		
	// 	if (flag && q->bs_id == store && 
	// 	q->bs_map.bs_pid == pid && 
	// 	q->bs_map.bs_vpno == vpno) {
	// 		p->next = q->next;
	// 		freemem(q, sizeof(bs_list));
	// 	}
	// }
	
	restore(ps);
	return OK;
}




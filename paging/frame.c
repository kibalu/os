/* frame.c - manage physical frames */
#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

/*-------------------------------------------------------------------------
 * init_frm - initialize frm_tab
 *-------------------------------------------------------------------------
 */
fr_map_t frm_tab[NFRAMES];

SYSCALL init_frm()
{
	int i = 0;
	for(i = 0; i < NFRAMES; i++){
		frm_tab[i].fr_status = FRM_UNMAPPED;
		frm_tab[i].fr_pid = -1;
		frm_tab[i].fr_vpno = -1;
		frm_tab[i].fr_refcnt = 0;
		frm_tab[i].fr_type = FR_PAGE;
		frm_tab[i].fr_dirty = 0;
	}
  kprintf("To be implemented!\n");
  return OK;
}

/*-------------------------------------------------------------------------
 * get_frm - get a free frame according page replacement policy
 *-------------------------------------------------------------------------
 */
SYSCALL get_frm(int* avail)
{
	STATWORD ps;
	disable(ps);
	int i = 0;
	for(i = 0; i < NFRAMES; i++){
		if(frm_tab[i].fr_status == FRM_UNMAPPED){
			frm_tab[i].fr_status = FRM_UNMAPPED;
			*avail = i;
			restore(ps);
			return OK;
		}
	}
	int flag = 0;
	if(grpolicy() == SC){
		while(1){
			int cur = sccur->frame_no;
			pid_of_this_fram_list *list = frm_tab[cur - FRAME0].list;
			unsigned long pt_offset = list->vpno%1024;
			unsigned long pd_offset = list->vpno/1024;
			pd_t *pd_address = proctab[list->pid].pdbr + (pd_offset*sizeof(pd_t));
			pt_t *pt_address = NBPG * pd_address->pd_base + (sizeof(pt_t) * pt_offset);
			if(pt_address->pt_acc){
				pt_address->pt_acc = 0;
			}else{
				*avail = sccur->frame_no - FRAME0;
				break;
			}
			sccur = sccur->next;
		}
		free_frm(sccur->frame_no - FRAME0);
		sccur = sccur->next;
	}
    restore(ps);
  return OK;
}

/*-------------------------------------------------------------------------
 * free_frm - free a frame 
 *-------------------------------------------------------------------------
 */
SYSCALL free_frm(int i)
{
	STATWORD ps;
	disable(ps);
	if(i < 0 || i >= NFRAMES){
		restore(ps);
		return SYSERR;
	}
	if(frm_tab[i].fr_type == FR_DIR){
		int j;
		for(j = 4; j < 2014; j++){
			pd_t *pd_address = proctab[currpid].pdbr + (j * sizeof(pd_t));
			if(pd_address -> pd_pres){
				free_frm(pd_address->pd_base - FRAME0);
			}
		}
		frm_tab[i].fr_status = FRM_UNMAPPED;
		frm_tab[i].fr_pid = -1;
		frm_tab[i].fr_vpno = -1;
		frm_tab[i].fr_refcnt = 0;
		frm_tab[i].fr_type = FR_PAGE;
		frm_tab[i].fr_dirty = 0;
		restore(ps);
		return OK;
	}
	if(frm_tab[i].fr_type == FR_TBL){
		int j = 0;
		for(j = 0; j < 1024; j++){
			pt_t *pt_address = (FRAME0 + i) * NBPG + (j * sizeof(pt_t));
			if(pt_address->pt_pres){
				free_frm(pt_address->pt_pres);
			}
		}
		for(j = 0; j < 1024; j++){
			pd_t *pd_address = proctab[i].pdbr + (j * sizeof(pd_t));
			if(pd_address->pd_base - FRAME0 == i){
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
		frm_tab[i].fr_status = FRM_UNMAPPED;
		frm_tab[i].fr_pid = -1;
		frm_tab[i].fr_vpno = -1;
		frm_tab[i].fr_refcnt = 0;
		frm_tab[i].fr_type = FR_PAGE;
		frm_tab[i].fr_dirty = 0;
		restore(ps);
		return OK;
	}
	if(frm_tab[i].fr_type == FR_PAGE){
		int store;
			int pageth;
		fr_map_t *frame_no = &frm_tab[i];

		frm_tab[i].fr_status = FRM_UNMAPPED;
		frm_tab[i].fr_pid = -1;
		frm_tab[i].fr_vpno = 0;
		frm_tab[i].fr_refcnt = 0;
		frm_tab[i].fr_type = FR_PAGE;
		frm_tab[i].fr_dirty = 0;

		pid_of_this_fram_list *list = frame_no->list;
		while(list != NULL){
			int vpaddress = list->vpno;
			int pid = list->pid;
			
			pd_t *pd_address = proctab[pid].pdbr + (vpaddress/1024) * sizeof(pd_t);
			pt_t *pt_address = (pd_address->pd_base * NBPG) + vpaddress % 1024 * sizeof(pt_t);
			bsm_lookup(pid, list->vpno * 4096, &store, &pageth);
			write_bs((FRAME0 + i) * NBPG, store, pageth);
			pt_address->pt_pres = 0;
			pt_address->pt_write = 1;
			pt_address->pt_user = 0;
			pt_address->pt_pwt = 0;
			pt_address->pt_pcd = 0;
			pt_address->pt_acc = 0;
			pt_address->pt_dirty = 0;
			pt_address->pt_mbz = 0;
			pt_address->pt_global = 0;
			pt_address->pt_avail = 0;
			pt_address->pt_base = 0;
			unsigned long pt_frame_no = (unsigned long)pt_address/4096 - FRAME0;
			int re = --frm_tab[pt_frame_no].fr_refcnt;
			if(re == 0){
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
				fr_map_t *cur_frame = &frm_tab[pt_frame_no];
				cur_frame->fr_status = FRM_UNMAPPED;
				cur_frame->fr_pid = -1;
				cur_frame->fr_vpno = -1;
				cur_frame->fr_refcnt = 0;
				cur_frame->fr_type = FR_PAGE;
				cur_frame->fr_dirty = 0;
			}
			list = list->next;
		}
		if(grpolicy() == SC){
			sclist *dummy = scroot;
			sclist *moveout = sccur; 
			while(dummy->next != sccur){
				dummy = dummy->next;
			}
			dummy->next = moveout->next;
			sccur = moveout->next;
			if(moveout == scroot){
				if(scroot->next == scroot){
					scroot = NULL;
					sccur = NULL;
				}else{
					scroot = scroot->next;
				}
			}
			freemem(moveout,sizeof(sclist));
		}
		fr_map_t *nextnode = &frm_tab[i];
		pid_of_this_fram_list *currr = nextnode->list;
		pid_of_this_fram_list *currrnext = nextnode->list;
		while(currr!= NULL){
			currrnext = currr->next;
			freemem(currr, sizeof(pid_of_this_fram_list));
			currr = currrnext;
		}
		bsm_tab[store].bs_frame_no[pageth] = -1;
		restore(ps);
		return OK;
	}
  kprintf("To be implemented!\n");
  return OK;
}

int global_page_table(){
	int i = 0;
	int j = 0;
	for(i = 0; i < 4; i++){
		frm_tab[i].fr_status = FRM_MAPPED;
		frm_tab[i].fr_pid = NULLPROC;
		frm_tab[i].fr_vpno = -1;
		frm_tab[i].fr_refcnt = 0;
		frm_tab[i].fr_type = FR_TBL;
		frm_tab[i].fr_dirty = 0;
		for(j = 0; j < 1024; i++){
			pt_t *pt= (FRAME0 + i) * NBPG + (j*sizeof(pt_t));
			pt->pt_pres = 1;
			pt->pt_write = 1;
			pt->pt_user = 0;
			pt->pt_pwt = 0;
			pt->pt_pcd = 0;
			pt->pt_acc = 0;
			pt->pt_dirty = 0;
			pt->pt_mbz = 0;
			pt->pt_global = 0;
			pt->pt_avail = 0;
			pt->pt_base = j + i * 1024;
			frm_tab[i].fr_refcnt++;
		}

	}
	return OK;
}

int NULLPROC_page_directory(){
	int i = 0;
	frm_tab[4].fr_status = FRM_MAPPED;
	frm_tab[4].fr_pid = NULLPROC;
	frm_tab[4].fr_vpno = -1;
	frm_tab[4].fr_refcnt = 4;
	frm_tab[4].fr_type = FR_DIR;
	frm_tab[4].fr_dirty = 0;
	proctab[NULLPROC].pdbr = (FRAME0 + 4) * NBPG;
	unsigned long pdbr = proctab[NULLPROC].pdbr;
	for(i = 0; i< 1024; i++){
		pd_t *pd = pdbr + (i * sizeof(pd_t));
		if(i < 4){
			pd->pd_pres = 1;
			pd->pd_base = FRAME0 + i;
		}else{
			pd->pd_pres = 0;
			pd->pd_base = 0;
		}
		pd->pd_write = 1;
		pd->pd_user = 0;
		pd->pd_pwt = 0;
		pd->pd_pcd = 0;
		pd->pd_acc = 0;
		pd->pd_mbz = 0;
		pd->pd_fmb = 0;
		pd->pd_global = 0;
		pd->pd_avail = 0;
	}
	return OK;
}

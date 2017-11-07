/* pfint.c - pfint */

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

/*-------------------------------------------------------------------------
 * pfint - paging fault ISR
 *-------------------------------------------------------------------------
 */
SYSCALL pfint()
{
  STATWORD ps;
  disable(ps);
  
  // Obtain faulted address
  unsigned long fault_address = read_cr2();
  unsigned long vpno = fault_address / 4096;
  unsigned long pd_offset = fault_address >> 22;
  
  int store =-1, pageth=-1;
  pd_t *directory_table = proctab[currpid].pdbr + (pd_offset * sizeof(pd_t));
  if (bsm_lookup(currpid, fault_address, &store, &pageth) == SYSERR || store == -1 || pageth == -1) {
	kprintf("pfint.c: SYSERR - Backstore not mapped to virtual address.\n");
	kill(currpid);
	restore(ps);
	return SYSERR;
  }

  if (directory_table->pd_pres == 0) {
  		int i = 0;
  		int new_frame;
  		get_frm(&new_frame);
  		frm_tab[new_frame].fr_status = FRM_MAPPED;
		frm_tab[new_frame].fr_pid = currpid;
		frm_tab[new_frame].fr_vpno = -1;
		frm_tab[new_frame].fr_refcnt = 0;
		frm_tab[new_frame].fr_type = FR_TBL;
		frm_tab[new_frame].fr_dirty = 0;
	
		for (i = 0; i < 1024; i++) {
				pt_t *pt = (FRAME0 + new_frame) * NBPG + (i * sizeof(pt_t));
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
		
		directory_table->pd_pres = 1;
		directory_table->pd_write = 1;
		directory_table->pd_base = new_frame + FRAME0;
		unsigned long entry_index = (unsigned) directory_table;
		entry_index = entry_index / NBPG;
		entry_index -= FRAME0;
		
		frm_tab[entry_index].fr_refcnt++;
	}
	
	unsigned long pt_offset = (unsigned long) fault_address;
	pt_offset = pt_offset / NBPG;
	pt_offset = pt_offset & 0x000003ff;
	pt_t *table_entry = directory_table->pd_base * NBPG + pt_offset * sizeof(pt_t);
	
	int next_frame;
	if (bsm_tab[store].bs_frame_no[pageth] == -1) {
		get_frm(&next_frame);
		read_bs((FRAME0 + next_frame) * NBPG, store, pageth);
		bsm_tab[store].bs_frame_no[pageth] = next_frame;
		frm_tab[next_frame].fr_refcnt = 1;
		frm_tab[next_frame].fr_status = FRM_MAPPED;
	} else {
		next_frame = bsm_tab[store].bs_frame_no[pageth];
		frm_tab[next_frame].fr_refcnt ++;
	}
	
	if (next_frame == SYSERR) {
		kprintf("SYSERR - No frame available\n");
		kill(currpid);
		restore(ps);
		return SYSERR;
	}
	
	frm_tab[next_frame].fr_status = FRM_MAPPED;
	frm_tab[next_frame].fr_pid = currpid;
	frm_tab[next_frame].fr_vpno = vpno;
	frm_tab[next_frame].fr_type = FR_PAGE;
	
	if (grpolicy() == SC) {
		if (scroot == NULL) {
			scroot = getmem(sizeof(sclist));
			scroot->frame_no = FRAME0 + next_frame;
			scroot->next = scroot;
			sccur = scroot;
		} else {
			sclist *node = scroot;
			while (node->next != scroot) {
				node = node->next;
			}
			sclist *new_node = getmem(sizeof(sclist));
			new_node->frame_no = next_frame + FRAME0;
			new_node->next = scroot;
			node->next = new_node;
		}
	} 
	table_entry->pt_base = FRAME0 + next_frame;
	table_entry->pt_pres = 1;
	table_entry->pt_write = 1;
	int index = next_frame;
	frm_tab[index].fr_refcnt ++;
	fr_map_t *fr_ptr;
	  fr_ptr = &frm_tab[index];
	  
	  pid_of_this_fram_list *list = fr_ptr->list;
	  int flag = 1;
	  while (list->next != NULL) {
		  if (list->next->pid == currpid) {
			  flag = 0;
			  break;
		  }
		  list = list->next;
	  }
	  
	  if (flag) {
		  pid_of_this_fram_list *node = getmem(sizeof(pid_of_this_fram_list));
		  node->pid = currpid;
		  list->next = node;
	  }
	write_cr3(proctab[currpid].pdbr);
	
	restore(ps);
	return OK;
}



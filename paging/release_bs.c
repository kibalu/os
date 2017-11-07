#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>
bs_map_t bsm_tab[8];
SYSCALL release_bs(bsd_t bs_id) {
	
  /* release the backing store with ID bs_id */
	STATWORD ps;
	disable(ps);
	
	if (bs_id < 0 || bs_id > 7) {
		restore(ps);
		return SYSERR;
	}
	
	int i;
	for (i = 0; i < NPROC; i ++) {
		bs_no *data_list = NULL;
		bs_no *p; 
		struct pentry *pptr;
		pptr = &proctab[i];
		
		if (pptr->bshead == NULL)
			continue;
		
		bs_no *it;
		it = proctab[i].bshead;
		
		// The data list to call bsm_unmap
		while (it != NULL) {
			if (it->bs_id == bs_id) {
				if (data_list == NULL) {
					data_list = getmem(sizeof(bs_no));
					data_list->bs_id = bs_id;
					data_list->next = NULL;
					p = data_list;
				} else {
					bs_no *node = getmem(sizeof(bs_no));
					node->bs_id = bs_id;
					node->next = NULL;
					p->next = node;
					p = p->next;
				}
			}
			it = it->next;
		}
		
		if (data_list == NULL) {
			continue;
		} else {
			bs_no *z;
			z = data_list;
			
			while (z != NULL) {
				//bsm_unmap(int pid, int vpno, int flag)
				bsm_unmap(i, bsm_tab[z->bs_id].bs_vpno, 0);
				z = z->next;
			}
		}
		freemem(data_list, sizeof(bs_no));
	}
	
	free_bsm(bs_id);
	
	restore(ps);
	return OK;

}


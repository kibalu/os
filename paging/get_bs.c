#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>
bs_map_t bsm_tab[8];
int get_bs(bsd_t bs_id, unsigned int npages) {
	STATWORD ps;
	disable(ps);
	if(bs_id < 0 || bs_id > 7){
		kprintf("bs_id is smaller than 0 or bigger than 7");
		restore(ps);
		return SYSERR;
	}
	if(npages < 1 || npages > 256){
		kprintf("napges is smaller than 1 or bigger than 256");
		restore(ps);
		return SYSERR;
	}
	if(bsm_tab[bs_id].private == 1){
		kprintf("backingstore is private");
		restore(ps);
		return SYSERR;
	}
	if(bsm_tab[bs_id].bs_status == BSM_UNMAPPED){
		bsm_tab[bs_id].bs_status = BSM_MAPPED;
		bsm_tab[bs_id].bs_npages = npages;
		bsm_tab[bs_id].private = 0;
	}
  /* requests a new mapping of npages with ID map_id */
    restore(ps);
    return bsm_tab[bs_id].bs_npages;

}



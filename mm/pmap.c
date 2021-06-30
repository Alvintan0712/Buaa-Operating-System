#include "mmu.h"
#include "pmap.h"
#include "printf.h"
#include "env.h"
#include "error.h"


/* These variables are set by mips_detect_memory() */
u_long maxpa;            /* Maximum physical address */
u_long npage;            /* Amount of memory(in pages) */
u_long basemem;          /* Amount of base memory(in bytes) */
u_long extmem;           /* Amount of extended memory(in bytes) */

Pde *boot_pgdir;

struct Page *pages;
static u_long freemem;

static struct Page_list page_free_list;	    // Free list of physical pages
static int page_list[8];  // page in physical memory
static int swap_list[50]; // page in swap space

// lab2-challenge
int future_lock = 0;
int page_list_max = 8;
// inverted page table
const int Hsize = 1031;
struct iPage *ipages;
static struct iPage_list htable[1031];
static struct iPage_list ipage_free_list;
u_int nipage;

/* Overview:
 	Initialize basemem and npage.
 	Set basemem to be 64MB, and calculate corresponding npage value.*/
void mips_detect_memory()
{
    /* Step 1: Initialize basemem.
     * (When use real computer, CMOS tells us how many kilobytes there are). */
    maxpa = basemem = 1 << 26; // 64MB = 2^26
    extmem = 0;

    // Step 2: Calculate corresponding npage value.
    npage = PPN(basemem); // 1 page space have 4KB

    printf("Physical memory: %dK available, ", (int)(maxpa / 1024));
    printf("base = %dK, extended = %dK\n", (int)(basemem / 1024), (int)(extmem / 1024));
}

/* Overview:
 	Allocate `n` bytes physical memory with alignment `align`, if `clear` is set, clear the
 	allocated memory.
 	This allocator is used only while setting up virtual memory system.

   Post-Condition:
	If we're out of memory, should panic, else return this address of memory we have allocated.*/
static void *alloc(u_int n, u_int align, int clear)
{
    extern char end[];
    u_long alloced_mem;

    /* Initialize `freemem` if this is the first time. The first virtual address that the
     * linker did *not* assign to any kernel code or global variables. */
    if (freemem == 0) {
        freemem = (u_long)end; // end
    }

    /* Step 1: Round up `freemem` up to be aligned properly */
    freemem = ROUND(freemem, align);

    /* Step 2: Save current value of `freemem` as allocated chunk. */
    alloced_mem = freemem;

    /* Step 3: Increase `freemem` to record allocation. */
    freemem = freemem + n;

    // We're out of memory, PANIC !!
    if (PADDR(freemem) >= maxpa) {
        panic("out of memorty\n");
        return (void *)-E_NO_MEM;
    }

    /* Step 4: Clear allocated chunk if parameter `clear` is set. */
    if (clear) {
        bzero((void *)alloced_mem, n);
    }

    /* Step 5: return allocated chunk. */
    return (void *)alloced_mem;
}

/* Overview:
 	Get the page table entry for virtual address `va` in the given
 	page directory `pgdir`.
	If the page table is not exist and the parameter `create` is set to 1,
	then create it.*/
static Pte *boot_pgdir_walk(Pde *pgdir, u_long va, int create)
{
    Pde *pgdir_entryp = NULL;
    Pte *pgtable = NULL, *pgtable_entry = NULL;

    /* Step 1: Get the corresponding page directory entry and page table. */
    /* Hint: Use KADDR and PTE_ADDR to get the page table from page directory
     * entry value. */
    pgdir_entryp = pgdir + PDX(va);

    /* Step 2: If the corresponding page table is not exist and parameter `create`
     * is set, create one. And set the correct permission bits for this new page
     * table. */
    if (!(*pgdir_entryp & PTE_V)) { // if this page table not exist
        if (create) { 
            *pgdir_entryp = PADDR(alloc(BY2PG, BY2PG, 1)); // alloc a page directory
            *pgdir_entryp = (*pgdir_entryp) | PTE_V; // give the valid bit
        } 
        else return 0;
    }

    /* Step 3: Get the page table entry for `va`, and return it. */
    pgtable = (Pte *) KADDR(PTE_ADDR(*pgdir_entryp));
    pgtable_entry = pgtable + PTX(va);

    return pgtable_entry;
}

/*Overview:
 	Map [va, va+size) of virtual address space to physical [pa, pa+size) in the page
	table rooted at pgdir.
	Use permission bits `perm|PTE_V` for the entries.
 	Use permission bits `perm` for the entries.

  Pre-Condition:
	Size is a multiple of BY2PG.*/
void boot_map_segment(Pde *pgdir, u_long va, u_long size, u_long pa, int perm)
{
    int i;
    Pte *pgtable_entry;

    /* Step 1: Check if `size` is a multiple of BY2PG. */
    size = ROUND(size, BY2PG);

    /* Step 2: Map virtual address space to physical address. */
    /* Hint: Use `boot_pgdir_walk` to get the page table entry of virtual address `va`. */
    for (i = 0; i < size; i += BY2PG) {
        pgtable_entry = boot_pgdir_walk(pgdir, va + i, 1);
        *pgtable_entry = PTE_ADDR(pa + i) | perm | PTE_V;
    }
}

/* Overview:
    Set up two-level page table.

   Hint:  
    You can get more details about `UPAGES` and `UENVS` in include/mmu.h. */
void mips_vm_init()
{
    extern char end[];
    extern int mCONTEXT;
    extern struct Env *envs;

    Pde *pgdir;
    u_int n;

    /* Step 1: Allocate a page for page directory(first level page table). */
    pgdir = alloc(BY2PG, BY2PG, 1);
    printf("to memory %x for struct page directory.\n", freemem);
    mCONTEXT = (int)pgdir;

    boot_pgdir = pgdir;

    /* Step 2: Allocate proper size of physical memory for global array `pages`,
     * for physical memory management. Then, map virtual address `UPAGES` to
     * physical address `pages` allocated before. For consideration of alignment,
     * you should round up the memory size before map. */
    pages = (struct Page *)alloc(npage * sizeof(struct Page), BY2PG, 1);
    printf("to memory %x for struct Pages.\n", freemem);
    n = ROUND(npage * sizeof(struct Page), BY2PG);
    boot_map_segment(pgdir, UPAGES, n, PADDR(pages), PTE_R);

    /* Step 3, Allocate proper size of physical memory for global array `envs`,
     * for process management. Then map the physical address to `UENVS`. */
    envs = (struct Env *)alloc(NENV * sizeof(struct Env), BY2PG, 1);
    n = ROUND(NENV * sizeof(struct Env), BY2PG);
    boot_map_segment(pgdir, UENVS, n, PADDR(envs), PTE_R);

    printf("pmap.c:\t mips vm init success\n");
}

/*Overview:
 	Initialize page structure and memory free list.
 	The `pages` array has one `struct Page` entry per physical page. Pages
	are reference counted, and free pages are kept on a linked list.
  Hint:
	Use `LIST_INSERT_HEAD` to insert something to list.*/
void page_init(void)
{
    /* Step 1: Initialize page_free_list. */
    /* Hint: Use macro `LIST_INIT` defined in include/queue.h. */
    LIST_INIT(&page_free_list);

    /* Step 2: Align `freemem` up to multiple of BY2PG. */
    freemem = ROUND(freemem, BY2PG);

    /* Step 3: Mark all memory blow `freemem` as used(set `pp_ref`
     * filed to 1) */
    struct Page *page;
    u_long i;
    u_long page_size = PPN(PADDR(freemem));
    for (i = 0; i < page_size; i++) {
        pages[i].pp_ref = 1;
        pages[i].pp_lock = 0x3;
    }

    /* Step 4: Mark the other memory as free. */
    for (i; i < npage; i++) {
        pages[i].pp_ref = 0;
        LIST_INSERT_HEAD(&page_free_list, &pages[i], pp_link);
    }
}

/*Overview:
	Allocates a physical page from free memory, and clear this page.

  Post-Condition:
	If failed to allocate a new page(out of memory(there's no free page)),
 	return -E_NO_MEM.
	Else, set the address of allocated page to *pp, and returned 0.

  Note:
 	Does NOT increment the reference count of the page - the caller must do
 	these if necessary (either explicitly or via page_insert).

  Hint:
	Use LIST_FIRST and LIST_REMOVE defined in include/queue.h .*/
int page_alloc(struct Page **pp)
{
    struct Page *ppage_temp;

    /* Step 1: Get a page from free memory. If fails, return the error code.*/
    if (LIST_EMPTY(&page_free_list)) return -E_NO_MEM;

    /* Step 2: Initialize this page.
     * Hint: use `bzero`. */
    ppage_temp = LIST_FIRST(&page_free_list);
    LIST_REMOVE(ppage_temp, pp_link);
    bzero(page2kva(ppage_temp), BY2PG);
    *pp = ppage_temp;

    return 0;
}

/*Overview:
	Release a page, mark it as free if it's `pp_ref` reaches 0.
  Hint:
	When to free a page, just insert it to the page_free_list.*/
void page_free(struct Page *pp)
{
    /* Step 1: If there's still virtual address refers to this page, do nothing. */
    if (pp->pp_ref > 0) return;

    /* Step 2: If the `pp_ref` reaches to 0, mark this page as free and return. */
    if (pp->pp_ref == 0) {
        LIST_INSERT_HEAD(&page_free_list, pp, pp_link);
        return;
    }

    /* If the value of `pp_ref` less than 0, some error must occurred before,
     * so PANIC !!! */
    panic("cgh:pp->pp_ref is less than zero\n");
}

/*Overview:
 	Given `pgdir`, a pointer to a page directory, pgdir_walk returns a pointer
 	to the page table entry (with permission PTE_R|PTE_V) for virtual address 'va'.

  Pre-Condition:
	The `pgdir` should be two-level page table structure.

  Post-Condition:
 	If we're out of memory, return -E_NO_MEM.
	Else, we get the page table entry successfully, store the value of page table
	entry to *ppte, and return 0, indicating success.

  Hint:
	We use a two-level pointer to store page table entry and return a state code to indicate
	whether this function execute successfully or not.
    This function have something in common with function `boot_pgdir_walk`.*/
int pgdir_walk(Pde *pgdir, u_long va, int create, Pte **ppte)
{
    Pde *pgdir_entryp = NULL;
    Pte *pgtable = NULL;
    struct Page *ppage;

    /* Step 1: Get the corresponding page directory entry and page table. */
    pgdir_entryp = pgdir + PDX(va);

    /* Step 2: If the corresponding page table is not exist(valid) and parameter `create`
     * is set, create one. And set the correct permission bits for this new page
     * table.
     * When creating new page table, maybe out of memory. */
    if (!(*pgdir_entryp & PTE_V)) {
        if (create) {
            if (page_alloc(&ppage) == -E_NO_MEM) {
                return -E_NO_MEM;
            } else {
                ppage->pp_ref++;
                ppage->pp_lock |= future_lock;
                *pgdir_entryp = page2pa(ppage);
                *pgdir_entryp = (*pgdir_entryp) | PTE_V | PTE_R;
            }
        } else {
            *ppte = NULL;
            return 0;
        }
    }

    /* Step 3: Set the page table entry to `*ppte` as return value. */
    pgtable = (Pte *) KADDR(PTE_ADDR(*pgdir_entryp));
    *ppte = pgtable + PTX(va);

    return 0;
}

/*Overview:
 	Map the physical page 'pp' at virtual address 'va'.
 	The permissions (the low 12 bits) of the page table entry should be set to 'perm|PTE_V'.

  Post-Condition:
    Return 0 on success
    Return -E_NO_MEM, if page table couldn't be allocated

  Hint:
	If there is already a page mapped at `va`, call page_remove() to release this mapping.
	The `pp_ref `  should be incremented if the insertion succeeds.*/
int page_insert(Pde *pgdir, struct Page *pp, u_long va, u_int perm)
{
    u_int PERM = perm | PTE_V;
    Pte *pgtable_entry;

    if (pp->pp_lock) return -E_INVAL;

    /* Step 1: Get corresponding page table entry. */
    pgdir_walk(pgdir, va, 0, &pgtable_entry);

    if (pgtable_entry != 0 && (*pgtable_entry & PTE_V) != 0) {
        if (pa2page(*pgtable_entry) != pp) {
            // check physical page locked
            page_remove(pgdir, va);
        } else {
            tlb_invalidate(pgdir, va);
            *pgtable_entry = (page2pa(pp) | PERM);
            return 0;
        }
    } 

    /* Step 2: Update TLB. */
    /* hint: use tlb_invalidate function */
    tlb_invalidate(pgdir, va);

    /* Step 3: Do check, re-get page table entry to validate the insertion. */
    /* Step 3.1 Check if the page can be insert, if can’t return -E_NO_MEM */
    if (pgdir_walk(pgdir, va, 1, &pgtable_entry) == -E_NO_MEM) return -E_NO_MEM;

    /* Step 3.2 Insert page and increment the pp_ref   */
    *pgtable_entry = page2pa(pp) | PERM;
    pp->pp_ref++;
    pp->pp_lock |= future_lock;

    return 0;
}

/*Overview:
	Look up the Page that virtual address `va` map to.

  Post-Condition:
	Return a pointer to corresponding Page, and store it's page table entry to *ppte.
	If `va` doesn't mapped to any Page, return NULL.*/
struct Page *page_lookup(Pde *pgdir, u_long va, Pte **ppte) {
    struct Page *ppage;
    Pte *pte;

    /* Step 1: Get the page table entry. */
    pgdir_walk(pgdir, va, 0, &pte);

    /* Hint: Check if the page table entry doesn't exist or is not valid. */
    if (pte == 0) {
        return 0;
    }
    if ((*pte & PTE_V) == 0) {
        return 0;    //the page is not in memory.
    }

    /* Step 2: Get the corresponding Page struct. */

    /* Hint: Use function `pa2page`, defined in include/pmap.h . */
    ppage = pa2page(*pte);
    if (ppte) {
        *ppte = pte;
    }

    return ppage;
}

// Overview:
// 	Decrease the `pp_ref `  value of Page `*pp`, if `pp_ref `  reaches to 0, free this page.
void page_decref(struct Page *pp) {
    if(--pp->pp_ref   == 0) {
        page_free(pp);
    }
}

// Overview:
// 	Unmaps the physical page at virtual address `va`.
void page_remove(Pde *pgdir, u_long va)
{
    Pte *pagetable_entry;
    struct Page *ppage;

    /* Step 1: Get the page table entry, and check if the page table entry is valid. */
    ppage = page_lookup(pgdir, va, &pagetable_entry);

    if (ppage == 0) {
        return;
    } else if (ppage->pp_lock) { // locked page can't remove
        return;
    }

    /* Step 2: Decrease `pp_ref `  and decide if it's necessary to free this page. */
    /* Hint: When there's no virtual address mapped to this page, release it. */
    ppage->pp_ref--;
    if (ppage->pp_ref == 0) {
        page_free(ppage);
    }

    /* Step 3: Update TLB. */
    *pagetable_entry = 0;
    tlb_invalidate(pgdir, va);
    return;
}

// Overview:
// 	Update TLB.
void tlb_invalidate(Pde *pgdir, u_long va)
{
    if (curenv) {
        tlb_out(PTE_ADDR(va) | GET_ENV_ASID(curenv->env_id));
    } else {
        tlb_out(PTE_ADDR(va));
    }
}

void physical_memory_manage_check(void)
{
    struct Page *pp, *pp0, *pp1, *pp2;
    struct Page_list fl;
    int *temp;

    // should be able to allocate three pages
    pp0 = pp1 = pp2 = 0;
    assert(page_alloc(&pp0) == 0);
    assert(page_alloc(&pp1) == 0);
    assert(page_alloc(&pp2) == 0);

    assert(pp0);
    assert(pp1 && pp1 != pp0);
    assert(pp2 && pp2 != pp1 && pp2 != pp0);

    // temporarily steal the rest of the free pages
    fl = page_free_list;
    // now this page_free list must be empty!!!!
    LIST_INIT(&page_free_list);
    // should be no free memory
    assert(page_alloc(&pp) == -E_NO_MEM);

    temp = (int*)page2kva(pp0);
    //write 1000 to pp0
    *temp = 1000;
    // free pp0
    page_free(pp0);
    printf("The number in address temp is %d\n",*temp);

    // alloc again
    assert(page_alloc(&pp0) == 0);
    assert(pp0);

    // pp0 should not change
    assert(temp == (int*)page2kva(pp0));
    // pp0 should be zero
    assert(*temp == 0);

    page_free_list = fl;
    page_free(pp0);
    page_free(pp1);
    page_free(pp2);
    struct Page_list test_free;
    struct Page *test_pages;
	test_pages= (struct Page *)alloc(10 * sizeof(struct Page), BY2PG, 1);
	LIST_INIT(&test_free);
	//LIST_FIRST(&test_free) = &test_pages[0];
	int i, j = 0;
	struct Page *p, *q;
	//test inert tail
	for (i = 0; i < 10; i++) {
		test_pages[i].pp_ref = i;
		//test_pages[i].pp_link=NULL;
		//printf("0x%x  0x%x\n",&test_pages[i], test_pages[i].pp_link.le_next);
		LIST_INSERT_TAIL(&test_free,&test_pages[i],pp_link);
		//printf("0x%x  0x%x\n",&test_pages[i], test_pages[i].pp_link.le_next);
	}
	p = LIST_FIRST(&test_free);
	int answer1[] = {0,1,2,3,4,5,6,7,8,9};
	assert(p != NULL);
	while(p != NULL)
	{
		//printf("%d %d\n",p->pp_ref,answer1[j]);
		assert(p->pp_ref == answer1[j++]);
		//printf("ptr: 0x%x v: %d\n",(p->pp_link).le_next,((p->pp_link).le_next)->pp_ref);
		p = LIST_NEXT(p,pp_link);

	}
	// insert_after test
	int answer2[] = {0,1,2,3,4,20,5,6,7,8,9};
	q = (struct Page *)alloc(sizeof(struct Page), BY2PG, 1);
	q->pp_ref = 20;

	//printf("---%d\n",test_pages[4].pp_ref);
	LIST_INSERT_AFTER(&test_pages[4], q, pp_link);
	//printf("---%d\n",LIST_NEXT(&test_pages[4],pp_link)->pp_ref);
	p = LIST_FIRST(&test_free);
	j = 0;
	//printf("into test\n");
	while(p != NULL) {
	//      printf("%d %d\n",p->pp_ref,answer2[j]);
		assert(p->pp_ref == answer2[j++]);
		p = LIST_NEXT(p,pp_link);
	}

    printf("physical_memory_manage_check() succeeded\n");
}

void page_check(void)
{
    struct Page *pp, *pp0, *pp1, *pp2;
    struct Page_list fl;

    // should be able to allocate three pages
    pp0 = pp1 = pp2 = 0;
    assert(page_alloc(&pp0) == 0);
    assert(page_alloc(&pp1) == 0);
    assert(page_alloc(&pp2) == 0);

    assert(pp0);
    assert(pp1 && pp1 != pp0);
    assert(pp2 && pp2 != pp1 && pp2 != pp0);

    // temporarily steal the rest of the free pages
    fl = page_free_list;
    // now this page_free list must be empty!!!!
    LIST_INIT(&page_free_list);

    // should be no free memory
    assert(page_alloc(&pp) == -E_NO_MEM);

    // there is no free memory, so we can't allocate a page table
    assert(page_insert(boot_pgdir, pp1, 0x0, 0) < 0);

    // free pp0 and try again: pp0 should be used for page table
    page_free(pp0);
    assert(page_insert(boot_pgdir, pp1, 0x0, 0) == 0);
    assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));

    printf("va2pa(boot_pgdir, 0x0) is %x\n",va2pa(boot_pgdir, 0x0));
    printf("page2pa(pp1) is %x\n",page2pa(pp1));

    assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));
    assert(pp1->pp_ref == 1);

    // should be able to map pp2 at BY2PG because pp0 is already allocated for page table
    assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);
    assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));
    assert(pp2->pp_ref == 1);

    // should be no free memory
    assert(page_alloc(&pp) == -E_NO_MEM);

    printf("start page_insert\n");
    // should be able to map pp2 at BY2PG because it's already there
    assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);
    assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));
    assert(pp2->pp_ref == 1);

    // pp2 should NOT be on the free list
    // could happen in ref counts are handled sloppily in page_insert
    assert(page_alloc(&pp) == -E_NO_MEM);

    // should not be able to map at PDMAP because need free page for page table
    assert(page_insert(boot_pgdir, pp0, PDMAP, 0) < 0);

    // insert pp1 at BY2PG (replacing pp2)
    assert(page_insert(boot_pgdir, pp1, BY2PG, 0) == 0);

    // should have pp1 at both 0 and BY2PG, pp2 nowhere, ...
    assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));
    assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));
    // ... and ref counts should reflect this
    assert(pp1->pp_ref == 2);
    printf("pp2->pp_ref %d\n",pp2->pp_ref);
    assert(pp2->pp_ref == 0);
    printf("end page_insert\n");

    // pp2 should be returned by page_alloc
    assert(page_alloc(&pp) == 0 && pp == pp2);

    // unmapping pp1 at 0 should keep pp1 at BY2PG
    page_remove(boot_pgdir, 0x0);
    assert(va2pa(boot_pgdir, 0x0) == ~0);
    assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));
    assert(pp1->pp_ref == 1);
    assert(pp2->pp_ref == 0);

    // unmapping pp1 at BY2PG should free it
    page_remove(boot_pgdir, BY2PG);
    assert(va2pa(boot_pgdir, 0x0) == ~0);
    assert(va2pa(boot_pgdir, BY2PG) == ~0);
    assert(pp1->pp_ref == 0);
    assert(pp2->pp_ref == 0);

    // so it should be returned by page_alloc
    assert(page_alloc(&pp) == 0 && pp == pp1);

    // should be no free memory
    assert(page_alloc(&pp) == -E_NO_MEM);

    // forcibly take pp0 back
    assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));
    boot_pgdir[0] = 0;
    assert(pp0->pp_ref == 1);
    pp0->pp_ref = 0;

    // give free list back
    page_free_list = fl;

    // free the pages we took
    page_free(pp0);
    page_free(pp1);
    page_free(pp2);

    u_long* va = 0x12450;
    u_long* pa;

    // page_insert(boot_pgdir, pp, va, PTE_R);
    // pa = va2pa(boot_pgdir, va);
    // printf("va: %x -> pa: %x\n", va, pa);
    // *va = 0x88888;
    // printf("va value: %x\n", *va);
    // printf("pa value: %x\n", *((u_long *)((u_long)pa + (u_long)ULIM)));

    printf("page_check() succeeded!\n");
}

void pageout(int va, int context)
{
    u_long r;
    struct Page *p = NULL;

    if (context < 0x80000000) {
        panic("tlb refill and alloc error!");
    }

    if ((va > 0x7f400000) && (va < 0x7f800000)) {
        panic(">>>>>>>>>>>>>>>>>>>>>>it's env's zone");
    }

    if (va < 0x10000) {
        panic("^^^^^^TOO LOW^^^^^^^^^");
    }

    if ((r = page_alloc(&p)) < 0) {
        panic ("page alloc error!");
    }

    p->pp_ref++;

    page_insert((Pde *)context, p, VA2PFN(va), PTE_R);
    printf("pageout:\t@@@___0x%x___@@@  ins a page \n", va);
}

void lock(u_long addr, size_t len) {
    u_long i;
    Pte *pgtable_entry;
    struct Page *pp;
    
    for (i = 0; i < len; i += BY2PG) {
        pp = page_lookup(boot_pgdir, addr + i, &pgtable_entry);
        pp->pp_lock |= 0x1;
    }
}

void unlock(u_long addr, size_t len) {
    u_long i;
    Pte *pgtable_entry;
    struct Page *pp;

    for (i = 0; i < len; i += BY2PG) {
        pp = page_lookup(boot_pgdir, addr + i, &pgtable_entry);
        pp->pp_lock &= ~1;
    }
}

/* Overview:
 *  mlock() locks pages in the address range starting at addr
 *  and continuing for len bytes. All pages that contain
 *  a part of the specified address range are guaranteed
 *  to be resident in RAM when the call returns successfully;
 *  the pages are guaranteed to stay in RAM until later unlocked.
 * Post-Condition:
 *  On success return 0. 
 *  On error, return < 0.
 */
int mlock(u_long addr, size_t len) {
    u_long i;
    Pte *pgtable, *pgtable_entry;
    struct Page *ppage;

    len = ROUND(len, BY2PG); // align in 4KB
    for (i = 0; i < len; i += BY2PG) {
        // get the page
        ppage = page_lookup(boot_pgdir, addr + i, &pgtable_entry);
        if (ppage == NULL || ppage->pp_ref > 1) return -1;
    }
    lock(addr, len);

    return 0;
}

/* Overview:
 *  munlock() unlocks pages in the address range starting at addr and continuing
 *  for len bytes. After this call, all pages that contain a part of the specified 
 *  memory range can be moved to external swap space again by the kernel.
 * Post-Condition:
 *  On success return 0.
 *  On error, return < 0.
 */
int munlock(u_long addr, size_t len) {
    u_long i;
    Pte *pgtable, *pgtable_entry;

    len = ROUND(len, BY2PG);
    for (i = 0; i < len; i += BY2PG) {
        pgdir_walk(boot_pgdir, addr + i, 0, &pgtable_entry);
        if (pgtable_entry == NULL || !(*pgtable_entry & PTE_V)) 
            return -1;
    }
    unlock(addr, len);

    return 0;
}

void lockall_current() {
    int i;
    Pte *pte;
    for (i = 0; i < npage; i++) // travese all pages
        if (pages[i].pp_ref == 1) // if this page map on 1 virtual page
            pages[i].pp_lock |= 0x1;
}

/* Overview:
 *  mlockall() locks all pages mapped into the address space 
 *  of the calling process. This includes the pages of the code, 
 *  data and stack segment, as well as shared libraries, user space 
 *  kernel data, shared memory, and memory-mapped files. All mapped 
 *  pages are guaranteed to be resident in RAM when the call returns 
 *  successfully; the pages are guaranteed to stay in RAM until later 
 *  unlocked.
 * Post-Condition:
 *  On success return 0.
 *  On error, return < 0.
 */
int mlockall(int flags) {
    switch (flags) {
        case MCL_CURRENT:
            lockall_current();
            break;
        
        case MCL_FUTURE:
            future_lock = 1;
            break;
        
        default:
            return -E_INVAL;
    }
    return 0;
}

/* Overview:
 *  munlockall() unlocks all pages mapped into the address 
 *  space of the calling process.
 * Post-Condition:
 *  On success return 0.
 *  On error, return < 0.
 */
int munlockall(void) {
    int i;
    Pte *pte;
    for (i = 0; i < npage; i++) 
        pages[i].pp_lock &= ~1;

    return 0;
}

void lock_check() {
    int i, n;
    u_long va;
    Pte *pte;
    struct Page *pp, *pp0, *pp1, *pp2;
    extern char end[];

    assert(page_alloc(&pp0) == 0);
    assert(page_alloc(&pp1) == 0);
    assert(page_alloc(&pp2) == 0);

    assert(pp0);
    assert(pp1 && pp1 != pp0);
    assert(pp2 && pp2 != pp1 && pp2 != pp0);

    /********* initial test *********/
    printf("test ULIM\n");
    n = ROUND(end, BY2PG);
    for (va = ULIM; va < ULIM + n; va += BY2PG) {
        pp = page_lookup(boot_pgdir, va, &pte);
        assert(pp->pp_lock);
    }
    printf("test ULIM done\n");

    printf("test UPAGES ...\n");    
    n = ROUND(npage * sizeof(struct Page), BY2PG);
    for (va = UPAGES; va < UPAGES + n; va += BY2PG) {
        pp = page_lookup(boot_pgdir, va, &pte);
        assert(pp->pp_lock);
    }
    printf("test UPAGES done\n");

    printf("test UENVS ...\n");
    n = ROUND(NENV * sizeof(struct Env), BY2PG);
    for (va = UENVS; va < UENVS + n; va += BY2PG) {
        pp = page_lookup(boot_pgdir, va, &pte);
        assert(pp->pp_lock);
    }
    printf("test UENVS done\n");
    printf("initial pages are locked\n");
    /********* initial test END *********/

    /********* mlock munlock test *********/
    printf("mlock munlock test ...\n");
    /**** test mlock work ****/
    // alloc a page insert in physical memory
    assert(page_insert(boot_pgdir, pp0, 0x0, 0) == 0);

    // lock pp0
    assert(mlock(0x0, BY2PG) == 0);
    pp = page_lookup(boot_pgdir, 0x0, &pte);
    assert(pp->pp_lock);
    /**** mlock worked! ****/

    /**** test munlock work ****/
    // unlock pp0
    assert(munlock(0x0, BY2PG) == 0);
    pp = page_lookup(boot_pgdir, 0x0, &pte);
    assert(!pp->pp_lock);
    /**** munlock worked! ****/

    // remove page on physical memory
    page_remove(boot_pgdir, 0x0);

    /**** test mlock will not lock shared memory ****/
    assert(page_insert(boot_pgdir, pp, 0x0, 0) == 0);
    assert(page_insert(boot_pgdir, pp, BY2PG, 0) == 0);

    // when more than one virtual page map on physical
    assert(mlock(0x0, BY2PG) < 0);
    page_remove(boot_pgdir, BY2PG); // unmap BY2PG

    // relock again
    assert(mlock(0x0, BY2PG) == 0);
    // can't map new virtual page, because this page was locked
    assert(page_insert(boot_pgdir, pp, BY2PG, 0) == -E_INVAL); 

    assert(munlock(0x0, BY2PG) == 0);
    pp = page_lookup(boot_pgdir, 0x0, &pte);
    assert(!pp->pp_lock);

    page_remove(boot_pgdir, 0x0);
    /**** mlock don't lock shared memory ****/

    /**** test mlock just lock the pages one time ****/
    assert(page_insert(boot_pgdir, pp, 0x0, 0) == 0);
    assert(mlock(0x0, BY2PG) == 0);
    assert(mlock(0x0, BY2PG) == 0);
    assert(mlock(0x0, BY2PG) == 0);
    assert(pp == page_lookup(boot_pgdir, 0x0, &pte));
    assert(pp->pp_lock);
    assert(munlock(0x0, BY2PG) == 0);
    assert(!pp->pp_lock);
    page_remove(boot_pgdir, 0x0);
    /**** mlock just lock the pages one time ****/

    /**** test munlock will not unlock initial pages ****/
    assert(munlock(UPAGES, BY2PG) == 0);
    pp = page_lookup(boot_pgdir, UPAGES, &pte);
    assert(pp->pp_lock);
    /**** munlock will not unlock initial pages ****/
    printf("mlock munlock test Accepted!\n");
    /********* mlock munlock test END *********/

    /********* mlockall munlockall test *********/
    printf("mlockall munlockall test ...\n");
    /**** test mlockall current work ****/
    mlockall(MCL_CURRENT);
    for (i = 0; i < npage; i++) 
        if (pages[i].pp_ref == 1) assert(pages[i].pp_lock);
        else assert(!pages[i].pp_lock);
    /**** mlockall current worked! ****/

    /**** test munlockall work ****/
    munlockall();
    for (i = 0; i < npage; i++) 
        if (pages[i].pp_ref == 1) assert((pages[i].pp_lock & 1) == 0);
        else assert(!pages[i].pp_lock);
    // make sure initial pages locked
    n = ROUND(end, BY2PG);
    for (va = ULIM; va < ULIM + n; va += BY2PG) {
        pp = page_lookup(boot_pgdir, va, &pte);
        assert(pp->pp_lock);
    }
    n = ROUND(npage * sizeof(struct Page), BY2PG);
    for (va = UPAGES; va < UPAGES + n; va += BY2PG) {
        pp = page_lookup(boot_pgdir, va, &pte);
        assert(pp->pp_lock);
    }
    n = ROUND(NENV * sizeof(struct Env), BY2PG);
    for (va = UENVS; va < UENVS + n; va += BY2PG) {
        pp = page_lookup(boot_pgdir, va, &pte);
        assert(pp->pp_lock);
    }
    /**** munlockall worked! ****/
    assert(pp0->pp_ref == 0);
    
    /**** test mlockall future work ****/
    // now new alloc page will lock immediately
    mlockall(MCL_FUTURE);
    
    assert(page_insert(boot_pgdir, pp0, 0x0, 0) == 0);
    assert(page_insert(boot_pgdir, pp1, BY2PG, 0) == 0);
    assert(page_insert(boot_pgdir, pp2, 2*BY2PG, 0) == 0);

    assert(pp0->pp_lock);
    assert(pp1->pp_lock);
    assert(pp2->pp_lock);
    /**** mlockall future worked! ****/
    printf("mlockall munlockall test Accepted!\n");
    /********* mlockall munlockall test END *********/
    printf("lock check done!\n");
}

int size = 0, ptr = 0;
void pageInsert(struct Page *pp) {
    int i, j;
    if (pageExists(pp)) return;
    else if (size < page_list_max) {
        page_list[size++] = page2ppn(pp);
    } else {
        for (i = 0; i < size; i++) {
            struct Page *p = &pages[page_list[i]];
            if (!p->pp_lock) {
                for (j = i; j < size - 1; j++)
                    page_list[j] = page_list[j + 1];
                page_list[j] = page2ppn(pp);
                return;
            }
        }
        printf("all pages lock in physical pages\n");
    }
}

int pageExists(struct Page *pp) {
    int i;
    for (i = 0; i < size; i++) 
        if (page_list[i] == page2ppn(pp))
            return 1;
    return 0;
}

void page_replacement_check() {
    int i, j, data[50];
    struct Page *p;
    int merged_data[50] = {
        7515, 523,  2974, 1649, 5215, 
        4393, 7308, 2,    9157, 7024, 
        9584, 2082, 808,  2463, 4, 
        1,    6582, 802,  9553, 9333, 
        5721, 6578, 7135, 9266, 4, 
        2914, 1390, 5639, 1873, 3271, 
        8799, 4792, 8403, 9485, 3996, 
        2363, 1581, 8675, 5718, 4, 
        1464, 4180, 1210, 3,    9587, 
        2722, 7140, 6278, 4280, 7625
    };

    /**** normal testcase ****/
    printf("test normal testcase ...\n");
    for (i = 0; i < 50; i++) data[i] = npage - i - 1;
    for (i = 0; i < 50; i++) {
        pageInsert(&pages[data[i]]);
        printf("[ ");
        for (j = 0; j < size; j++, ptr = (ptr + 1) % size) 
            printf("%d ", page_list[ptr]);
        printf("]\n");
    }
    printf("normal testcase done\n");

    /**** all locked pages testcase ****/
    printf("test all locked pages testcase ...\n");
    ptr = size = 0;
    for (i = 0; i < 50; i++) data[i] = i;
    for (i = 0; i < 50; i++) {
        pageInsert(&pages[data[i]]);
        printf("[ ");
        for (j = 0; j < size; j++, ptr = (ptr + 1) % size) 
            printf("%d ", page_list[ptr]);
        printf("]\n");
    }
    printf("all locked pages testcase done\n");

    /**** merged testcase ****/
    printf("test merged testcase ...\n");    
    ptr = size = 0;
    for (i = 0; i < 50; i++) {
        pageInsert(&pages[merged_data[i]]);
        printf("[ ");
        for (j = 0; j < size; j++, ptr = (ptr + 1) % size) 
            printf("%d ", page_list[ptr]);
        printf("]\n");
    }
    printf("merged testcase done\n");
}

int hash(u_long va) {
    int vpn = VPN(va);
    return vpn % Hsize;
}

int find(u_long va, struct iPage **pp) {
    struct iPage *p;
    LIST_FOREACH(p, &htable[hash(va)], pp_link) {
        assert(p != LIST_NEXT(p, pp_link));
        if (VPN(va) == p->vpn) {
            *pp = p;
            return 1;
        }
    }
    *pp = NULL;
    return 0;
}

struct iPage* remove(u_long va) {
    struct iPage *pp;
    if (!find(va, &pp)) return NULL;
    LIST_REMOVE(pp, pp_link);
    pp->perm = 0;
    pp->vpn  = 0;
    return pp;
}

void insert(u_long va, struct iPage *pp) {
    struct iPage *p;
    remove(va); // if exists remove it
    pp->perm |= PTE_V;
    pp->vpn = VPN(va);
    LIST_INSERT_HEAD(&htable[hash(va)], pp, pp_link);
    assert(find(va, &p));
}

void ipage_init() {
    int i, n;

    LIST_INIT(&ipage_free_list);
    for (i = 0; i < Hsize; i++) LIST_INIT(&htable[i]);

    // init page size
    nipage = PPN(maxpa);

    // alloc memory for inverted page table
    ipages = (struct iPage *) alloc(nipage * sizeof(struct iPage *), BY2PG, 1);
 
    n = PPN(ROUND(nipage * sizeof(struct iPage), BY2PG));
    for (i = 0; i < n; i++) {
        ipages[i].vpn  = VPN(UPAGES) + i;
        ipages[i].perm = (PTE_V | PTE_R);
        LIST_INSERT_HEAD(&htable[ipages[i].vpn], ipages + i, pp_link);
    }

    for (i; i < nipage; i++) {
        ipages[i].vpn  = 0;
        ipages[i].perm = 0;
        LIST_INSERT_HEAD(&ipage_free_list, ipages + i, pp_link);
    }

    printf("inverted pages init success\n");
}

int ipage_alloc(struct iPage **pp) {
    if (LIST_EMPTY(&ipage_free_list)) return -E_NO_MEM;
    struct iPage *p = LIST_FIRST(&ipage_free_list);

    LIST_REMOVE(p, pp_link);
    p->vpn  = 0;
    p->perm = 0;
    *pp = p;

    return 0;
}

void ipage_free(struct iPage *pp) {
    if (pp->perm & PTE_V) return;
    LIST_INSERT_HEAD(&ipage_free_list, pp, pp_link);
}

int ipgdir_walk(u_long va, Pte **ppte) {
    struct Page *pp;
    
    if (find(va, &pp)) {
        *ppte = ipage2pa(pp);
        return 0;
    } else {
        *ppte = NULL;
        return -E_NOT_FOUND;
    }
}

int ipage_insert(struct iPage *pp, u_long va, u_int perm) {
    u_int PERM = perm | PTE_V;
    struct iPage *p;
    Pte *pte;
    int r;
    
    p = ipage_lookup(va, &pte);
    if (p && (p->perm & PTE_V) && p != pp) ipage_remove(va);
    if (pp->perm & PTE_V) ipage_remove(pp->vpn << PGSHIFT);
    
    pp->perm = PERM;
    pp->vpn  = VPN(va);
    insert(va, pp);

    return 0;
}

struct iPage* ipage_lookup(u_long va, Pte **ppte) {
    struct iPage *pp;
    int r;
    
    if (find(va, &pp)) *ppte = ipage2pa(pp);
    else *ppte = NULL;
    return pp;
}

void ipage_remove(u_long va) {
    struct iPage *pp = remove(va);
    pp->perm = 0;
    pp->vpn  = 0;
    ipage_free(pp);
}

u_long iva2pa(u_long va) {
    struct iPage *pp;
    if (!find(va, &pp)) {
        return ~0;
    }
    return ipage2pa(pp);
}

void printList(u_long va) {
    struct iPage *pp;
    printf("htable[%d] list:\n", hash(va));
    LIST_FOREACH(pp, &htable[hash(va)], pp_link)
        printf("%d ", pp - ipages);
    printf("\n");
}

void ipage_check() {
    struct iPage *pp, *pp0, *pp1, *pp2;
    struct iPage_list fl;
    u_long va1 = 0x0;
    u_long va2 = Hsize << PGSHIFT;
    u_long va3 = Hsize << (PGSHIFT + 1);
    Pte *pte;
    int i;

    /**** test page alloc ****/
    printf("test page alloc ...\n");
    pp0 = pp1 = pp2 = NULL;
    assert(ipage_alloc(&pp0) == 0);
    assert(ipage_alloc(&pp1) == 0);
    assert(ipage_alloc(&pp2) == 0);

    assert(pp0);
    assert(pp1 && pp1 != pp0);
    assert(pp2 && pp2 != pp1 && pp2 != pp0);

    printf("pp0 = %d\n", pp0 - ipages);
    printf("pp1 = %d\n", pp1 - ipages);
    printf("pp2 = %d\n", pp2 - ipages);

    fl = ipage_free_list;
    LIST_INIT(&ipage_free_list);

    assert(ipage_alloc(&pp) == -E_NO_MEM);
    printf("page alloc Accepted!\n");
    /**** page alloc worked! ****/

    /**** test hash table ****/
    printf("test hash table ...\n");
    insert(0x0, pp0);
    assert(find(0x0, &pp));
    assert(pp && pp == pp0);
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    insert(BY2PG, pp1);
    assert(find(BY2PG, &pp));
    assert(pp && pp == pp1);
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    insert(2*BY2PG, pp2);
    assert(find(2*BY2PG, &pp));
    assert(pp && pp == pp2);
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    assert(pp0 == remove(0x0));
    assert(remove(0x0) == NULL);
    assert(!find(0x0, &pp));
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    assert(pp1 == remove(BY2PG));
    assert(remove(0x0) == NULL);
    assert(!find(BY2PG, &pp));
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    assert(pp2 == remove(2*BY2PG));
    assert(remove(0x0) == NULL);
    assert(!find(2*BY2PG, &pp));
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    // test chain
    insert(va1, pp0);
    assert(find(va1, &pp));
    assert(pp && pp == pp0);
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    insert(va2, pp1);
    assert(find(va2, &pp));
    assert(pp && pp == pp1);
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    insert(va3, pp2);
    assert(find(va3, &pp));
    assert(pp && pp == pp2);
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    assert(pp1 == remove(va2));
    assert(remove(va2) == NULL);
    assert(!find(va2, &pp));
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    assert(pp0 == remove(va1));
    assert(remove(va1) == NULL);
    assert(!find(va1, &pp));
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    assert(pp2 == remove(va3));
    assert(remove(va3) == NULL);
    assert(!find(va3, &pp));
    printList(0x0);
    printList(BY2PG);
    printList(2*BY2PG);
    printf("-----------------------------\n");

    printf("hash table Accepted!\n");
    /**** hash table worked! ****/

    /**** test page insert and remove ****/
    printf("test page insert and remove ...\n");
    assert(ipage_insert(pp0, 0x0, 0) == 0);
    assert(pp0->perm & PTE_V);
    assert(iva2pa(0x0) == ipage2pa(pp0));

    assert(ipage_insert(pp1, 0x0, 0) == 0);
    assert(pp1->perm & PTE_V);
    assert(!(pp0->perm & PTE_V));
    assert(iva2pa(0x0) == ipage2pa(pp1));
    assert(iva2pa(0x0) != ipage2pa(pp0));

    assert(ipage_insert(pp0, 0x0, 0) == 0);
    assert(ipage_insert(pp0, BY2PG, 0) == 0);
    assert(iva2pa(0x0) == ~0);
    assert(iva2pa(BY2PG) == ipage2pa(pp0));

    ipage_remove(BY2PG);
    assert(iva2pa(BY2PG) == ~0);
    printf("page insert and remove Accepted!\n");
    /**** page insert and remove worked ****/

    /**** test pgdir walk ****/
    printf("test pgdir walk ...\n");
    assert(ipage_insert(pp0, 0x0, 0) == 0);
    assert(ipage_insert(pp1, BY2PG, 0) == 0);
    assert(ipage_insert(pp2, 2*BY2PG, 0) == 0);

    assert(ipgdir_walk(0x0, &pte) == 0);
    assert(pte == ipage2pa(pp0));
    assert(ipgdir_walk(BY2PG, &pte) == 0);
    assert(pte == ipage2pa(pp1));
    assert(ipgdir_walk(2*BY2PG, &pte) == 0);
    assert(pte == ipage2pa(pp2));

    ipage_remove(0x0);
    assert(ipgdir_walk(0x0, &pte) < 0);
    ipage_remove(BY2PG);
    assert(ipgdir_walk(BY2PG, &pte) < 0);
    ipage_remove(2*BY2PG);
    assert(ipgdir_walk(2*BY2PG, &pte) < 0);
    printf("pgdir walk Accepted!\n");
    /**** pgdir walk worked! ****/

    ipage_free_list = fl;
    printf("inverted pages test done!\n");
}

#define MAX_PHY_PAGE 64
#define MAX_PAGE 12
#define get_Page(x) (x>>MAX_PAGE)


void pageReplace(long *physic_memory, long nwAdd);

#include <env.h>
#include <pmap.h>
#include <printf.h>

/* Overview:
 *  Implement simple round-robin scheduling.
 *
 * Hints:
 *  1. The variable which is for counting should be defined as 'static'.
 *  2. Use variable 'env_sched_list', which is a pointer array.
 *  3. CANNOT use `return` statement!
 */
/*** exercise 3.14 ***/
void sched_yield(void)
{
    static int count = 0; // remaining time slices of current env
    static int point = 0; // current env_sched_list index
    
    /*  hint:
     *  1. if (count==0), insert `e` into `env_sched_list[1-point]`
     *     using LIST_REMOVE and LIST_INSERT_TAIL.
     *  2. if (env_sched_list[point] is empty), point = 1 - point;
     *     then search through `env_sched_list[point]` for a runnable env `e`, 
     *     and set count = e->env_pri
     *  3. count--
     *  4. env_run()
     *
     *  functions or macros below may be used (not all):
     *  LIST_INSERT_TAIL, LIST_REMOVE, LIST_FIRST, LIST_EMPTY
     */

    struct Env *e = curenv; // get the curenv
    if (e == NULL) { // curenv initial value is NULL
        int flag = 0;
        if (LIST_EMPTY(&env_sched_list[point])) point ^= 1; // if list empty change list
        LIST_FOREACH(e, &env_sched_list[point], env_sched_link) { // find the env that is ready
            if (e->env_status == ENV_RUNNABLE) {
                flag = 1;
                count = e->env_pri;
                break;
            }
        }
        if (!flag) { // if env_sched_list[point] don't have any env ready
            point ^= 1;
            LIST_FOREACH(e, &env_sched_list[point], env_sched_link) {
                if (e->env_status == ENV_RUNNABLE) {
                    count = e->env_pri;
                    break;
                }
            }
        }
    } else if (count == 0) { // change the e to another sched_list
        LIST_REMOVE(e, env_sched_link); // remove curenv
        LIST_INSERT_TAIL(&env_sched_list[1 - point], e, env_sched_link); // insert the env to list tail

        // find the new env
        int flag = 0;
        if (LIST_EMPTY(&env_sched_list[point])) point ^= 1; // if list empty change list
        LIST_FOREACH(e, &env_sched_list[point], env_sched_link) { // find the env that is ready
            if (e->env_status == ENV_RUNNABLE) {
                flag = 1;
                count = e->env_pri;
                break;
            }
        }
        if (!flag) {
            point ^= 1;
            LIST_FOREACH(e, &env_sched_list[point], env_sched_link) {
                count = e->env_pri;
                break;
            }
        }
    }

    assert(count > 0);
    assert(e != NULL);
    assert(e->env_status == ENV_RUNNABLE);

    count--;
    env_run(e);
}

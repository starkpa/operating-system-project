/*
Question - 
A number of cats and mice inhabit a house. The cats and mice have worked out a deal where the mice can steal pieces of the cats’ food, so long as the cats never see the mice actually doing so. If the cats see the mice, then the cats must eat the mice (or else lose face with all of their cat friends). There are NumBowls cat food dishes, NumCats cats, and NumMice mice. Your job is to synchronize the cats and mice so that the following requirements are satisfied:
No mouse should ever get eaten. You should assume that if a cat is eating at a food dish, any mouse attempting to eat from that dish or any other food dish will be seen and eaten. When cats aren’t eating, they will not see mice eating. In other words, this requirement states that if a cat is eating from any bowl, then no mouse should be eating from any bowl. Only one nor mice should starve. A cat or mouse that wants to eat should eventually be able to eat
. For example, a synchronization solution that permanently 
prevents all mice from eating would be unacceptable. When we actually 
test your solution, each simulated cat and mouse will only eat a finite 
number of times; however, even if the simulation were allowed to run 
forever, neither cats nor mice should starve. 

Solution - By doing process synchronization using mutex and semaphore we can avoide deadlock condition. 
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>    //provides a macro called assert which can be used to verify assumptions made by the program and print a diagnostic message if this assumption is false.
#include <errno.h>     //The errno.h header file of the C Standard Library defines the integer variable errno, which is set by system calls and some library functions in the event of an error to indicate what went wrong.
#include <pthread.h>

#define DISHES_N        2       // number of dishes we will allocate to cat a and mice //
#define N_CATS          6       // number of cats we have  //
#define N_MICE          2       // number of mice we have //

#define CAT_WAIT        15      // Max time in seconds a cat sleeps //
#define CAT_EAT         1       // how long in seconds a cat is eating //
#define CAT_N_EAT       4       // how many times a cat wants to eat //
#define MOUSE_WAIT      1       // max time in seconds a mouse sleeps //
#define MOUSE_EAT       1       // how long in seconds a mouse is eating //
#define MOUSE_N_EAT     4       // how many times a mouse wants to eat //

typedef struct dish 
{
    int free_dishes;            // how many dishes are free //
    int cats_eating;            // how many cats are eating at the moment //
    int mice_eating;            // how many mice are eating at the moment //
    int cats_waiting;           // how many cats are waiting for dish //
    
    enum 
    {
        none_eating,
        cat_eating,
        mouse_eating
    } status[DISHES_N];         // status of each dish //
    
    pthread_mutex_t mutex;      // mutex for accessing dish //
    pthread_cond_t free_cv;     // used to wait for a free dish //
    pthread_cond_t cat_cv;      // used to wait for coming cats //
} dish_t;

static const char *progname = "pets";

static void
dump_dish (const char *name, pthread_t pet, const char *what,dish_t *dish, int my_dish)
{
    int i;
    struct tm t;
    time_t tt;
    
    tt = time(NULL);
    assert(tt != (time_t) -1);
    localtime_r (&tt, &t);

    printf("%02d:%02d:%02d [", t.tm_hour, t.tm_min, t.tm_sec);
    
    for (i = 0; i < DISHES_N; i++) 
    {
        if (i) 
            printf(":");
        
        switch (dish->status[i]) 
        {
            case none_eating:
                printf("__");
                break;
            case cat_eating:
                printf("cat");
                break;
            case mouse_eating:
                printf("mouse");
                break;
        }
    }
    printf("] %s (id %x) %s eating from dish %d\n", name, pet, what, my_dish);
}

/*
     Cats wait for a dish to become free and when coming to the dish,
     they wait for the mice to run away from the dish (they are nice
     cats and want to eat dish rather than mice). Eating is simulated
     by a simple sleep().
 */

static void* 

cat(void *arg)
{
    dish_t *dish = (dish_t *) arg;
    int n = CAT_N_EAT;
    int my_dish = -1;
    int i;

    for (n = CAT_N_EAT; n > 0; n--) {

        pthread_mutex_lock(&dish->mutex);        // wait for a dish to become free //
       
        pthread_cond_broadcast(&dish->cat_cv);
        dish->cats_waiting++;
        
        while (dish -> free_dishes <= 0 || dish -> mice_eating > 0) 
        {
            pthread_cond_wait(&dish -> free_cv, &dish -> mutex);
        }
        
        dish -> cats_waiting--;

        // starting to eat - first check everything is fine //
        
        assert(dish -> free_dishes > 0);
        dish -> free_dishes--;
        assert(dish -> cats_eating < N_CATS);
        dish -> cats_eating++;
        
        // then find the first available free dish  //
        
        for (i = 0 ; i < DISHES_N && dish -> status[i] != none_eating ; i++) ;
        my_dish = i;
        assert(dish -> status[my_dish] == none_eating);
        dish -> status[my_dish] = cat_eating;
        dump_dish("cat", pthread_self(), "started", dish, my_dish);
        pthread_mutex_unlock (&dish  ->  mutex);

        // cats simply eat - mice avoid meeting cats for obvious reasons //
        sleep(CAT_EAT);
        
        // finished eating - check everything is fine and update state //
        pthread_mutex_lock (&dish -> mutex);
        assert(dish -> free_dishes < DISHES_N);
        dish -> free_dishes++;
        assert(dish -> cats_eating > 0);
        dish -> cats_eating--;
        dish -> status[my_dish] = none_eating;

        // finally  tell others we're done eating //
        pthread_cond_broadcast (&dish -> free_cv);
        dump_dish ("cat", pthread_self(), "finished", dish, my_dish);
        pthread_mutex_unlock (&dish -> mutex);

        // sleep some time to give others a chance to grab some food //
        
        sleep(rand() % CAT_WAIT);
    }

    return NULL;
}

/*
 * Mice wait for a dish to become free and cats go away. While eating,
 * they have to check that no cat is coming. Hence, a simple sleep()
 * cannot be used.
 */

static void* 

mouse(void *arg)
{
    dish_t *dish = (dish_t *) arg;
    int n = MOUSE_N_EAT;
    struct timespec ts;
    struct timeval tp;
    int my_dish;
    int i;

    for (n = MOUSE_N_EAT; n > 0; n--) 
    {

        pthread_mutex_lock(&dish -> mutex);
        // wait for a dish to become free and cats go away //
        
        while (dish -> free_dishes <= 0 || dish -> cats_eating > 0 || dish->cats_waiting > 0) 
        {
            pthread_cond_wait(&dish -> free_cv, &dish -> mutex);
        }

        // starting to eat - first check everything is fine // 
        
        assert(dish -> free_dishes > 0);
        dish -> free_dishes--;
        assert(dish -> cats_eating == 0);
        assert(dish -> mice_eating < N_MICE);
        dish -> mice_eating++;

        // then find the first available free dish //
        
        for (i = 0; i < DISHES_N && dish -> status[i] != none_eating; i++) ;
        my_dish = i;
        assert(dish -> status[my_dish] == none_eating);
        dish -> status[my_dish] = mouse_eating;
        dump_dish("mouse", pthread_self(), "started", dish, my_dish);
        pthread_mutex_unlock(&dish -> mutex);
        
        // eat - mice need to watch for cats becoming hungry //
        
        gettimeofday(&tp,NULL);
        ts.tv_sec  = tp.tv_sec;
        ts.tv_nsec = tp.tv_usec * 1000;
        ts.tv_sec += MOUSE_EAT;
        
        pthread_mutex_lock ( &dish -> mutex );
        pthread_cond_timedwait ( &dish -> cat_cv, &dish -> mutex, &ts);
        pthread_mutex_unlock ( &dish -> mutex );
        
        // finished eating - check everything is fine and update state //
        pthread_mutex_lock ( &dish -> mutex );
        assert( dish -> free_dishes < DISHES_N );
        dish -> free_dishes++;
        assert ( dish -> cats_eating == 0);
        assert (dish -> mice_eating > 0);
        dish -> mice_eating--;
        dish -> status[my_dish] = none_eating;

        // finally tell others we're done eating //
        pthread_cond_broadcast( &dish -> free_cv);
        dump_dish ("mouse", pthread_self(), "finished", dish, my_dish);
        pthread_mutex_unlock ( &dish -> mutex);
        
        // sleep some time to give others a chance to grab some food //
        sleep( rand() % MOUSE_WAIT);
    }

    return NULL;
}

/*
 * Initialize the shared data object, kick off cat and mice threads
 * and wait until they are done.
 */

int
main(int argc, char *argv[])
{
    int i, err;
    dish_t _dish, *dish;
    pthread_t cats [N_CATS];
    pthread_t mice [N_MICE];

    srand ( time(NULL) );  // initialize random number generator //

    dish = &_dish;
    memset ( dish, 0, sizeof(dish_t));
    dish -> free_dishes = DISHES_N;
    pthread_mutex_init ( &dish -> mutex, NULL);
    pthread_cond_init ( &dish -> free_cv, NULL);
    pthread_cond_init ( &dish -> cat_cv, NULL);
    
    // create cats //
    for (i = 0; i < N_CATS; i++) 
    {
        err = pthread_create ( &cats[i], NULL, cat, dish);
        if (err != 0) 
        {
            fprintf(stderr, "%s: %s: unable to create cat thread %d: %d\n",progname, __func__, i, err);
        }
    }

    // create mice //
    for (i = 0; i < N_MICE; i++) 
    {
        err = pthread_create (&mice[i], NULL, mouse, dish);
        if (err != 0) 
        {
            fprintf(stderr, "%s: %s: unable to create mouse thread %d: %d\n",progname, __func__, i, err);
        }
    }

    // join cat and mice //
    for (i = 0; i < N_CATS; i++) 
    {
        (void) pthread_join ( cats[i], NULL);
    }
    
    for (i = 0; i < N_MICE; i++) 
    {
        (void) pthread_join ( mice[i], NULL);
    }
    
    pthread_mutex_destroy ( &dish -> mutex);
    pthread_cond_destroy ( &dish -> free_cv);
    pthread_cond_destroy ( &dish -> cat_cv);
    
    return 0;
}

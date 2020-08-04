#include <pthread.h>

#include "console.h" // start the console thread
#include "error.h" // except 
#include "objects.h" // init, destroy objects

#include "renderer.h"

int main(int argc, char *argv[]) {
    const _Bool terminal_only = (argc > 1 && strcmp(argv[1], "term") == 0);

    objects_init();
    if (!terminal_only) window_init();

    // create the console thread
    volatile _Atomic _Bool sigquit = 0;
    pthread_t console_thread;
    if (pthread_create(&console_thread, NULL, console_start, (void*)&sigquit)) {
        EXCEPT("An error occured whilst creating the console thread\n");
    }

    if (terminal_only) 
        while (!sigquit);
    else
    while (!sigquit) {
        // handle events
        if (!window_update()) 
            break;

        window_draw();
    }

    pthread_cancel(console_thread);
    //pthread_join(console_thread, NULL);

    console_cleanup();
    objects_destroy();
    if (!terminal_only) window_destroy();

    fflush(stdout);
    //system(CLEAR);

    return 0;
}

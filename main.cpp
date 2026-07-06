#include <gtk/gtk.h>
#include "src/interface/gtk/window.h"

int main(int argc, char *argv[]) {
    create_main_window(argc, argv);
    gtk_main();
    
    return 0;
}

#include "simpsh.h"

int main(int argc, char **argv) {
  (void)argc;
  setlocale(LC_ALL, "");

  shell_loop(argv);
}

/*
* this is just a test lets see if this works
*/

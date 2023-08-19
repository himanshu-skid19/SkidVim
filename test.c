#include <unistd.h>
#include <stdio.h>
int main() {
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1);
  printf("done");
  return 0;
}







#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
int n = atoi(argv[1]);
for (int i = 0; i < n; i++) {
printf("%d\n", n);
sleep(1); // wait 1 second
}
return 0;
}



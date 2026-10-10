#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


struct Order {
    char **items;
    int count;
    int capacity;
};


int main(int argc, char *argv[]) {
	
	FILE *file = fopen(argv[1], "r");

	if (file == NULL){

	printf("Error: file could not open");
	return 1;	
	}
	
	char line[1024];
	struct Order order[100] = {0};


	while(fgets(line, sizeof(line),file) != NULL){
		char *token = strtok(line, " ");
		if (token == NULL){
			continue;
		}

		char *end;
		long  number = strtol(token, &end, 10);
		
		if (*end != '\0'){
			continue;
		}
		if (number < 0 || number >= 100) {
 			   continue;
			}

		token = strtok(NULL, "\n");
		
		if (token == NULL){
			continue;

		}	

		if (order[number].capacity == order[number].count){
			if (order[number].capacity == 0){
				order[number].capacity = 4;
			}else{
				order[number].capacity = order[number].capacity * 2; 	
			}
			order[number].items = realloc(
			order[number].items, order[number].capacity * sizeof(char *));
		}	
		
		order[number].items[order[number].count] = strdup(token);
		order[number].count++;
		}	

	 for (int i = 0; i < 100; i++) {
    if (order[i].count > 0) {
        printf("Order %d:\n", i);

        for (int j = 0; j < order[i].count; j++) {
            printf("  %s\n", order[i].items[j]);
        }
    }
}
		
	 for (int i = 0; i < 100; i++) {
    		for (int j = 0; j < order[i].count; j++) {
        		free(order[i].items[j]);
    			}

    		free(order[i].items);
	}

	fclose(file);
	return 0;

}

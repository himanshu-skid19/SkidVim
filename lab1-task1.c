#include <stdio.h>
#include <time.h>
#include <stdlib.h>


void printarray(int s[], int len);


int main(int argc, char *argv[]) {
    int len = atoi(argv[1]);
    int s[len];
    time_t t;
    srand((unsigned) time(&t));

    for (int i = 0; i<len;i++){
        s[i] = rand() % 5;
    }
    printarray(s, len);

    for (int i = 0; i<len; i++){
        for (int j = i+1; j<len; j++){
            if (s[i] == s[j]){
                s[j] = -1;
            }
        }
    }
    printf("\n");

    int l = len;
    for (int i = 0; i<len; i++){
        if (s[i] == -1){
            l--;
        }
    }

    int temp[l];
    int k = 0;
    for (int i = 0; i < len; i++){
        if (s[i] == -1){
            continue;
        }
        else{
            temp[k] = s[i];
            k++;
        }
    }


    printarray(temp, l);
}

void printarray(int s[], int len){
    for (int j =0; j<len; j++){
        printf("%d ", s[j]);
    }
}

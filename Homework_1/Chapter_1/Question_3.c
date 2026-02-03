#include <stdio.h>
#include <stdlib.h>

struct Node {
    char* data;
    struct Node* previous;
    struct Node* next;
};

void insert(struct Node** head, char* data) {
    // create new node
    struct Node* newNode;
    
    // allocate mem
    newNode = malloc(sizeof(struct Node));

    // allocate mem for data
    newNode->data = malloc(strlen(data) + 1);

    // copy the data
    strcpy(newNode->data, data);

    // set new node prev = none
    newNode->previous = NULL;
    newNode->next = *head;

    // if head 
    if (*head != NULL) {
        (*head)->previous = newNode;

    };

    *head = newNode;
}

void find(struct Node* head, char* target) {

}

void delete(struct Node** head, char* target) {

}

int main() {
    struct Node* head = NULL;
    return 0;
}

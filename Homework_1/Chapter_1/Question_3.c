// LARRY LA - CS 4080 - HW 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Node {
    char* data;
    struct Node* previous;
    struct Node* next;
};

void insert(struct Node** head, char* data) {
    struct Node* newNode = malloc(sizeof(struct Node));
    if (!newNode) return;

    newNode->data = malloc(strlen(data) + 1);
    if (!newNode->data) return;

    strcpy(newNode->data, data);

    newNode->previous = NULL;
    newNode->next = *head;

    if (*head != NULL) {
        (*head)->previous = newNode;
    }

    *head = newNode;   
}

struct Node* find(struct Node* head, char* target) {
    while (head != NULL) {
        if (strcmp(head->data, target) == 0) {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

void delete(struct Node** head, char* target) {
    struct Node* target_node = find(*head, target);

    if (target_node == NULL) return;

    if (target_node->previous != NULL) {
        target_node->previous->next = target_node->next;
    } else {
        *head = target_node->next;
    }

    if (target_node->next != NULL) {
        target_node->next->previous = target_node->previous;
    }

    free(target_node->data);
    free(target_node);
}

int main() {
    struct Node* head = NULL;     

    insert(&head, "5");
    insert(&head, "6");
    insert(&head, "7");

    struct Node* found = find(head, "6");
    if (found != NULL) {
        printf("Found node with data: %s\n", found->data);
    } else {
        printf("Node with data '6' not found.\n");
    }

    delete(&head, "6");

    found = find(head, "6");
    if (found != NULL) {
        printf("Found node with data: %s\n", found->data);
    } else {
        printf("Node with data '6' not found after deletion.\n");
    }

    struct Node* curr = head;
    while (curr != NULL) {
        printf("%s ", curr->data);
        curr = curr->next;
    }
    printf("\n");

    return 0;
}

// OUTPUT:
// Found node with data: 6
// Node with data '6' not found after deletion.
// 7 5 

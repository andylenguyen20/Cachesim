#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const int addressBits = 24;
//GLOBAL variables
int associativity;
int blockSize;

int offsetBits;
int indexBits;

const long memorySize = 16*(2<<19);
char mainMemory[memorySize];

struct Block{
	int tag;
  char* data;
	struct Block* prev;
	struct Block* next;
};

int ones(int n){
    return (1 << n) - 1;
}

//function used to free memory
void freeMemory(struct Block* cache[], int cacheSize){
	for(int i = 0; i < cacheSize; i = i + 1){
		struct Block* head = cache[i];
		while(head != NULL){
			struct Block* temp = head;
			head = head->next;
			if(temp!= NULL){
				free(temp->data);
				free(temp);
			}
		}
	}
}

//function used in load and store
void moveNodeToFront(struct Block* node, struct Block* cache[], int index){
	if(node->prev != NULL){
		struct Block** head = &(cache[index]);
		(*head)->prev = node;
		node->next->prev = node->prev;
		node->prev->next = node->next;
		node->prev = NULL;
		node->next = *head;
		*head = node;
	}
}

void moveTailToFront(struct Block* tail, struct Block* cache[], int index){
	if(tail->prev != NULL){
		struct Block** head = &(cache[index]);
		(*head)->prev = tail;
	  tail->next = *head;
	  tail->prev->next = NULL;
	  tail->prev = NULL;
	  *head = tail;
	}
}

//functions used only in store
void writeBlockFromArray(struct Block* node, int blockOffset, int sizeOfAccess, char writeByteArray[]){
	int byteIndex = 0;
	for(int i = blockOffset; i < blockOffset + sizeOfAccess; i = i + 1){
		node->data[i] = writeByteArray[byteIndex];
		byteIndex = byteIndex + 1;
	}
}

/*
void writeMemoryFromArray(int accessAddress, int sizeOfAccess, char writeByteArray[]){
	int byteIndex = 0;
	for(long i = accessAddress; i < accessAddress + sizeOfAccess; i = i + 1){
		mainMemory[i] = writeByteArray[byteIndex];
		byteIndex = byteIndex + 1;
	}
}
*/
void writeMemoryFromArray(int accessAddress, int sizeOfAccess, char writeByteArray[]){
	int memIndex = accessAddress;
	for(int i = 0; i < sizeOfAccess; i = i + 1){
		mainMemory[memIndex] = writeByteArray[i];
		memIndex = memIndex + 1;
	}
}


//functions used only in load
void writeBlockFromMemory(struct Block* node, int startingAddress, int blockSize){
	int dataIndex = 0;
	for(int i = startingAddress; i < startingAddress + blockSize; i++){
		 node->data[dataIndex] = mainMemory[i];
		 dataIndex = dataIndex + 1;
	}
}

void printLoadedInfo(int accessAddress, int hit, int blockOffset, int sizeOfAccess, struct Block* node){
	if(hit == 0){
		printf("load 0x%x miss ", accessAddress);
	}else{
		printf("load 0x%x hit ", accessAddress);
	}
	for(int i = blockOffset; i < blockOffset + sizeOfAccess; i = i + 1){
		printf("%02x", (unsigned char) node->data[i]);
	}
	printf("\n");
}


void store(int accessAddress, int sizeOfAccess, char writeByteArray[], struct Block* cache[]){
  int blockOffset = accessAddress & ones(offsetBits);
	int index = (accessAddress >> offsetBits) & ones(indexBits);
	int tag = accessAddress >> (offsetBits + indexBits);

	struct Block* head = cache[index];
	//MISS, this set hasn't been touched yet. Just write to memory
	if(head == NULL){
		writeMemoryFromArray(accessAddress, sizeOfAccess, writeByteArray);
		printf("store 0x%x miss\n", accessAddress);
	}else{
		struct Block* curr = head;
		while(true){
			//hit! Time to write to cache and rearrange pointers
			if(curr->next != NULL && curr->tag == tag){
				writeBlockFromArray(curr, blockOffset, sizeOfAccess, writeByteArray);
				moveNodeToFront(curr, cache, index);
				printf("store 0x%x hit\n", accessAddress);
				break;
			}
			if(curr->next == NULL){
				if(curr->tag == tag){
					//hit on tail
					writeBlockFromArray(curr, blockOffset, sizeOfAccess, writeByteArray);
					moveTailToFront(curr, cache, index);
					printf("store 0x%x hit\n", accessAddress);
				}else{
					//miss
					printf("store 0x%x miss\n", accessAddress);
				}
				break;
			}
			curr = curr->next;
		}
		//write to memory regardless of whether it was a hit or miss
		writeMemoryFromArray(accessAddress, sizeOfAccess, writeByteArray);
	}
}

void load(int accessAddress, int sizeOfAccess, struct Block* cache[]){
	int blockOffset = accessAddress & ones(offsetBits);
	int index = (accessAddress >> offsetBits) & ones(indexBits);
	int tag = accessAddress >> (offsetBits + indexBits);

	int startingAddress = (tag<<(indexBits+offsetBits)) | (index<<(offsetBits));

	struct Block* head = cache[index];
	//MISS, this set hasn't been touched yet. Take from memory
	if(head == NULL){
		cache[index] = (struct Block*)malloc(sizeof(struct Block));
		struct Block* head = cache[index];
		head->tag = tag;
		head->prev = NULL;
		head->next = NULL;
		head->data = (char*)malloc(blockSize);
		writeBlockFromMemory(head, startingAddress, blockSize);
		printLoadedInfo(accessAddress, 0, blockOffset, sizeOfAccess, head);
	}else{
		struct Block* curr = head;
		int nodesEncountered = 0;
		while(true){
			nodesEncountered = nodesEncountered + 1;
			//hit! Time to load data and rearrange pointers
			if(curr->next != NULL && curr->tag == tag){
				printLoadedInfo(accessAddress, 1, blockOffset, sizeOfAccess, curr);
				moveNodeToFront(curr, cache, index);
				break;
			}
			if(curr->next == NULL){
				if(curr->tag == tag){
					//hit on tail
					printLoadedInfo(accessAddress, 1, blockOffset, sizeOfAccess, curr);
					moveTailToFront(curr, cache, index);
					break;
				}else{
					//miss! Time to add a node to the cache, use LRU algorithm if numNodes == associativity
					if(nodesEncountered == associativity){
						//curr is currently the tail. Just change tag and data and move to front if not already head
						curr->tag = tag;
						writeBlockFromMemory(curr, startingAddress, blockSize);
						moveTailToFront(curr, cache, index);
						printLoadedInfo(accessAddress, 0, blockOffset, sizeOfAccess, curr);
					}else{//add new head node with malloc
						struct Block* node = (struct Block*)malloc(sizeof(struct Block));
						node->tag = tag;
						node->next = NULL;
						curr->next = node;
						node->prev = curr;
						node->data = (char*)malloc(blockSize);
						writeBlockFromMemory(node, startingAddress, blockSize);
						moveTailToFront(node, cache, index);
						printLoadedInfo(accessAddress, 0, blockOffset, sizeOfAccess, node);
					}
					break;
				}
			}
			curr = curr->next;
		}
	}
}

int main(int argc, char* argv[]){
	//get stuff ready for reading file and using fgets
	char* fileName = argv[1];
	FILE *fr = fopen(fileName,"rt");
  int cacheSizeKB = atoi(argv[2]);
  associativity = atoi(argv[3]);
  blockSize = atoi(argv[4]);
	const int sets = cacheSizeKB * (2<<9) / blockSize / associativity;

	//initialize global variables
  offsetBits = log2(blockSize);
	indexBits = log2(sets);

	//initialize cache and mainMemory here. VERY IMPORTANT!!!
	struct Block* cache[sets - 1];
	for(int i = 0; i < sets; i = i + 1){
		cache[i] = NULL;
	}
	for(long i = 0; i < memorySize; i++){
		mainMemory[i] = 0;
	}

	//perform instructions in a while loop until file ends
  char instruction[6];
  while(fscanf(fr, "%s", instruction) != EOF){
		if(strcmp(instruction,"load") == 0){
			//get address
			char accessAddressHex[9];
			fscanf(fr, "%s", accessAddressHex);
			int accessAddress = (int)strtol(accessAddressHex, NULL, 16);
			//get size of address
			int sizeOfAccess;
			fscanf(fr, "%d", &sizeOfAccess);
      load(accessAddress, sizeOfAccess, cache);
    }else if(strcmp(instruction,"store") == 0){
			//get address
			char accessAddressHex[9];
      fscanf(fr, "%s", accessAddressHex);
      int accessAddress = (int)strtol(accessAddressHex, NULL, 16);
			//get size of address
			int sizeOfAccess;
      fscanf(fr, "%d", &sizeOfAccess);
      char writeByteArray[sizeOfAccess];
			//get bytes to be written to memory and maybe also to cache
			char byte;
      for(int i = 0; i < sizeOfAccess; i = i + 1){
        fscanf(fr, "%2hhx", &byte);
				writeByteArray[i] = byte;
      }
      store(accessAddress, sizeOfAccess, writeByteArray, cache);
    }else{
      return EXIT_FAILURE;
    }
  }
	freeMemory(cache, sets);
	fclose(fr);
	return EXIT_SUCCESS;
}

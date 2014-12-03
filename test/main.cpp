#include "redsea.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void print(RedSeaDirectory *dir, int indent = 0) {
	for (int i = 0; i < dir->CountEntries(); i++) {
		RedSeaDirEntry *entry = dir->GetEntry(i);
		if (entry == 0)
			continue;

		if (strcmp(entry->GetName(), "..") == 0)
			printf("%.*sSpecial dir '%s'\n", indent, "\t", entry->GetName());
		else if (entry->IsDirectory()) {
			RedSeaDirectory *dir2 = static_cast<RedSeaDirectory *>(entry);
			printf("%.*sDirectory '%s': %d files\n", indent, "\t", dir2->GetName(), dir2->CountEntries());
			print(dir2, indent + 1);
		} else {
			printf("%.*sFile '%s'\n", indent, "\t", entry->GetName());	
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return 2;

	RedSea r(fopen(argv[1], "rb"));
	printf("Sector Offset: 0x%016X\n", r.BaseOffset());

	RedSeaDirectory *d = r.RootDirectory();

	print(d, 0);

	int i;
	printf("\nAllocation Test:\n");
	srand(1234);
	for (i = 0; i < 10; i++) {
		int ij = rand() % 1234;
		printf("\tAllocating %d sector(s) ->\t", ij);
		uint64_t alloc = r.Allocate(ij);
		printf("0x%016X\n", alloc);
	}
	return 0;
}

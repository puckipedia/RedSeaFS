#include "redsea.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void print(RedSeaDirectory *dir, int indent = 0) {
	for (int i = 0; i < dir->CountEntries(); i++) {
		RedSeaDirEntry *entry = dir->GetEntry(i);
		if (entry == 0)
			continue;

		if (strcmp(entry->Name(), "..") == 0)
			printf("%.*sSpecial dir '%s'\n", indent, "\t", entry->Name());
		else if (entry->IsDirectory()) {
			RedSeaDirectory *dir2 = static_cast<RedSeaDirectory *>(entry);
			printf("%.*sDirectory '%s': %d files\n", indent, "\t", dir2->Name(), dir2->CountEntries());
			print(dir2, indent + 1);
		} else {
			printf("%.*sFile '%s' (size %d)\n", indent, "\t", entry->Name(), entry->DirEntry().mSize);
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return 2;

	RedSea r(fopen(argv[1], "r+b"));
	printf("Sector Offset: 0x%016X\n", r.BaseOffset());

	RedSeaDirectory *d = r.RootDirectory();

	for (int i = d->CountEntries() - 1; i >= 0; i--) {
		RedSeaDirEntry *ent = d->GetEntry(i);
		if (strcmp(ent->Name(), "Testing") == 0) {
			ent->Delete();
			ent->Flush();
			delete ent;
		}
	}

	print(d, 0);

	char data[] = "testing 1234";

	d->CreateFile("Testing", 0x200)->Write(0, sizeof(data), data);

	r.FlushBitmap();
	return 0;
}

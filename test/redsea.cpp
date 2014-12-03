#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

class RedSeaDirectory;
class RedSeaDirEntry;

struct RSBoot {
	uint8_t jump_and_nop[3];
	uint8_t signature;
	uint8_t reserved[4];
	uint64_t base_offset;
	uint64_t count;
	uint64_t root_sector;
	uint64_t bitmap_sectors;
	uint64_t unique_id;
	uint8_t code[462];
	uint16_t signature2;
};

class RedSea {
public:
				RedSea(FILE *f);
	RedSeaDirectory *	RootDirectory();
	uint64_t		BaseOffset() { return mBoot.base_offset; }
	uint64_t		FirstFreeSector(int count);
	uint64_t		Allocate(int count);
private:
	friend class RedSeaDirEntry;
	friend class RedSeaFile;
	friend class RedSeaDirectory;
	FILE *mFile;
	RSBoot mBoot;
	uint8_t *mBitmapSectors;
	uint64_t mBitmapLength;
	void			Read(uint64_t location, uint64_t count, void *result);
};

RedSea::RedSea(FILE *f)
{
	mFile = f;
	Read(0, 0x200, &mBoot);
	mBitmapLength = mBoot.bitmap_sectors * 0x200;
	mBitmapSectors = new uint8_t[mBitmapLength];
	Read(0x200, mBitmapLength * 0x200, mBitmapSectors);
}

//#define UINT64_MAX 0xFFFFFFFFFFFFFFFF

uint64_t
RedSea::FirstFreeSector(int count)
{
	//printf("Finding %d sectors next to each other...\n");
	int i;
	uint64_t start = UINT64_MAX;
	int ccount = 0;
	for (i = 0; i < mBitmapLength; i++) {
		int bi, ci;
		for (bi = 1, ci = 0; ci < 8; bi <<= 1, ci++) {
			if (!(mBitmapSectors[i] & bi)) {
				//printf("Found free sector 0x%016X-%d\n", i, ci);
				if (start == UINT64_MAX) {
					//printf("\tStart updated\n");
					start = (i << 3) | ci;
				}
				ccount++;
				if (ccount >= count)
					return start;
				//printf("Count is now %d\n", ccount);
			} else {
				//printf("Full sector, 0x%016X-%d - %d > %d?\n", i, ci, ccount, count);
				if (ccount >= count)
					return start;
				start = ~0;
			}
		}
	}

	if (ccount >= count)
		return start;

	return ~0;
}

uint64_t
RedSea::Allocate(int count)
{
	uint64_t firstFree = FirstFreeSector(count);
	if (firstFree == UINT64_MAX)
		return firstFree;

	uint64_t start = firstFree;
	uint64_t end = firstFree + count - 1; // inclusive!

	int startbit = start % 8;
	int endbit = end % 8;

	start >>= 3;
	end >>= 3;

	if (start == end) {
		for (int i = startbit; i <= endbit; i++) {
			printf("Set 0x%016X %d to 1\n", start, i);
			mBitmapSectors[start] |= (1 << i);
		}
	} else {
		if (startbit == 0) {
			mBitmapSectors[start] = 0xFF;
		} else {
			for (int i = startbit; i < 8; i++) {
				mBitmapSectors[start] |= 1 << i;
			}
		}

		if (endbit == 0) {
			memset(mBitmapSectors + start + 1, 0xFF, end - start + 1);
		} else {
			memset(mBitmapSectors + start + 1, 0xFF, end - start);
			for (int i = 0; i <= endbit; i++) {
				mBitmapSectors[end] |= 1 << i;
			}
		}

		return firstFree;
	}

	return firstFree;
}

void
RedSea::Read(uint64_t location, uint64_t count, void *result) {
	fseek(mFile, location, SEEK_SET);
	fread(result, count, 1, mFile);
}


class RedSeaDateTime {
public:
				RedSeaDateTime();
				RedSeaDateTime(uint32_t, uint32_t);
				RedSeaDateTime(uint64_t);
private:
	uint32_t mDaysSinceChrist;
	uint32_t mTicks; // 49710Hz
} __attribute__((packed));


RedSeaDateTime::RedSeaDateTime()
{

}

RedSeaDateTime::RedSeaDateTime(uint32_t days, uint32_t ticks)
{
	mDaysSinceChrist = days;
	mTicks = ticks;
}


RedSeaDateTime::RedSeaDateTime(uint64_t value)
{
	mDaysSinceChrist = value >> 32;
	mTicks = value & 0xFFFFFFFF;
}

struct RSDirEntry {
	uint16_t mAttributes;
	char mName[38];
	uint64_t mCluster;
	uint64_t mSize;
	RedSeaDateTime mDateTime;
} __attribute__((packed));


class RedSeaDirEntry {
public:
				RedSeaDirEntry(RedSea *, uint64_t);
	bool			IsDirectory() const { return mDirEntry.mAttributes & 0x10; }
	bool			IsFile() const { return !IsDirectory(); }
	const char *		GetName() const { return mDirEntry.mName; }
protected:
	RSDirEntry mDirEntry;
	RedSea *mRedSea;
} __attribute__((packed));


RedSeaDirEntry::RedSeaDirEntry(RedSea *rs, uint64_t location)
{
	rs->Read(location, sizeof(RSDirEntry), &mDirEntry);
	mDirEntry.mCluster -= rs->BaseOffset();
	mRedSea = rs;
}


class RedSeaFile : public RedSeaDirEntry {
public:
				RedSeaFile(RedSea *, uint64_t);
	void			Read(uint64_t start, uint64_t count, void *result);
};


RedSeaFile::RedSeaFile(RedSea *rs, uint64_t location)
	: RedSeaDirEntry(rs, location)
{

}


void
RedSeaFile::Read(uint64_t start, uint64_t count, void *result)
{
	if (start > mDirEntry.mSize || start + count > mDirEntry.mSize)
		return;
	mRedSea->Read(start + (mDirEntry.mCluster * 0x200), count, result);
}


class RedSeaDirectory : public RedSeaDirEntry {
public:
				RedSeaDirectory(RedSea *, uint64_t);
	int			CountEntries() { return mEntryCount - 1; }
	RedSeaDirEntry *	GetEntry(int i);
	RedSeaDirectory *	Self() { return (RedSeaDirectory *)GetEntry(-1); }
protected:
	int mEntryCount;
};


RedSeaDirectory::RedSeaDirectory(RedSea *rs, uint64_t location)
	: RedSeaDirEntry(rs, location)
{
	mEntryCount = mDirEntry.mSize / 64;
}

RedSeaDirEntry *
RedSeaDirectory::GetEntry(int i)
{
	// First entry is itself!
	i++;

	if (i >= mEntryCount)
		return nullptr;

	uint64_t base = mDirEntry.mCluster * 0x200 + i * 64;
	uint16_t attributes;
	mRedSea->Read(base, 2, &attributes);

	if (attributes == 0x0000) {
		return nullptr;
		mEntryCount = i;
	}

	if (attributes & 0x10) {
		return new RedSeaDirectory(mRedSea, base);
	} else {
		return new RedSeaFile(mRedSea, base);
	}
}

RedSeaDirectory *
RedSea::RootDirectory()
{
	return new RedSeaDirectory(this, (mBoot.root_sector - mBoot.base_offset) * 0x200);
}


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
	srand(1234);
	for (i = 0; i < 10; i++) {
		int ij = rand() % 1234;
		printf("Allocating %d sector(s): \n", ij);
		uint64_t alloc = r.Allocate(ij);
		printf("\tGot 0x%016X\n", alloc);
	}
	return 0;
}

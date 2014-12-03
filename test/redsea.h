#include <cstdint>
#include <cstdio>

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

class RedSeaDateTime {
public:
				RedSeaDateTime();
				RedSeaDateTime(uint32_t, uint32_t);
				RedSeaDateTime(uint64_t);
private:
	uint32_t mDaysSinceChrist;
	uint32_t mTicks; // 49710Hz
} __attribute__((packed));


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

class RedSeaFile : public RedSeaDirEntry {
public:
				RedSeaFile(RedSea *, uint64_t);
	void			Read(uint64_t start, uint64_t count, void *result);
};

class RedSeaDirectory : public RedSeaDirEntry {
public:
				RedSeaDirectory(RedSea *, uint64_t);
	int			CountEntries() { return mEntryCount - 1; }
	RedSeaDirEntry *	GetEntry(int i);
	RedSeaDirectory *	Self() { return (RedSeaDirectory *)GetEntry(-1); }
protected:
	int mEntryCount;
};



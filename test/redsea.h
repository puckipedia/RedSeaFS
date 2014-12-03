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
	void			Deallocate(uint64_t, int);
	void			FlushBitmap();
private:
	friend class RedSeaDirEntry;
	friend class RedSeaFile;
	friend class RedSeaDirectory;
	FILE *mFile;
	RSBoot mBoot;
	uint8_t *mBitmapSectors;
	uint64_t mBitmapLength;
	void			Read(uint64_t location, uint64_t count, void *result);
	void			Write(uint64_t location, uint64_t count, const void *from);
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

#define RS_ATTR_DIR		0x0010
#define RS_ATTR_DELETED		0x0100
#define RS_ATTR_COMPRESSED	0x0400
#define RS_ATTR_CONTIGUOUS	0x0800

struct RSDirEntry {
	uint16_t mAttributes;
	char mName[38];
	uint64_t mCluster;
	uint64_t mSize;
	RedSeaDateTime mDateTime;
} __attribute__((packed));


class RedSeaDirEntry {
public:
				RedSeaDirEntry(RedSea *, uint64_t, RedSeaDirectory *);
	bool			IsDirectory() const { return mDirEntry.mAttributes & RS_ATTR_DIR; }
	bool			IsFile() const { return !IsDirectory(); }
	const char *		Name() const { return mDirEntry.mName; }
	RSDirEntry &		DirEntry() { return mDirEntry; }
	void			Delete();
	void			Flush();
protected:
	RedSeaDirectory *mDirectory;
	RSDirEntry mDirEntry;
	uint64_t mEntryLocation;
	RedSea *mRedSea;
};

class RedSeaFile : public RedSeaDirEntry {
public:
				RedSeaFile(RedSea *, uint64_t, RedSeaDirectory *);
	void			Read(uint64_t start, uint64_t count, void *result);
	void			Write(uint64_t start, uint64_t count, const void *result);
private:
};

class RedSeaDirectory : public RedSeaDirEntry {
public:
				RedSeaDirectory(RedSea *, uint64_t, RedSeaDirectory *);
				~RedSeaDirectory();
	int			CountEntries() { return mUsedEntries; }
	bool			AddEntry(RedSeaDirEntry *);
	RedSeaDirEntry *	GetEntry(int i);
	RedSeaDirectory *	Self();
	RedSeaDirectory *	CreateDirectory(const char *name, int space);
	RedSeaFile *		CreateFile(const char *name, int size);
	void			Flush();
protected:
	int mEntryCount;
	int mUsedEntries;
	uint16_t *mAttributes;
};

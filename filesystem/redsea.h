#include <stdint.h>
#include <stdio.h>

#include <Locker.h>

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

struct RSEntryPointer {
	uint64_t mLocation;
	RedSeaDirectory *mParent;
};

extern RSEntryPointer gInvalidPointer;

class RedSea {
public:
				RedSea(int f);
	RSEntryPointer		RootDirectory();
	uint64_t			BaseOffset() { return mBoot.base_offset; }
	uint64_t			FirstFreeSector(int count);
	bool				IsFree(uint64_t sector);
	void				ForceAllocate(uint64_t sector);
	uint64_t			Allocate(int count);
	void				Deallocate(uint64_t, int);
	void				FlushBitmap();
	bool				Valid() { return mIsValid; }
	RSBoot &			BootStructure() { return mBoot; }
	int					UsedClusters();
	RedSeaDirEntry *	Create(RSEntryPointer);
private:
	friend class 		RedSeaDirEntry;
	friend class 		RedSeaFile;
	friend class 		RedSeaDirectory;
	bool				mIsValid;
	int					mFile;
	RSBoot				mBoot;
	uint8_t *			mBitmapSectors;
	uint64_t			mBitmapLength;
	uint64_t			Read(uint64_t location, uint64_t count, void *result);
	uint64_t			Write(uint64_t location, uint64_t count, const void *from);
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
	const char *	Name() const { return mDirEntry.mName; }
	RSDirEntry &	DirEntry() { return mDirEntry; }
	uint64_t		EntryLocation() const { return mEntryLocation; }
	bool			Resize(uint64_t preferredSize);
	void			Delete();
	void			Flush();
	void			LockRead();
	void			LockWrite();
	void			UnlockRead();
	void			UnlockWrite();
protected:
	BLocker mReadLocker;
	BLocker mWriteLocker;
	RedSeaDirectory *mDirectory;
	RSDirEntry mDirEntry;
	uint64_t mEntryLocation;
	RedSea *mRedSea;
};

class RedSeaFile : public RedSeaDirEntry {
public:
					RedSeaFile(RedSea *, uint64_t, RedSeaDirectory *);
	uint64_t		Read(uint64_t start, uint64_t count, void *result);
	uint64_t		Write(uint64_t start, uint64_t count, const void *result);
private:
};

class RedSeaDirectory : public RedSeaDirEntry {
public:
						RedSeaDirectory(RedSea *, uint64_t, RedSeaDirectory *);
						~RedSeaDirectory();
	int					CountEntries() { return mUsedEntries; }
	int					AddEntry(RedSeaDirEntry *);
	RSEntryPointer		GetEntry(int i);
	RSEntryPointer		Self();
	RSEntryPointer		CreateDirectory(const char *name, int space);
	RSEntryPointer		CreateFile(const char *name, int size);
	bool				RemoveEntry(RedSeaDirEntry *);
	void				Flush();
protected:
	int mEntryCount;
	int mUsedEntries;
	uint16_t *mAttributes;
};

#include "redsea.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

RSEntryPointer gInvalidPointer = { UINT64_MAX, NULL };

RedSea::RedSea(int f)
{
	mFile = f;
	Read(0, 0x200, &mBoot);

	if (mBoot.signature != 0x88 || mBoot.signature2 != 0xAA55) {
		mIsValid = false;
		return;
	}
	
	mIsValid = true;

	mBitmapLength = mBoot.bitmap_sectors * 0x200;
	mBitmapSectors = new uint8_t[mBitmapLength];
	Read(0x200, mBitmapLength * 0x200, mBitmapSectors);
}


const uint64_t m1  = (uint64_t)0x5555555555555555LL;
const uint64_t m2  = (uint64_t)0x3333333333333333LL;
const uint64_t m4  = (uint64_t)0x0f0f0f0f0f0f0f0fLL;
const uint64_t h01 = (uint64_t)0x0101010101010101LL;

int popcount(uint64_t x) {
    x -= (x >> 1) & m1;
    x = (x & m2) + ((x >> 2) & m2);
    x = (x + (x >> 4)) & m4;
    return (x * h01) >> 56;
}


int
RedSea::UsedClusters()
{
	int result = 0;
	for (int i = 0; i < mBitmapLength; i++)
		result += popcount(mBitmapSectors[i]);
	result += mBitmapLength + 1;
	return result;
}

uint64_t
RedSea::FirstFreeSector(int count)
{
	int i;
	uint64_t start = UINT64_MAX;
	int ccount = 0;
	for (i = 0; i < mBitmapLength; i++) {
		int bi, ci;
		for (bi = 1, ci = 0; ci < 8; bi <<= 1, ci++) {
			if ((mBitmapSectors[i] & bi) == 0) {
				if (start == UINT64_MAX) {
					start = (i << 3) | ci;
					start += mBoot.bitmap_sectors + 1;
				}
				ccount++;
				if (ccount >= count)
					return start;
			} else {
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
	start -= mBoot.bitmap_sectors + 1;

	uint64_t end = start + count - 1; // inclusive!

	int startbit = start % 8;
	int endbit = end % 8;

	start >>= 3;
	end >>= 3;

	if (start == end) {
		for (int i = startbit; i <= endbit; i++) {
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
	}

	return firstFree;
}


void
RedSea::Deallocate(uint64_t start, int count)
{
	start -= mBoot.bitmap_sectors + 1;
	uint64_t end = start + count - 1; // inclusive!

	int startbit = start % 8;
	int endbit = end % 8;

	start >>= 3;
	end >>= 3;

	if (start == end) {
		for (int i = startbit; i <= endbit; i++) {
			mBitmapSectors[start] &= ~(1 << i);
		}
	} else {
		if (startbit == 0) {
			mBitmapSectors[start] = 0x00;
		} else {
			for (int i = startbit; i < 8; i++) {
				mBitmapSectors[start] &= ~(1 << i);
			}
		}

		if (endbit == 0) {
			memset(mBitmapSectors + start + 1, 0x00, end - start + 1);
		} else {
			memset(mBitmapSectors + start + 1, 0x00, end - start);
			for (int i = 0; i <= endbit; i++) {
				mBitmapSectors[end] &= ~(1 << i);
			}
		}
	}
}


bool
RedSea::IsFree(uint64_t sector)
{
	sector -= mBoot.bitmap_sectors + 1;
	return mBitmapSectors[sector >> 3] & (1 << (sector % 8)) == 1;
}


void
RedSea::ForceAllocate(uint64_t sector)
{
	sector -= mBoot.bitmap_sectors + 1;
	mBitmapSectors[sector >> 3] |= (1 << (sector % 8));
}


void
RedSea::FlushBitmap()
{
	Write(0x200, mBitmapLength, mBitmapSectors);
}


RedSeaDirEntry *
RedSea::Create(RSEntryPointer pointer)
{
	uint16_t attributes;
	Read(pointer.mLocation, 2, &attributes);

	if (attributes & RS_ATTR_DIR) {
		return new RedSeaDirectory(this, pointer.mLocation, pointer.mParent);
	} else {
		return new RedSeaFile(this, pointer.mLocation, pointer.mParent);
	}
}


uint64_t
RedSea::Read(uint64_t location, uint64_t count, void *result)
{
	lseek(mFile, location, SEEK_SET);
	return read(mFile, result, count);
}


uint64_t
RedSea::Write(uint64_t location, uint64_t count, const void *from)
{
	lseek(mFile, location, SEEK_SET);
	return write(mFile, from, count);
}


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


RedSeaDirEntry::RedSeaDirEntry(RedSea *rs, uint64_t location, RedSeaDirectory *dir)
{
	rs->Read(location, sizeof(RSDirEntry), &mDirEntry);
	mDirEntry.mCluster -= rs->BaseOffset();
	mRedSea = rs;
	mEntryLocation = location;
	mDirectory = dir;
}


bool
RedSeaDirEntry::Resize(uint64_t preferred)
{
	uint64_t previousEndSector = (mDirEntry.mSize + 0x1FF) / 0x200;
	uint64_t currentEndSector = (preferred + 0x1FF) / 0x200;

	if (currentEndSector == previousEndSector) {// no new sectors needed!
		mDirEntry.mSize = preferred;
		return true;
	}

	if (preferred < mDirEntry.mSize) {
		// Downsizing
		mDirEntry.mSize = preferred;
		mRedSea->Deallocate(currentEndSector + 1, previousEndSector - currentEndSector);
	} else {
		uint64_t newsectors = currentEndSector - previousEndSector;
		for (uint64_t i = currentEndSector + 1; i <= currentEndSector; i++) {
			if (!mRedSea->IsFree(i)) {
				if (IsDirectory())
					return false; // other directories may point to this one, can't know which ones
				
				uint64_t sectors = mRedSea->Allocate(newsectors);
				if (sectors == UINT64_MAX)
					return false; // not enough space?

				uint8_t *buffer = new uint8_t[mDirEntry.mSize];
				uint64_t oldSize = mDirEntry.mSize;
				mRedSea->Read(mDirEntry.mCluster * 0x200, mDirEntry.mSize, buffer);
				mRedSea->Deallocate(mDirEntry.mCluster, (mDirEntry.mSize + 0x1FF) / 0x200);
				mDirEntry.mCluster = sectors;
				mDirEntry.mSize = preferred;
				mRedSea->Write(mDirEntry.mCluster * 0x200, oldSize, buffer);
			}
		}
		
		// All sectors are free, continue getting file
		for (uint64_t i = currentEndSector + 1; i <= currentEndSector; i++) {
			mRedSea->ForceAllocate(i);
		}

		mDirEntry.mSize = preferred;
	}
	
	return true;
}

void
RedSeaDirEntry::Delete()
{
	mRedSea->Deallocate(mDirEntry.mCluster, (mDirEntry.mSize + 0x1FF) / 0x200);
	mDirEntry.mAttributes |= RS_ATTR_DELETED;
}


void
RedSeaDirEntry::Flush()
{
	mDirEntry.mCluster += mRedSea->BaseOffset();
	mRedSea->Write(mEntryLocation, sizeof(RSDirEntry), &mDirEntry);
	mDirEntry.mCluster -= mRedSea->BaseOffset();
	if (mDirectory)
		mDirectory->Flush();
}


void
RedSeaDirEntry::LockRead()
{
	mReadLocker.Lock();
}


void
RedSeaDirEntry::LockWrite()
{
	mWriteLocker.Lock();
}


void
RedSeaDirEntry::UnlockRead()
{
	mReadLocker.Unlock();
}


void
RedSeaDirEntry::UnlockWrite()
{
	mWriteLocker.Unlock();
}


RedSeaFile::RedSeaFile(RedSea *rs, uint64_t location, RedSeaDirectory *dir)
	: RedSeaDirEntry(rs, location, dir)
{

}


uint64_t
RedSeaFile::Read(uint64_t start, uint64_t count, void *result)
{
	if (start > mDirEntry.mSize)
		return UINT64_MAX;
	if (start + count > mDirEntry.mSize)
		count = mDirEntry.mSize - start;
	return mRedSea->Read(start + (mDirEntry.mCluster * 0x200), count, result);
}


uint64_t
RedSeaFile::Write(uint64_t start, uint64_t count, const void *result)
{
	if (start > mDirEntry.mSize)
		return UINT64_MAX;
	if (start + count > mDirEntry.mSize)
		count = mDirEntry.mSize - start;
	return mRedSea->Write(start + (mDirEntry.mCluster * 0x200), count, result);
}


RedSeaDirectory::RedSeaDirectory(RedSea *rs, uint64_t location, RedSeaDirectory *dir)
	: RedSeaDirEntry(rs, location, dir),
	mAttributes(0)
{
	mEntryCount = mDirEntry.mSize / 64;

	Flush();
}


int
RedSeaDirectory::AddEntry(RedSeaDirEntry *entry)
{
	if (mEntryCount == mUsedEntries)
		return -1;

	int j;
	for (j = 1; j < mEntryCount; j++) {
		if ((mAttributes[j] & RS_ATTR_DELETED) || mAttributes[j] == 0) {
			break;
		}
	}
	
	mUsedEntries++;
	
	RSDirEntry &ent = entry->DirEntry();
	
	RedSeaDirEntry entr(mRedSea, mDirEntry.mCluster * 0x200 + j * 64, this);
	RSDirEntry &nent = entr.DirEntry();
	nent.mAttributes = ent.mAttributes;
	strcpy(nent.mName, ent.mName);
	nent.mCluster = ent.mCluster;
	nent.mSize = ent.mSize;
	nent.mDateTime = ent.mDateTime;
	
	entr.Flush();
	return j;
}


bool
RedSeaDirectory::RemoveEntry(RedSeaDirEntry *entry)
{
	uint64_t location = entry->EntryLocation() - mDirEntry.mCluster * 0x200;
	location /= 64;
	
	if (location > mEntryCount)
		return false;
	
	entry->DirEntry().mAttributes |= RS_ATTR_DELETED;
	entry->Flush();
	return true;
}

void
RedSeaDirectory::Flush()
{
	if (mAttributes)
		delete[] mAttributes;

	mAttributes = new uint16_t[mEntryCount];
	mUsedEntries = 0;

	for (int i = 1; i < mEntryCount; i++) {
		uint64_t base = mDirEntry.mCluster * 0x200 + i * 64;
		mRedSea->Read(base, 2, mAttributes + i);
		if (mAttributes[i] != 0 && !(mAttributes[i] & RS_ATTR_DELETED)) {
			mUsedEntries++;
		}
	}
}

RedSeaDirectory::~RedSeaDirectory()
{
	delete[] mAttributes;
}


RSEntryPointer
RedSeaDirectory::GetEntry(int i)
{
	if (i >= mUsedEntries)
		return gInvalidPointer;

	int count = 0;
	int j;
	for (j = 1; count != i; j++) {
		if (j > mEntryCount)
			return gInvalidPointer;

		if (mAttributes[j] != 0x0000 && !(mAttributes[j] & RS_ATTR_DELETED)) {
			count++;
		}
	}

	i = j;

	uint64_t base = mDirEntry.mCluster * 0x200 + i * 64;

	return (RSEntryPointer) { base, this };
}


RSEntryPointer
RedSeaDirectory::Self()
{
	return (RSEntryPointer) {mDirEntry.mCluster * 0x200, this};
}


RSEntryPointer
RedSeaDirectory::CreateFile(const char *name, int size)
{
	if (mEntryCount == mUsedEntries) {
		if (Resize(mDirEntry.mSize + 0x200)) {
			mEntryCount = mDirEntry.mSize / 64;
			Flush();
		} else {
			return gInvalidPointer;
		}
	}

	int sectors = (size + 0x1FF) / 0x200;
	uint64_t location = mRedSea->Allocate(sectors);

	if (location == UINT64_MAX)
		return gInvalidPointer;

	int j;
	for (j = 1; j < mEntryCount; j++) {
		if ((mAttributes[j] & RS_ATTR_DELETED) || mAttributes[j] == 0) {
			break;
		}
	}

	RedSeaDirEntry ent(mRedSea, mDirEntry.mCluster * 0x200 + j * 64, this);
	RSDirEntry &d = ent.DirEntry();
	d.mAttributes = RS_ATTR_CONTIGUOUS;
	strncpy(d.mName, name, 37);
	d.mName[37] = 0;
	d.mCluster = location;
	d.mSize = size;

	ent.Flush();

	return (RSEntryPointer) { mDirEntry.mCluster * 0x200 + j * 64, this};
}

RSEntryPointer
RedSeaDirectory::CreateDirectory(const char *name, int space)
{
	if (mEntryCount == mUsedEntries)
		return gInvalidPointer;

	char zerobuffer[0x200] = {0};

	int sectors = (space * 64 + 0x1FF) / 0x200;
	uint64_t location = mRedSea->Allocate(sectors);

	if (location == UINT64_MAX)
		return gInvalidPointer;

	int bytes = sectors * 0x200;
	uint64_t loc = location * 0x200;

	while (bytes > 0) {
		mRedSea->Write(loc, 0x200, zerobuffer);
		loc += 0x200;
		bytes -= 0x200;
	}

	RedSeaDirEntry d(mRedSea, location * 0x200, this);

	RSDirEntry &ent = d.DirEntry();
	ent.mAttributes = RS_ATTR_DIR | RS_ATTR_CONTIGUOUS;
	strncpy(ent.mName, name, 37);
	ent.mCluster = location;
	ent.mSize = sectors * 0x200;
	d.Flush();

	RedSeaDirEntry parent(mRedSea, location * 0x200 + 64, this);

	RSDirEntry &pent = parent.DirEntry();
	pent.mAttributes = RS_ATTR_DIR | RS_ATTR_CONTIGUOUS;
	strncpy(pent.mName, "..", 3);
	pent.mCluster = mDirEntry.mCluster;
	pent.mSize = mDirEntry.mSize;
	parent.Flush();

	int j;
	for (j = 1; j < mEntryCount; j++) {
		if ((mAttributes[j] & RS_ATTR_DELETED) || mAttributes[j] == 0) {
			break;
		}
	}

	mAttributes[j] = RS_ATTR_DIR | RS_ATTR_CONTIGUOUS;

	RedSeaDirEntry child(mRedSea, mDirEntry.mCluster * 0x200 + j * 64, this);

	RSDirEntry &cent = child.DirEntry();
	cent.mAttributes = RS_ATTR_DIR | RS_ATTR_CONTIGUOUS;
	strncpy(cent.mName, name, 37);
	cent.mCluster = location;
	cent.mSize = sectors * 0x200;
	child.Flush();

	mUsedEntries++;

	return (RSEntryPointer) {mDirEntry.mCluster * 0x200 + j * 64, this};
}

RSEntryPointer
RedSea::RootDirectory()
{
	return (RSEntryPointer) {(mBoot.root_sector - mBoot.base_offset) * 0x200, NULL};
}

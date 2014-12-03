#include "redsea.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

RedSea::RedSea(FILE *f)
{
	mFile = f;
	Read(0, 0x200, &mBoot);
	mBitmapLength = mBoot.bitmap_sectors * 0x200;
	mBitmapSectors = new uint8_t[mBitmapLength];
	Read(0x200, mBitmapLength * 0x200, mBitmapSectors);
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
			if (!(mBitmapSectors[i] & bi)) {
				if (start == UINT64_MAX) {
					start = (i << 3) | ci;
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
	uint64_t end = firstFree + count - 1; // inclusive!

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


void
RedSea::Read(uint64_t location, uint64_t count, void *result) {
	fseek(mFile, location, SEEK_SET);
	fread(result, count, 1, mFile);
}


void
RedSea::Write(uint64_t location, uint64_t count, const void *from) {
	fseek(mFile, location, SEEK_SET);
	fwrite(from, count, 1, mFile);
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


RedSeaDirEntry::RedSeaDirEntry(RedSea *rs, uint64_t location)
{
	rs->Read(location, sizeof(RSDirEntry), &mDirEntry);
	mDirEntry.mCluster -= rs->BaseOffset();
	mRedSea = rs;
	mEntryLocation = location;
}


void
RedSeaDirEntry::Delete()
{
	mRedSea->Deallocate(mDirEntry.mCluster, (mDirEntry.mSize + 0x200) / 0x200);
	mDirEntry.mAttributes |= RS_ATTR_DELETED;
}


void
RedSeaDirEntry::Flush()
{
	mRedSea->Write(mEntryLocation, sizeof(RSDirEntry), &mDirEntry);
}


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


RedSeaDirectory::RedSeaDirectory(RedSea *rs, uint64_t location)
	: RedSeaDirEntry(rs, location)
{
	mEntryCount = mDirEntry.mSize / 64;
	mAttributes = new uint16_t[mEntryCount];

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


RedSeaDirEntry *
RedSeaDirectory::GetEntry(int i)
{
	if (i >= mUsedEntries)
		return nullptr;

	int count = 0;
	int j;
	for (j = 1; j < mEntryCount && count != i; j++) {
		if (mAttributes[j] != 0x0000 && !(mAttributes[j] & RS_ATTR_DELETED)) {
			count++;
		}
	}

	i = j;

	uint64_t base = mDirEntry.mCluster * 0x200 + i * 64;
	if (mAttributes[i] == 0x0000) {
		return nullptr;
		mEntryCount = i;
	}

	if (mAttributes[i] & RS_ATTR_DIR) {
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

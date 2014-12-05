#include <fs_index.h>
#include <fs_info.h>
#include <fs_interface.h>
#include <fs_query.h>
#include <fs_volume.h>

// #include <ObjectList.h>

#include <stdlib.h>
#include <string.h>
#include "redsea.h"

// #pragma mark - Module Interface

ino_t ino_for_dirent(fs_volume *volume, RedSeaDirEntry *entry, bool remove = false);

RedSeaDirEntry *entry_for_name(RedSeaDirectory *directory, const char *name)
{
	if (strcmp(".", name) == 0)
		return directory->Self(); // create copy of directory
	
	for (int i = 0; i < directory->CountEntries(); i++)
	{
		RedSeaDirEntry *entry = directory->GetEntry(i);
		if (strcmp(entry->Name(), name) == 0) {
			return entry;
		}
		delete entry;
	}
	
	return NULL;
}

static status_t
redsea_std_ops(int32 op, ...)
{
	switch (op) {
		case B_MODULE_INIT:
		{
			return B_OK;
		}

		case B_MODULE_UNINIT:
			return B_OK;

		default:
			return B_ERROR;
	}
}

status_t redsea_lookup(fs_volume *volume, fs_vnode *v_dir, const char *name, ino_t *id)
{
	RedSeaDirEntry *entr = (RedSeaDirEntry *)v_dir->private_node;
	if (!entr->IsDirectory())
		return B_ERROR;
	
	RedSeaDirectory *dir = (RedSeaDirectory *)entr;
	
	RedSeaDirEntry *entry = entry_for_name(dir, name);

	if (entry != NULL) {
		*id = ino_for_dirent(volume, entry);
		return B_OK;
	}
	
	return B_ERROR;
}


status_t redsea_get_vnode_name(fs_volume *volume, fs_vnode *vnode, char *buffer, size_t bufferSize)
{
	RedSeaDirEntry *entr = (RedSeaDirEntry *)vnode->private_node;

	if (entr == NULL)
		return B_ERROR;

	strncpy(buffer, entr->Name(), bufferSize);
	return B_OK;
}


status_t redsea_unlink(fs_volume *volume, fs_vnode *v_dir, const char *name)
{
	RedSeaDirEntry *entr = (RedSeaDirEntry *)v_dir->private_node;
	if (!entr->IsDirectory())
		return B_ERROR;
	
	RedSeaDirectory *dir = (RedSeaDirectory *)entr;
	
	for (int i = 0; i < dir->CountEntries(); i++)
	{
		RedSeaDirEntry *entry = dir->GetEntry(i);
		if (strcmp(entry->Name(), name) == 0) {
			entry->Delete();
			entry->Flush();
			delete entry;
			return true;
		}
		delete entry;
	}
	
	return B_ERROR;
}


status_t redsea_rename(fs_volume *volume, fs_vnode *dir, const char *fromName,
	fs_vnode *todir, const char *toName)
{
	return B_READ_ONLY_DEVICE;
/*	
	RedSeaDirectory *from = (RedSeaDirectory *)dir->private_node;
	RedSeaDirectory *to = (RedSeaDirectory *)todir->private_node;
	
	RedSeaDirEntry *fromnode = entry_for_name(from, fromName);
	
	if (from == to) {
		from->RemoveEntry(fromnode);
		to->AddEntry(fromnode);
	} else {
		if (!to->AddEntry(fromnode)) {
			delete fromnode;
			return B_ERROR;
		}
		from->RemoveEntry(fromnode);
	}
	
	delete fromnode;
	return B_OK;*/
}


status_t redsea_access(fs_volume* volume, fs_vnode *vnode, int mode)
{
	return B_OK;
}


status_t redsea_read_stat(fs_volume *volume, fs_vnode *vnode,
	struct stat *stat)
{
	RedSeaDirEntry *entry = (RedSeaDirEntry *)vnode->private_node;
	stat->st_mode = DEFFILEMODE | (entry->IsDirectory() ? S_IFDIR : S_IFREG);
	stat->st_nlink = 0;
	stat->st_uid = 0;
	stat->st_gid = 0;
	stat->st_size = entry->DirEntry().mSize;
	stat->st_blksize = 0x200;
	stat->st_blocks = (stat->st_size + 0x1FF) / 0x200;
	return B_OK;
}


status_t redsea_unmount(fs_volume *volume)
{
	/*
	RedSea *rs = (RedSea *)volume->private_volume;
	BObjectList<RedSeaDirEntry> entries;
	BObjectList<RedSeaDirectory> directoriesToTraverse;
	directoriesToTraverse.AddItem(rs->RootDirectory());
	for (int i = 0; i < directoriesToTraverse.CountItems(); i++) {
		RedSeaDirectory *dir = directoriesToTraverse.ItemAt(i);
		for (int j = 0; j < dir->CountEntries(); j++) {
			RedSeaDirEntry *entr = dir->GetEntry(j);
			if (entr->IsDirectory()) {
				directoriesToTraverse.AddItem((RedSeaDirectory *)entr);
			} else {
				entries.AddItem(entr);
			}
		}
		directoriesToTraverse.RemoveItemAt(i);
		i--;

		entries.AddItem(dir);
	}
	
	for (int i = 0; i < entries.CountItems(); i++) {
		RedSeaDirEntry *entr = entries.ItemAt(i);
		remove_vnode(volume, entr->DirEntry().mCluster);
		delete entr;
		entries.RemoveItemAt(i);
		i--;
	}*/
	
	return B_OK;
}

status_t redsea_write_stat(fs_volume *volume, fs_vnode *vnode,
	const struct stat *stat, uint32 statmask)
{
	return B_ERROR;
}


status_t redsea_create(fs_volume *volume, fs_vnode *dir, const char *name,
	int openmode, int perms, void **cookie, ino_t *newVnodeId)
{
	return B_ERROR;
}

struct FileCookie {
	RedSeaFile *file;
};

status_t redsea_open(fs_volume *volume, fs_vnode *vnode, int openmode, void **cookie)
{
	if (openmode & O_WRONLY || openmode & O_RDWR)
		return B_READ_ONLY_DEVICE;
	
	*cookie = malloc(sizeof(FileCookie));
	FileCookie *c = (FileCookie *)*cookie;
	
	c->file = (RedSeaFile *)vnode->private_node;
	return B_OK;
}


status_t redsea_close(fs_volume *volume, fs_vnode *vnode, void *cookie)
{
	return B_OK;
}


status_t redsea_free_cookie(fs_volume *volume, fs_vnode *vnode, void *cookie)
{
	delete (FileCookie *)cookie;
	return B_OK;
}


status_t redsea_read(fs_volume *volume, fs_vnode *vnode, void *cookie,
	off_t pos, void *buffer, size_t *length)
{
	RedSeaFile *f = ((FileCookie *)cookie)->file;

	*length = f->Read(pos, *length, buffer);

	if (*length == UINT64_MAX)
		return B_ERROR;

	return B_OK;
}


status_t redsea_write(fs_volume *volume, fs_vnode *vnode, void *cookie,
	off_t pos, const void *buffer, size_t *length)
{
	return B_READ_ONLY_DEVICE;

	/*
	RedSeaFile *f = ((FileCookie *)cookie)->file;

	*length = f->Write(pos, *length, buffer);

	if (*length == UINT64_MAX)
		return B_ERROR;

	return B_OK; */
}

struct DirCookie {
	int index;
};


status_t redsea_open_dir(fs_volume *volume, fs_vnode *vnode, void **cookie) {
	RedSeaDirEntry *entr = (RedSeaDirEntry *)vnode->private_node;

	if (!entr->IsDirectory())
		return B_ERROR;

	DirCookie *c = (DirCookie *)malloc(sizeof(DirCookie));
	c->index = 0;
	*cookie = c;
	
	return B_OK;
}


status_t redsea_close_dir(fs_volume *volume, fs_vnode *vnode, void *cookie)
{
	return B_OK;
}

status_t redsea_free_dir_cookie(fs_volume *volume, fs_vnode *vnode, void *cookie)
{
	delete (DirCookie *)cookie;
	return B_OK;
}


status_t redsea_read_dir(fs_volume *volume, fs_vnode *vnode, void *cookie,
	struct dirent *buffer, size_t buffersize, uint32 *num)
{
	RedSeaDirectory *dir = (RedSeaDirectory *)vnode->private_node;
	DirCookie *dircookie = (DirCookie *)cookie;
	
	if (dircookie->index >= dir->CountEntries()) {
		*num = 0;
		return B_OK;
	}
	
	RedSeaDirEntry *entry = dir->GetEntry(dircookie->index++);
	
	buffer->d_dev = volume->id;
	buffer->d_ino = ino_for_dirent(volume, entry, false);

	size_t namesize = buffersize - sizeof(struct dirent) - 1;
	int namelength = strlen(entry->Name());
	buffer->d_reclen = sizeof(struct dirent) - 1 +
		(namesize > namelength ? namelength : namesize);
	
	if (namelength > namesize) {
		return B_BUFFER_OVERFLOW;
	}
	
	strcpy(buffer->d_name, entry->Name());

	*num = 1;
	return B_OK;
}


status_t redsea_rewind_dir(fs_volume *volume, fs_vnode *vnode, void *cookie)
{
	RedSeaDirectory *dir = (RedSeaDirectory *)vnode->private_node;
	DirCookie *dircookie = (DirCookie *)cookie;
	dircookie->index = 0;
	
	return B_OK;
}


fs_vnode_ops gRedSeaFSVnodeOps = {
	// vnode operations
	redsea_lookup,			// lookup
	redsea_get_vnode_name,	// get name
	NULL, // write_vnode,	// write
	NULL, // remove_vnode,	// remove

	// VM file access
	NULL,					// can_page
	NULL,					// read pages
	NULL,					// write pages

	NULL,					// io?
	NULL,					// cancel io

	NULL,					// get file map

	NULL, // ioctl,
	NULL, // set_flags,
	NULL,   // NULL, // select,
	NULL,   // NULL, // deselect,
	NULL, // fsync,

	NULL, // read_symlink,
	NULL, // create_symlink,

	NULL, // link,
	redsea_unlink, // unlink,
	redsea_rename, // rename,

	redsea_access, // access,
	redsea_read_stat, // read_stat,
	redsea_write_stat, // write_stat,
	NULL,   // NULL, // preallocate,

	// file operations
	redsea_create, // create,
	redsea_open, // open,
	redsea_close, // close,
	redsea_free_cookie, // free_cookie,
	redsea_read, // read,
	redsea_write, // write,

	// directory operations
	NULL, // create_dir,
	NULL, // remove_dir,
	redsea_open_dir, // open_dir,
	redsea_close_dir, // close_dir,
	redsea_free_dir_cookie, // free_dir_cookie,
	redsea_read_dir, // read_dir,
	redsea_rewind_dir, // rewind_dir,

	// attribute directory operations
	NULL, // open_attr_dir,
	NULL, // close_attr_dir,
	NULL, // free_attr_dir_cookie,
	NULL, // read_attr_dir,
	NULL, // rewind_attr_dir,

	// attribute operations
	NULL, // create_attr,
	NULL, // open_attr,
	NULL, // close_attr,
	NULL, // free_attr_cookie,
	NULL, // read_attr,
	NULL, // write_attr,

	NULL, // read_attr_stat,
	NULL,   // NULL, // write_attr_stat,
	NULL, // rename_attr,
	NULL, // remove_attr,

	// special nodes
	NULL	// create_special_node
};


ino_t ino_for_dirent(fs_volume *volume, RedSeaDirEntry *entry, bool remove)
{
	void *private_node;
	ino_t presumed = (ino_t)entry->DirEntry().mCluster;
	status_t result = publish_vnode(volume, presumed, entry, &gRedSeaFSVnodeOps,
		(entry->IsDirectory() ? S_IFDIR : S_IFREG), 0);

	// TODO: Kinda-bad hack

	if (result != B_OK) {
		if (remove)
			delete entry;
		get_vnode(volume, presumed, &private_node);
	}

	return presumed;
}

status_t redsea_read_fs_info(fs_volume* volume, struct fs_info* info)
{
	RedSea *rs = (RedSea *)volume->private_volume;

	if (rs == NULL)
		return B_ERROR;

	RedSeaDirectory *root = rs->RootDirectory();
	info->root = root->DirEntry().mCluster;
	
	info->flags = B_FS_IS_READONLY;
	info->block_size = 0x200;
	info->io_size = 0x200;
	info->total_blocks = rs->BootStructure().count;
	info->free_blocks = info->total_blocks - rs->UsedClusters();

	info->total_nodes = info->total_blocks * 8;
	info->free_nodes = info->free_blocks * 8;
	
	strcpy(info->volume_name, "RedSea Volume");
	strcpy(info->fsh_name, "RedSeaFS");

	return B_OK;
}

fs_volume_ops gRedSeaFSVolumeOps = {
	redsea_unmount, // unmount,
	redsea_read_fs_info, // read_fs_info
	NULL, // write_fs_info
	NULL, // sync
	NULL, // read_vnode,

	/* index directory & index operations */
	NULL, // open_index_dir,
	NULL, // close_index_dir,
	NULL, // free_index_dir_cookie,
	NULL, // read_index_dir,
	NULL, // rewind_index_dir,

	NULL, // create_index,
	NULL, // remove_index,
	NULL, // read_index_stat,

	/* query operations */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL	// rewind_query
};

uint32 redsea_get_supported_operations(partition_data *data, uint32 mask)
{
	return B_DISK_SYSTEM_SUPPORTS_WRITING;
}


status_t redsea_mount(fs_volume *volume, const char *device, uint32 flags,
	const char *args, ino_t *_rootVnodeID)
{
	int fd = open(device, O_RDWR | O_NOCACHE);

	RedSea *rs = new RedSea(fd);
	
	if (!rs->Valid()) {
		delete rs;
		return B_ERROR;
	}
	
	volume->ops = &gRedSeaFSVolumeOps;
	volume->private_volume = rs;
	
	*_rootVnodeID = ino_for_dirent(volume, rs->RootDirectory(), true);
	
	return B_OK;
}


static file_system_module_info sRedSeaModuleInfo = {
	{
		"file_systems/redseafs" B_CURRENT_FS_API_VERSION,
		0,
		redsea_std_ops,
	},

	"redseafs",				// short_name
	"RedSea File System",	// pretty_name
	0						// DDM flags
	| B_DISK_SYSTEM_SUPPORTS_WRITING,

	// scanning
	NULL,	// identify_partition()
	NULL,	// scan_partition()
	NULL,	// free_identify_partition_cookie()
	NULL,	// free_partition_content_cookie()

	redsea_mount,   // mount

	redsea_get_supported_operations,	// get_supported_operations

	NULL,   // validate_resize
	NULL,   // validate_move
	NULL,   // validate_set_content_name
	NULL,   // validate_set_content_parameters
	NULL,   // validate_initialize,

	/* shadow partition modification */
	NULL,   // shadow_changed

	/* writing */
	NULL,   // defragment
	NULL,   // repair
	NULL,   // resize
	NULL,   // move
	NULL,   // set_content_name
	NULL,   // set_content_parameters
	NULL	// bfs_initialize
};

module_info *modules[] = {
	(module_info *)&sRedSeaModuleInfo,
	NULL,
};


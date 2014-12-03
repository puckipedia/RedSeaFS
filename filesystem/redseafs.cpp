#include <fs_index.h>
#include <fs_info.h>
#include <fs_interface.h>
#include <fs_query.h>
#include <fs_volume.h>

// #pragma mark - Module Interface


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


fs_volume_ops gRedSeaFSVolumeOps = {
	NULL, // unmount,
	NULL, // read_fs_info
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

/*
fs_vnode_ops gRamFSVnodeOps = {
	// vnode operations
	NULL, // lookup,			// lookup
	NULL,					// get name
	NULL, // write_vnode,		// write
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
	NULL, // unlink,
	NULL, // rename,

	NULL, // access,
	NULL, // read_stat,
	NULL, // write_stat,
	NULL,   // NULL, // preallocate,

	// file operations
	NULL, // create,
	NULL, // open,
	NULL, // close,
	NULL, // free_cookie,
	NULL, // read,
	NULL, // write,

	// directory operations
	NULL, // create_dir,
	NULL, // remove_dir,
	NULL, // open_dir,
	NULL, // close_dir,
	NULL, // free_dir_cookie,
	NULL, // read_dir,
	NULL, // rewind_dir,

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
};*/

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

	NULL,   // mount

	NULL,	// get_supported_operations

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


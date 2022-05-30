/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018  Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include<linux/buffer_head.h>
#include<linux/debugfs.h>
#include<linux/slab.h>
#include <linux/fs.h>
#include "ouichefs.h"

static struct dentry *debug_dir;
static struct dentry *dentry;


ssize_t read_db_ouichefs(struct file *file, __user char *buf,
	size_t size, loff_t *offset)
{
	struct inode *inode_dir;
	struct inode *inode;
	struct ouichefs_inode_info *ci_dir;
	struct ouichefs_inode_info *ci_inode;
	struct ouichefs_file_index_block *index_file;
	struct ouichefs_file_index_block *index_tmp;
	struct ouichefs_dir_block *index_dir;
	struct buffer_head *bh;
	struct buffer_head *bh_index;
	struct buffer_head *bh_tmp;
	char *buffer;
	uint32_t cpt;
	int curr;
	int j = 0;
	int err;
	int size_to_ret = 0;

	pr_info("Call to read: size=%lu, offset=%lld\n", size, *offset);
	if (*offset > 0)
		goto out;
	inode_dir = dentry->d_inode;
	ci_dir = OUICHEFS_INODE(inode_dir);
	bh = sb_bread(inode_dir->i_sb, ci_dir->index_block);
	if (!bh)
		return -EIO;
	index_dir = (struct ouichefs_dir_block *)bh->b_data;
	if (index_dir->files[0].inode == 0) {
		brelse(bh);
		goto empty_dir;
	}
	buffer = kmalloc(sizeof(char) * 2048, GFP_KERNEL);
	memset(buffer, 0, 2048);
	size_to_ret += snprintf(buffer, 25, "| Inode\t|\tHistorique\n");
	pr_info("| Inode\t| Historique\n");
	while (index_dir->files[j].inode != 0) {
		size_to_ret += snprintf(buffer+size_to_ret,
			sizeof(index_dir->files[j].inode)+2,
				"|\t%d", index_dir->files[j].inode);
		pr_info("|\t%d", index_dir->files[j].inode);
		cpt = 0;
		inode = ouichefs_iget(inode_dir->i_sb,
			index_dir->files[j].inode);
		ci_inode = OUICHEFS_INODE(inode);
		bh_index = sb_bread(inode_dir->i_sb, ci_inode->index_block);
		index_file = (struct ouichefs_file_index_block *)
			bh_index->b_data;
		index_tmp = index_file;
		curr = ci_inode->index_block;
		size_to_ret += snprintf(buffer+size_to_ret, 4, "\t|\t");
		pr_info(" | ");
		while (curr != -1) {
			size_to_ret += snprintf(buffer,
			sizeof(index_dir->files[j].inode) + 4, " %d, ", curr);
			pr_info(" %d, ", curr);
			bh_tmp = sb_bread(inode->i_sb, curr);
			if (!bh_tmp) {
				size_to_ret = -EIO;
				goto failed;
			}
			index_tmp = (struct ouichefs_file_index_block *)
				bh_tmp->b_data;
			cpt++;
			curr = index_tmp->prev;
			brelse(bh_tmp);
		}
		j++;
		size_to_ret += snprintf(buffer+size_to_ret, 33,
			"\t Le nombre de versions est %d\n", cpt);
		pr_info("\t Le nombre de versions est %d\n", cpt);
	}
	size_to_ret += snprintf(buffer+size_to_ret, 1, "\0");
	brelse(bh_index);
	brelse(bh);
	pr_info("Copied to user: %s\n", buffer);
	err = copy_to_user(buf, buffer, size_to_ret);
	if (err)
		pr_debug("Not all data were copied!\n");
	kfree(buffer);
	*offset += size_to_ret;
	return size_to_ret;
failed:
	brelse(bh);
	kfree(buffer);
	return size_to_ret;

empty_dir:
out:
	return 0;
}


ssize_t write_db_ouichefs(struct file *file, const char *buffer, size_t size,
	loff_t *offset)
{
	return size;
}


static const struct file_operations ouichefs_file_ope = {
	.read = read_db_ouichefs,
	.write = write_db_ouichefs
};

/*
 * Mount a ouiche_fs partition
 */
struct dentry *ouichefs_mount(struct file_system_type *fs_type, int flags,
			      const char *dev_name, void *data)
{
	struct dentry *debug_file = NULL;
	char *buffer = kmalloc(sizeof(char) * 30, GFP_KERNEL);

	dentry = mount_bdev(fs_type, flags, dev_name, data,
		ouichefs_fill_super);

	strcpy(buffer, dev_name + 5);
	debug_file = debugfs_create_file(buffer, 0400, debug_dir, NULL,
		&ouichefs_file_ope);

	if (IS_ERR(dentry))
		pr_err("'%s' mount failure\n", dev_name);
	else
		pr_info("'%s' mount success\n", dev_name);
	return dentry;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
	debugfs_remove_recursive(debug_dir);
	kill_block_super(sb);
	pr_info("unmounted disk\n");
}

static struct file_system_type ouichefs_file_system_type = {
	.owner = THIS_MODULE,
	.name = "ouichefs",
	.mount = ouichefs_mount,
	.kill_sb = ouichefs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
	.next = NULL,
};

static int __init ouichefs_init(void)
{
	int ret;

	ret = ouichefs_init_inode_cache();
	if (ret) {
		pr_err("inode cache creation failed\n");
		goto end;
	}

	ret = register_filesystem(&ouichefs_file_system_type);

	if (ret) {
		pr_err("register_filesystem() failed\n");
		goto end;
	}

	debug_dir = debugfs_create_dir("ouichefs", NULL);
	pr_info("module loaded\n");
end:
	return ret;
}

static void __exit ouichefs_exit(void)
{
	int ret;

	ret = unregister_filesystem(&ouichefs_file_system_type);
	if (ret)
		pr_err("unregister_filesystem() failed\n");

	if (!debug_dir)
		debugfs_remove_recursive(debug_dir);

	ouichefs_destroy_inode_cache();
	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@lip6.fr>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");

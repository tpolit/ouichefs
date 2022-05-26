// SPDX-License-Identifier: GPL-2.0
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

static struct dentry* debug_dir;
static struct dentry *dentry = NULL;

ssize_t read_db_ouichefs(struct file* file,char* buffer,size_t size,loff_t* offset)
{

	struct inode* inode_partition;
	struct inode* first_inode;
	struct ouichefs_inode_info* ci;
	struct ouichefs_inode_info* ci_first;
	struct ouichefs_file_index_block* index_file;
	struct ouichefs_dir_block* index_dir;
	struct buffer_head* bh;
	struct ouichefs_file_index_block* tmp;
	struct buffer_head* bh_tmp;
	struct buffer_head* bh_index;

	int ret;
	int i = 0;
	int cpt=0;

	inode_partition=dentry->d_inode;
	ci=OUICHEFS_INODE(inode_partition);
	//pr_info("L'index bloc de la partition est %d\n",ci->index_block);
	bh=sb_bread(inode_partition->i_sb,ci->index_block);
	index_dir=(struct ouichefs_dir_block*)bh->b_data;
	//copy_to_user(buffer,"| inode |\t version |\t blocks \n",size);
	//ret=snprintf(to_send,20,"inode %d",index->files[0].inode);
	//copy_to_user(buffer,to_send,20);

	pr_info("Inode %d \n",index_dir->files[0].inode);
	first_inode=ouichefs_iget(inode_partition->i_sb,index_dir->files[0].inode);
	ci_first=OUICHEFS_INODE(first_inode);
	pr_info("Index bloc %d \n",ci_first->index_block);

	bh_index=sb_bread(inode_partition->i_sb,ci_first->index_block);
	index_file=(struct ouichefs_file_index_block*)bh_index->b_data;
	pr_info("Liste des blocs \n");
	pr_info("Bloc num  0 %d :\n ",index_file->blocks[0]);
	while(index_file->blocks[i]!=0){
		pr_info("Bloc %d \n",index_file->blocks[i]);
		i++;
	}

	//Parcours pour compter le nbre de versions"
	tmp=index_file;
	while(tmp->prev!=-1){
		bh_tmp=sb_bread(inode_partition->i_sb,tmp->prev);
		tmp=(struct ouichefs_file_index_block*)bh_tmp -> b_data;
		brelse(bh_tmp);
		cpt++;
	}

	pr_info("Il y'a %d versions du fichiers \n",cpt);
	

	brelse(bh);
	brelse(bh_index);
	return size;

}

ssize_t write_db_ouichefs(struct file* file,const char* buffer, size_t size,loff_t* offset)
{
	return size;
}


static const struct file_operations ouichefs_file_ope={
	.read=read_db_ouichefs,
	.write=write_db_ouichefs
};

/*
 * Mount a ouiche_fs partition
 */
struct dentry *ouichefs_mount(struct file_system_type *fs_type, int flags,
			      const char *dev_name, void *data)
{
	
	struct dentry* debug_file=NULL;
	int value;
	dentry = mount_bdev(fs_type, flags, dev_name, data,
			    ouichefs_fill_super);
	if (IS_ERR(dentry))
		pr_err("'%s' mount failure\n", dev_name);
	else
		pr_info("'%s' mount success\n", dev_name);

	debug_file = debugfs_create_file("partition",0666,debug_dir,&value,&ouichefs_file_ope);
	pr_info("L'inode de la partition est %lu \n",dentry->d_inode->i_ino);


	

	return dentry;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
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

	debug_dir=debugfs_create_dir("ouichefs",NULL);
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

	ouichefs_destroy_inode_cache();
	debugfs_remove_recursive(debug_dir);
	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@lip6.fr>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");

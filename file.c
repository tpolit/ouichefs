/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018 Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) "ouichefs: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>

#include "ouichefs.h"
#include "bitmap.h"

/*
 * Map the buffer_head passed in argument with the iblock-th block of the file
 * represented by inode. If the requested block is not allocated and create is
 * true,  allocate a new block on disk and map it.
 */
static int ouichefs_file_get_block(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index;
	bool alloc = false;
	int ret = 0, bno;

	/* If block number exceeds filesize, fail */
	if (iblock >= OUICHEFS_BLOCK_SIZE >> 2)
		return -EFBIG;

	/* Read index block from disk */
	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/*
	 * Check if iblock is already allocated. If not and create is true,
	 * allocate it. Else, get the physical block number.
	 */
	if (index->blocks[iblock] == 0) {
		if (!create)
			return 0;
		bno = get_free_block(sbi);
		if (!bno) {
			ret = -ENOSPC;
			goto brelse_index;
		}
		/* On alloue le nouveau bloc depuis le disque et on l'ajoute
		dans la liste des blocs du fichier */
		index->blocks[iblock] = bno;
		alloc = true;
	} else {
		bno = index->blocks[iblock];
	}

	/* Map the physical block to to the given buffer_head */
	map_bh(bh_result, sb, bno);

brelse_index:
	brelse(bh_index);

	return ret;
}

/*
 * Called by the page cache to read a page from the physical disk and map it in
 * memory.
 */
static int ouichefs_readpage(struct file *file, struct page *page)
{
	return mpage_readpage(page, ouichefs_file_get_block);
}

/*
 * Called by the page cache to write a dirty page to the physical disk (when
 * sync is called or when memory is needed).
 */
static int ouichefs_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, ouichefs_file_get_block, wbc);
}

/*
 * Called by the VFS when a write() syscall occurs on file before writing the
 * data in the page cache. This functions checks if the write will be able to
 * complete and allocates the necessary blocks through block_write_begin().
 */
static int ouichefs_write_begin(struct file *file,
				struct address_space *mapping, loff_t pos,
				unsigned int len, unsigned int flags,
				struct page **pagep, void **fsdata)
{
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);
	struct buffer_head *bh_old_index;
	/* struct ouichefs_inode *cinode = NULL; */
	struct buffer_head *bh_new_index;
	struct ouichefs_file_index_block *old_index_block;
	struct ouichefs_file_index_block *new_index_block;
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct buffer_head *bh_old_bloc;
	struct buffer_head *bh_new_bloc;
	struct buffer_head *bh;
	int new_index;
	int i;
	int err;
	uint32_t nr_allocs = 0;
	
	pr_info("***************************************\n");
	pr_info("index = %d,last index = %d\n",
		ci->index_block, ci->last_index_block);
	/* added for étape 3 */
	if (ci->index_block != ci->last_index_block)
		return -EPERM;

	/* Check if the write can be completed (enough space?) */
	if (pos + len > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;

	bh_old_index = sb_bread(inode->i_sb, ci->index_block);
	if (!bh_old_index)
		return -EIO;
	old_index_block = (struct ouichefs_file_index_block *)
		bh_old_index->b_data;
	new_index = get_free_block(sbi);
	if (!new_index) {
			brelse(bh_old_index);
			return -ENOSPC;
	}
	bh_new_index = sb_bread(inode->i_sb, new_index);
	if (!bh_new_index)
		return -EIO;
	new_index_block = (struct ouichefs_file_index_block *)
		bh_new_index->b_data;

	/* Allouer de nouveaux blocs de données pour l'index bloc */
	for (i = 0; i < inode->i_blocks - 1; i++) {
		new_index_block->blocks[i] = get_free_block(sbi);
		if (!new_index_block->blocks[i]) {
			brelse(bh_old_index);
			brelse(bh_new_index);
			return -ENOSPC;
		}
	}

	pr_info("nbr of data blocks = %lld\n", inode->i_blocks - 1);
	for (i = 0; i < inode->i_blocks - 1; i++) {
		bh_old_bloc = sb_bread(inode->i_sb, old_index_block->blocks[i]);
		/* pr_info("avant old\n"); */
		if (!bh_old_bloc)
			return -EIO;
		bh_new_bloc = sb_bread(inode->i_sb, new_index_block->blocks[i]);
		if (!bh_new_bloc)
			return -EIO;
		pr_debug("depuis %d vers %d\n", old_index_block->blocks[i],
			new_index_block->blocks[i]);
		memcpy(bh_new_bloc->b_data, bh_old_bloc->b_data,
			OUICHEFS_BLOCK_SIZE);
		mark_buffer_dirty(bh_new_bloc);
		brelse(bh_old_bloc);
		brelse(bh_new_bloc);
		pr_info("Le bloc %d est DIRTY\n", new_index_block->blocks[i]);
	}
	new_index_block->prev = ci->index_block;

	pr_info("Je  suis le nouvel index  bloc %d mon prec est %d\n",
		new_index, ci->index_block);
	pr_info(">>>>Le prev de mon prev = %d\n", old_index_block->prev);
	ci->index_block = new_index;
	ci->last_index_block = new_index; /* added for etape 3 */

	mark_inode_dirty(inode);
	brelse(bh);
	/* On fait ca dans le cas ou on ecrase des données qui ne sont pas à
	l'offset courant */
	nr_allocs = max(pos + len, file->f_inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	/* prepare the write */
	err = block_write_begin(mapping, pos, len, flags, pagep,
				ouichefs_file_get_block);
	/* if this failed, reclaim newly allocated blocks */
	if (err < 0) {
		pr_err("%s:%d: newly allocated blocks reclaim not implemented yet\n",
		    __func__, __LINE__);
	}
	brelse(bh_old_index);
	brelse(bh_new_index);
	return err;
}

/*
 * Called by the VFS after writing data from a write() syscall to the page
 * cache. This functions updates inode metadata and truncates the file if
 * necessary.
 */
static int ouichefs_write_end(struct file *file, struct address_space *mapping,
			      loff_t pos, unsigned int len, unsigned int copied,
			      struct page *page, void *fsdata)
{
	int ret;
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh;
	struct ouichefs_file_index_block *index_block;
	int i;

	/* Complete the write() */
	ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
	if (ret < len) {
		pr_err("%s:%d: wrote less than asked... what do I do? nothing for now...\n",
		       __func__, __LINE__);
	} else {
		uint32_t nr_blocks_old = inode->i_blocks;

		/* Update inode metadata */
		inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
		inode->i_mtime = inode->i_ctime = current_time(inode);
		mark_inode_dirty(inode);

		bh = sb_bread(sb, ci->index_block);
		if (!bh)
			return -EIO;
		index_block = (struct ouichefs_file_index_block *)bh->b_data;
		pr_info("L'index bloc du fichier est %d\n", ci->index_block);
		pr_info("Le nombre de blocks du fichier est %lld\n",
			inode->i_blocks);
		pr_info("Liste des nouveaux blocs :");
		for (i = 0; i < inode->i_blocks - 1 ; i++)
			pr_info("Bloc %d\n", index_block->blocks[i]);
		/* If file is smaller than before, free unused blocks */
		if (nr_blocks_old > inode->i_blocks) {
			int i;
			struct buffer_head *bh_index;
			struct ouichefs_file_index_block *index;

			/* Free unused blocks from page cache */
			truncate_pagecache(inode, inode->i_size);

			/* Read index block to remove unused blocks */
			bh_index = sb_bread(sb, ci->index_block);
			if (!bh_index) {
				pr_err("failed truncating '%s'. we just lost %llu blocks\n",
				       file->f_path.dentry->d_name.name,
				       nr_blocks_old - inode->i_blocks);
				goto end;
			}

			index = (struct ouichefs_file_index_block *)
				bh_index->b_data;

			for (i = inode->i_blocks - 1; i < nr_blocks_old - 1;
			     i++) {
				put_block(OUICHEFS_SB(sb), index->blocks[i]);
				index->blocks[i] = 0;
			}
			mark_buffer_dirty(bh_index);
			brelse(bh_index);
		}
	}
end:
	pr_info("***************************************\n");
	return ret;
}

const struct address_space_operations ouichefs_aops = {
	.readpage    = ouichefs_readpage,
	.writepage   = ouichefs_writepage,
	.write_begin = ouichefs_write_begin,
	.write_end   = ouichefs_write_end
};

const struct file_operations ouichefs_file_ops = {
	.owner      = THIS_MODULE,
	.llseek     = generic_file_llseek,
	.read_iter  = generic_file_read_iter,
	.write_iter = generic_file_write_iter
};

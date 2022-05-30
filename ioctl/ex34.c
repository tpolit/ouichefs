#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include "ioctl_ex34.h"
#include "../ouichefs.h"
#include "../bitmap.h"

MODULE_DESCRIPTION("Module ioctl pour pour actualiser la vue courante");
MODULE_AUTHOR("Titouan Polit, Firas Jebari, Mazigh Saoudi");
MODULE_LICENSE("GPL");

static int nb;
static int size_filename = -1;

static long do_version(int ver, char *cible)
{
	struct inode *c_inode;
	/* struct ouichefs_inode *d_inode; */
	struct ouichefs_inode_info *co_inode;
	struct buffer_head *temp_bh;
	struct ouichefs_file_index_block *block;
	struct file *cible_f;
	int nb_prev, current_index, i;
	/* uint32_t inode_block, inode_shift; */

	pr_info("******************ioctl:version*****************\n");
	cible_f = filp_open(cible, O_RDONLY, 0400);
	if (IS_ERR(cible_f)) {
		pr_info("Open error\n");
		return -1;
	}
	pr_info("En cherche la version -%d\n", ver);
	c_inode = cible_f->f_inode;
	co_inode = OUICHEFS_INODE(c_inode);
	pr_info("Before: current index %d, last %d\n",
		co_inode->index_block, co_inode->last_index_block);
	current_index = co_inode->last_index_block;
	for (nb_prev = ver; nb_prev > 0; nb_prev--) {
		/* pr_info("Current index of inode %ld is: %d\n",\
		 c_inode->i_ino, current_index); */
		temp_bh = sb_bread(c_inode->i_sb, current_index);
		if (!temp_bh)
			return -EIO;
		block = (struct ouichefs_file_index_block *)temp_bh->b_data;
		/* arrivé à la 1ére version et on veut quand même reculer
		 plus */
		if (block->prev == -1)
			return -1;
		current_index = block->prev;
		brelse(temp_bh);
	}

	/* mettre à jour l'inode en cache */
	co_inode->index_block = current_index;
	mark_inode_dirty(c_inode);
	pr_info("After: Current index of inode %ld is: %d\n",
		c_inode->i_ino, co_inode->index_block);
	/* verifier les blocs de donnée */
	temp_bh = sb_bread(c_inode->i_sb, co_inode->index_block);
	if (!temp_bh)
		return -EIO;
	block = (struct ouichefs_file_index_block *)temp_bh->b_data;
	pr_info("Liste des blocs de données: ");
	for (i = 0; i < c_inode->i_blocks - 1; i++)
		pr_info("Bloc %d\n", block->blocks[i]);
	brelse(temp_bh);
	fput(cible_f);
	pr_info("******************ioctl:version*****************\n");
	return 0;
}

static long do_rewind(char *cible)
{
	/*
	rendre l'index courant le last index
	tout en liberant tout les blocs utilisé
	par les versions entre now et last(inclus)
	*/
	struct inode *c_inode;
	struct ouichefs_inode_info *co_inode;
	struct buffer_head *temp_bh;
	struct ouichefs_file_index_block *block;
	struct file *cible_f;
	int current_index, tofree_index, i;

	pr_info("******************ioctl:rewind*****************\n");
	cible_f = filp_open(cible, O_RDONLY, 0400);
	if (IS_ERR(cible_f)) {
		pr_info("Open error\n");
		return -1;
	}

	c_inode = cible_f->f_inode;
	co_inode = OUICHEFS_INODE(c_inode);
	pr_info("Current index = %d, last index = %d",
		co_inode->index_block, co_inode->last_index_block);
	current_index = co_inode->last_index_block;
	while (current_index != co_inode->index_block) {
		temp_bh = sb_bread(c_inode->i_sb, current_index);
		if (!temp_bh)
			return -EIO;
		block = (struct ouichefs_file_index_block *)temp_bh->b_data;
		for (i = 0; i < c_inode->i_blocks - 1; i++) {
			pr_info("Bloc %d is now free\n", block->blocks[i]);
			put_block(OUICHEFS_SB(c_inode->i_sb), block->blocks[i]);
			block->blocks[i] = 0;
		}
		tofree_index = current_index;
		current_index = block->prev;
		mark_buffer_dirty(temp_bh);
		brelse(temp_bh);
		put_block(OUICHEFS_SB(c_inode->i_sb), tofree_index);
		pr_info("Bloc %d is now free\n", tofree_index);
	}
	co_inode->last_index_block = co_inode->index_block;
	pr_info("Current index = %d, last index = %d\n",
		co_inode->index_block, co_inode->last_index_block);
	mark_inode_dirty(c_inode);
	pr_info("******************ioctl:rewind*****************\n");
	return 0;
}

static long ul_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct parameters *p;
	struct parameters_rewind *pr;
	int ret;

	switch (cmd) {
	case SIZE:
		ret = copy_from_user(&size_filename, (int *)arg, sizeof(int));
		if (ret)
			pr_err("Read problem, ret = %d\n", ret);
		break;
	case VERSION:
		if (size_filename == -1) {
			pr_err("Size undifined\n");
			return -1;
		}
		p = (struct parameters *)kmalloc(sizeof(struct\
		 parameters) + size_filename, GFP_KERNEL);
		ret = copy_from_user(p, (struct parameters *)arg,\
		 sizeof(struct parameters) + size_filename);
		if (ret)
			pr_err("Read problem, ret = %d\n", ret);
		pr_info("Parameters: version = %d, filename = %s\n",\
		 p->version, p->cible);
		do_version(p->version, p->cible);
		size_filename = -1;
		break;
	case REWIND:
		if (size_filename == -1) {
			pr_err("Size undifined\n");
			return -1;
		}
		pr = (struct parameters_rewind *)kmalloc(sizeof(struct\
		 parameters_rewind) + size_filename, GFP_KERNEL);
		ret = copy_from_user(pr, (struct parameters_rewind *)arg,\
		 sizeof(struct parameters_rewind) + size_filename);
		if (ret)
			pr_err("Read problem, ret = %d\n", ret);
		pr_info("Parameters: filename = %s\n", pr->cible);
		do_rewind(pr->cible);
		size_filename = -1;
		break;
	default:
		pr_info("default\n");
		return -ENOTTY;
	}
	return 0;
}

static int __init current_view_init(void)
{
	static  const struct file_operations file_op = {
		.unlocked_ioctl = ul_ioctl
		};
	nb = register_chrdev(0, "current_view", &file_op);
	pr_info("numero de l'ioctl : %d\n", nb);
	return 0;
}
module_init(current_view_init);

static void __exit current_view_exit(void)
{
	unregister_chrdev(nb, "current_view");
	pr_info("Module déchargé\n");
}
module_exit(current_view_exit);

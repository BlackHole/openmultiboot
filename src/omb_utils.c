/*
 *
 * Copyright (C) 2014 Impex-Sat Gmbh & Co.KG
 * Written by Sandro Cavazzoni <sandro@skanetwork.com>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <mntent.h>
#include <unistd.h>

#include "omb_common.h"
#include "omb_log.h"
#include "omb_utils.h"
#include "omb_branding.h"

#define OMB_FS_MAX 3
static const char *omb_utils_fs_types[OMB_FS_MAX] = { "ext4", "ext3" };

int omb_utils_dir_exists(const char* folder)
{
	DIR *fd = opendir(folder);
	if (fd) {
		closedir(fd);
		return 1;
	}
	return 0;
}

int omb_utils_file_exists(const char* filename)
{
	struct stat st;
	int result = stat(filename, &st);
	return result == 0;
}

void omb_utils_create_dir_tree()
{
	char tmp[255];
	if (!omb_utils_dir_exists(OMB_MAIN_DIR))
		mkdir(OMB_MAIN_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	
	sprintf(tmp, "%s/.kernels", OMB_MAIN_DIR);
	if (!omb_utils_dir_exists(tmp))
		mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

int omb_utils_mount(const char* device, const char* mountpoint)
{
	int i;
	for (i = 0; i < OMB_FS_MAX; i++)
		if (mount(device, mountpoint, omb_utils_fs_types[i], 0, NULL) == 0)
			return OMB_SUCCESS;
	
	return OMB_ERROR;
}

int omb_utils_is_mounted(const char *mountpoint)
{
	FILE* mtab = NULL;
	struct mntent* part = NULL;
	int is_mounted = 0;
	
	if ((mtab = setmntent("/etc/mtab", "r")) != NULL) {
		while ((part = getmntent(mtab)) != NULL) {
			if (part->mnt_dir != NULL
				&& strcmp(part->mnt_dir, mountpoint) == 0) {
					
				is_mounted = 1;
			}
		}
		endmntent(mtab);
	}
	
	return is_mounted;
}

int omb_utils_umount(const char* mountpoint)
{
	return umount(mountpoint) == 0 ? OMB_SUCCESS : OMB_ERROR;
}

void omb_utils_remount_media(omb_device_item *item)
{
	FILE* mtab = NULL;
	struct mntent* part = NULL;
	char media[255];
	char base[255];
	char vol[255];
	sprintf(media, "%s/%s/%s/media", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
	sprintf(base, "%s/%s/%s", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
	sprintf(vol, "%s/%s/%s/etc/init.d/volatile-media.sh", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
	
	if (omb_utils_file_exists(vol)) {
		omb_log(LOG_DEBUG, "%-33s:remount /media into %s", __FUNCTION__, media);
		if (!omb_utils_is_mounted(media))
			if (mount("tmpfs", media, "tmpfs", 0, "size=64k") != 0)
				omb_log(LOG_ERROR, "%-33s: cannot mount %s", __FUNCTION__, media);
	}		
	if ((mtab = setmntent("/etc/mtab", "r")) != NULL) {
		while ((part = getmntent(mtab)) != NULL) {
			if (part->mnt_dir != NULL
				&& strlen(part->mnt_dir) > 6
				&& memcmp(part->mnt_dir, "/media", 6) == 0) {
					char tmp[255];
					sprintf(tmp, "%s/%s", base, part->mnt_dir);
					
					if (omb_utils_umount(part->mnt_dir) == OMB_ERROR)
						omb_log(LOG_WARNING, "%-33s: cannot umount %s", __FUNCTION__, part->mnt_dir);
					
					if (!omb_utils_dir_exists(tmp))
						mkdir(tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

					if (omb_utils_mount(part->mnt_fsname, tmp) == OMB_ERROR)
						omb_log(LOG_WARNING, "%-33s: cannot mount %s", __FUNCTION__, tmp);
				}
		}
		endmntent(mtab);
	}

	if (omb_utils_umount("/media") == OMB_ERROR)
		omb_log(LOG_WARNING, "%-33s: cannot umount /media", __FUNCTION__);
}

int omb_utils_find_and_mount()
{
	struct dirent *dir;
	DIR *fd = opendir(OMB_DEVICES_DIR);
	if (fd) {
		omb_utils_create_dir_tree();
		
		while ((dir = readdir(fd)) != NULL) {
			if (((strlen(dir->d_name) == 7 || strlen(dir->d_name) == 9) && memcmp(dir->d_name, "mmc", 3) == 0) || ((strlen(dir->d_name) == 3 || strlen(dir->d_name) == 4) && memcmp(dir->d_name, "sd", 2) == 0)) {
				char device[255];
				sprintf(device, "%s/%s", OMB_DEVICES_DIR, dir->d_name);
				omb_log(LOG_DEBUG, "%-33s: check device %s", __FUNCTION__, device);
				
				omb_utils_umount(OMB_MAIN_DIR); // just force umount without check
				if (omb_utils_mount(device, OMB_MAIN_DIR) == OMB_SUCCESS) {
					char datadir[255];
					sprintf(datadir, "%s/%s", OMB_MAIN_DIR, OMB_DATA_DIR);
					if (omb_utils_dir_exists(datadir)) {
						omb_log(LOG_DEBUG, "%-33s: found data on device %s", __FUNCTION__, device);
						closedir(fd);
						return OMB_SUCCESS;
					}
					
					if (omb_utils_umount(OMB_MAIN_DIR) == OMB_ERROR)
						omb_log(LOG_ERROR, "%-33s: cannot umount %s", __FUNCTION__, OMB_MAIN_DIR);
				}
			}
		}	
		closedir(fd);
	}
	return OMB_ERROR;
}

omb_device_item *omb_utils_get_images()
{
	struct dirent *dir;
	char datadir[255];
	DIR *fd;
	
	omb_device_item *first = NULL;
	omb_device_item *last = NULL;
	
	omb_log(LOG_DEBUG, "%-33s: discover images", __FUNCTION__);
	
	omb_device_item *item = omb_branding_read_info("", "flash");
	if (item != NULL) {
		if (first == NULL)
			first = item;
		if (last != NULL)
			last->next = item;
		last = item;
	}

	sprintf(datadir, "%s/%s", OMB_MAIN_DIR, OMB_DATA_DIR);
	fd = opendir(datadir);
	if (fd) {
		while ((dir = readdir(fd)) != NULL) {
			if (strlen(dir->d_name) > 0 && dir->d_name[0] != '.') {
				char base_dir[255];
				sprintf(base_dir, "%s/%s", datadir, dir->d_name);

				if (!omb_branding_is_compatible(base_dir)) {
					omb_log(LOG_DEBUG ,"%-33s: skipping image %s", __FUNCTION__, base_dir);
					continue;
				}

				omb_device_item *item = omb_branding_read_info(base_dir, dir->d_name);
				if (item != NULL) {
					if (first == NULL)
						first = item;
					if (last != NULL)
						last->next = item;
					last = item;
				}
			}
		}
		closedir(fd);
	}
	return first;
}

void omb_utils_free_items(omb_device_item *items)
{
	omb_device_item *tmp = items;
	while (tmp) {
		omb_device_item *tmp2 = tmp;
		tmp = tmp->next;
			
		free(tmp2->label);
		free(tmp2->directory);
		free(tmp2->identifier);
		free(tmp2);
	}
}

void omb_utils_update_background(omb_device_item *item)
{
	char tmp[255];
        sprintf(tmp, "%s/usr/share/bootlogo.mvi", item->directory);
	if (omb_utils_file_exists(tmp)) {
            sprintf(tmp, "%s %s/usr/share/bootlogo.mvi", OMB_SHOWIFRAME_BIN, item->directory);
	} else {
	    sprintf(tmp, "%s /usr/share/smartmultiboot/missing-background.mvi", OMB_SHOWIFRAME_BIN);
	}
	system(tmp);
}

void omb_utils_remove_nextboot()
{
	char tmp[255];
	sprintf(tmp, "%s/%s/.%s", OMB_MAIN_DIR, OMB_DATA_DIR, OMB_SETTINGS_NEXTBOOT);
	if(omb_utils_file_exists(tmp)) {
		char cmd[255];
		sprintf(cmd, "rm -rf %s", tmp);
		system(cmd);
	}
}

int omb_utils_gettimer()
{
	char tmp[255];
	sprintf(tmp, "%s/%s/.%s", OMB_MAIN_DIR, OMB_DATA_DIR, OMB_SETTINGS_TIMER);
	if(omb_utils_file_exists(tmp)) {
		char *tmp = omb_utils_read(OMB_SETTINGS_TIMER);
		if (tmp) {
			int ret = atoi(tmp);
			free(tmp);
			return ret;
		}
	}
	return OMB_DEFAULT_TIMER;
}

void omb_utils_setrctype()
{
	char tmp[255];
	sprintf(tmp, "%s/%s/.%s", OMB_MAIN_DIR, OMB_DATA_DIR, OMB_SETTINGS_RCTYPE);
	if(omb_utils_file_exists(tmp)) {
		char *tmp = omb_utils_read(OMB_SETTINGS_RCTYPE);
		if (tmp) {
			int ret = atoi(tmp);
			free(tmp);
			if (ret) {
				char cmd[255];
				sprintf(cmd, "echo %d > /proc/stb/ir/rc/type", ret);
				system(cmd);
			}
		}
	}
}

void omb_utils_save(const char* key, const char* value)
{
	char tmp[255];
	sprintf(tmp, "%s/%s/.%s", OMB_MAIN_DIR, OMB_DATA_DIR, key);
	FILE *fd = fopen(tmp, "w");
	if (fd) {
		fwrite(value, 1, strlen(value), fd);
		fclose(fd);
		sync();
	}
}

int omb_utils_check_lock_menu()
{
	char tmp[255];
	sprintf(tmp, "%s/%s/.bootmenu.lock", OMB_MAIN_DIR, OMB_DATA_DIR);
	if (omb_utils_file_exists(tmp)) {
		omb_log(LOG_DEBUG ,"%-33s: bootmenu disabled!", __FUNCTION__);
		return 1;
	}
	
	return 0;
	
}

char* omb_utils_read(const char *key)
{
	char tmp[255];
	sprintf(tmp, "%s/%s/.%s", OMB_MAIN_DIR, OMB_DATA_DIR, key);
	FILE *fd = fopen(tmp, "r");
	if (fd) {
		char line[1024];
		if (fgets(line, 1024, fd)) {
			strtok(line, "\n");
			char *ret = malloc(strlen(line) + 1);
			strcpy(ret, line);
			fclose(fd);
			return ret;
		}
		fclose(fd);
	}
	return NULL;
}

void omb_utils_save_int(const char* key, int value)
{
	char tmp[255];
	sprintf(tmp, "%d", value);
	omb_utils_save(key, tmp);
}

int omb_utils_read_int(const char *key)
{
	int ret = 0;
	char *tmp = omb_utils_read(key);
	if (tmp) {
		ret = atoi(tmp);
		free(tmp);
	}
	omb_log(LOG_DEBUG, "%-33s: selected %d", __FUNCTION__, ret);
	return ret;
}

void omb_utils_build_platform_wrapper(omb_device_item *item)
{
	FILE *fp;
	char tmp[255];
	char cmd[512];

	sprintf(tmp, "%s/%s/%s/usr/bin/platform-util-wrapper.sh", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
	fp = fopen(tmp,"w");
	fprintf(fp,"%s","#!/bin/sh\n\n");
	fprintf(fp,"%s","export PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin\n");
	fprintf(fp,"%s","/etc/init.d/vuplus-platform-util start\n");
	fprintf(fp,"%s","/etc/init.d/platform-util start\n");
	fprintf(fp,"%s","/etc/init.d/gigablue-platform-util start\n");
	fclose(fp);

	sprintf(cmd, "chmod 0755 %s/%s/%s/usr/bin/platform-util-wrapper.sh", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
	system(cmd);
}

void omb_utils_init_system()
{
	omb_log(LOG_DEBUG, "%-33s: mount /proc", __FUNCTION__);
	if (!omb_utils_is_mounted("/proc"))
		if (mount("proc", "/proc", "proc", 0, NULL) != 0)
			omb_log(LOG_ERROR, "%-33s: cannot mount /proc", __FUNCTION__);
	
	omb_log(LOG_DEBUG, "%-33s: mount /sys", __FUNCTION__);
	if (!omb_utils_is_mounted("/sys"))
		if (mount("sysfs", "/sys", "sysfs", 0, NULL) != 0)
			omb_log(LOG_ERROR, "%-33s: cannot mount /sys", __FUNCTION__);
	
	omb_log(LOG_DEBUG, "%-33s: mount /media", __FUNCTION__);
	if (!omb_utils_is_mounted("/media"))
		if (mount("tmpfs", "/media", "tmpfs", 0, "size=64k") != 0)
			omb_log(LOG_ERROR, "%-33s: cannot mount /media", __FUNCTION__);

	omb_log(LOG_DEBUG, "%-33s: run volatile media", __FUNCTION__);
	system(OMB_VOLATILE_MEDIA_BIN);

	omb_log(LOG_DEBUG, "%-33s: run mdev", __FUNCTION__);
	system(OMB_MDEV_BIN);
	
	// we really need this sleep?? :( - (wait for mdev to finalize)
	sleep(5);
}

/*
 **
 * by Meo.
 * We don't need to load drivers when we have not to show the bootmenu (force = 0).
 * So we split the original load_modules function to load drivers
 * only when needed.
 **
 */

void omb_utils_prepare_destination(omb_device_item *item)
{	
	omb_log(LOG_DEBUG, "%-33s: prepare destination", __FUNCTION__);

	if (item != NULL && strcmp(item->identifier, "flash") != 0)
	{
		char dev[255];
		char proc[255];
		char sys[255];
		char omb[255];
		char omb_plugin[255];
		sprintf(dev, "%s/%s/%s/dev", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		sprintf(proc, "%s/%s/%s/proc", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		sprintf(sys, "%s/%s/%s/sys", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		sprintf(omb, "%s/%s/%s/%s", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier, OMB_MAIN_DIR);
		sprintf(omb_plugin, "%s/%s/%s/%s", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier, OMB_PLUGIN_DIR);
		
		if (!omb_utils_is_mounted(dev))
			if (mount("/dev", dev, NULL, MS_BIND, NULL) != 0)
				omb_log(LOG_ERROR, "%-33s: cannot bind /dev to %s", __FUNCTION__, dev);
		
		if (!omb_utils_is_mounted(proc))
			if (mount("/proc", proc, NULL, MS_BIND, NULL) != 0)
				omb_log(LOG_ERROR, "%-33s: cannot bind /proc to %s", __FUNCTION__, proc);
		
		if (!omb_utils_is_mounted(sys))
			if (mount("/sys", sys, NULL, MS_BIND, NULL) != 0)
				omb_log(LOG_ERROR, "%-33s: cannot bind /sys to %s", __FUNCTION__, sys);

		if (!omb_utils_dir_exists(omb))
			mkdir(omb, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

		if (!omb_utils_is_mounted(omb))
			if (mount(OMB_MAIN_DIR, omb, NULL, MS_BIND, NULL) != 0)
				omb_log(LOG_ERROR, "%-33s: cannot bind %s to %s", __FUNCTION__, OMB_MAIN_DIR, omb);
				
		if (!omb_utils_dir_exists(omb_plugin))
			mkdir(omb_plugin, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

		if (!omb_utils_is_mounted(omb_plugin))
			if (mount(OMB_PLUGIN_DIR, omb_plugin, NULL, MS_BIND, NULL) != 0)
				omb_log(LOG_ERROR, "%-33s: cannot bind %s to %s", __FUNCTION__, OMB_PLUGIN_DIR, omb_plugin);
	}
	
}

void omb_utils_load_modules(omb_device_item *item)
{
	int i;
	
	omb_log(LOG_DEBUG, "%-33s: load modules", __FUNCTION__);

	if (item == NULL || strcmp(item->identifier, "flash") == 0) {
		system(OMB_MODUTILS_BIN);
	}
	else {
		
		char cmd[512];
		
		sprintf(cmd, "%s %s/%s/%s %s", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier, OMB_MODUTILS_BIN);
		system(cmd);
	}
	
	for (i = 0; i < 200; i++) {
		if (omb_utils_file_exists(OMB_VIDEO_DEVICE))
			break;
		
		usleep(10000);
	}

#ifdef __sh__
	omb_log(LOG_DEBUG, "%-33s: load lirc", __FUNCTION__);
	if (item == NULL || strcmp(item->identifier, "flash") == 0) {
		system("/etc/init.d/populate-volatile.sh start");
		system("/etc/init.d/lircd start");
	}
	else {
		char cmd[255];
		sprintf(cmd, "%s %s/%s/%s /etc/init.d/populate-volatile.sh start", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		system(cmd);
		sprintf(cmd, "%s %s/%s/%s /etc/init.d/lircd start", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		system(cmd);
	}
#endif

}

/*
 **
 * by Meo.
 * OpenGles modules are loaded at the end of rcS
 * So we need additional stuffs and a different procedure.
 **
 */
void omb_utils_load_modules_gl(omb_device_item *item)
{	
	omb_log(LOG_DEBUG, "%-33s: load platform-util", __FUNCTION__);
	
	int i;

	if (item == NULL || strcmp(item->identifier, "flash") == 0) 
	{
		system("/etc/init.d/mountall.sh start");
		system("/etc/init.d/modload.sh start");
		system("/etc/init.d/modutils.sh start");
		system("/etc/init.d/populate-volatile.sh start");
		system("/etc/init.d/bootmisc.sh start");
		system("/etc/init.d/vuplus-platform-util start");
		system("/etc/init.d/platform-util start");
		system("/etc/init.d/gigablue-platform-util start");
	}

	else 
	{
		
		char tmp[255];
		char cmd[512];
			

		sprintf(tmp, "%s/%s/%s/etc/init.d/mountrun.sh", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		if(omb_utils_file_exists(tmp)) {
			sprintf(cmd, "%s %s/%s/%s /etc/init.d/mountrun.sh start", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
			system(cmd);
		}

		sprintf(cmd, "%s %s/%s/%s /etc/init.d/mountall.sh start", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		system(cmd);
		
		sprintf(tmp, "%s/%s/%s/etc/init.d/modload.sh", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		if(omb_utils_file_exists(tmp)) {
			sprintf(cmd, "%s %s/%s/%s /etc/init.d/modload.sh start", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
			system(cmd);
		}
		
		sprintf(cmd, "%s %s/%s/%s /etc/init.d/modutils.sh start", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		system(cmd);
		
		sprintf(cmd, "%s %s/%s/%s /etc/init.d/populate-volatile.sh start", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		system(cmd);

		sprintf(cmd, "%s %s/%s/%s /etc/init.d/bootmisc.sh start", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		system(cmd);

// prevent missing path in chroot
		omb_utils_build_platform_wrapper(item);

		sprintf(cmd, "%s %s/%s/%s /usr/bin/platform-util-wrapper.sh", OMB_CHROOT_BIN, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		system(cmd);
		
	}

	for (i = 0; i < 500; i++) {
		if (omb_utils_file_exists(OMB_VIDEO_DEVICE))
			break;
		
		usleep(10000);
	}
}

void omb_utils_backup_kernel(omb_device_item *item)
{
	char cmd[512];

	if (!item)
		return;
	
	omb_log(LOG_DEBUG, "%-33s: backup kernel for image '%s'", __FUNCTION__, item->identifier);
#ifdef OMB_DREAMBOX
	sprintf(cmd, "%s %s -nof %s/%s/.kernels/%s.bin", OMB_NANDDUMP_BIN, OMB_KERNEL_MTD, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
#elif defined(OMB_MMCBLK)
	if (omb_utils_file_exists(OMB_PROC_STB))
		sprintf(cmd, "%s if=%s of=%s/%s/.kernels/%s.bin", OMB_DD_BIN, OMB_KERNEL_MTD, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
#else
	sprintf(cmd, "%s %s -f %s/%s/.kernels/%s.bin", OMB_NANDDUMP_BIN, OMB_KERNEL_MTD, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
#endif
	system(cmd);
//	omb_log(LOG_DEBUG, "omb_utils_backup_kernel(): cmd: %s");
}

void omb_utils_restore_kernel(omb_device_item *item)
{
	char cmd[512];
	char filename[255];

	if (!item)
		return;
	
	sprintf(filename, "%s/%s/.kernels/%s.bin", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
	if (omb_utils_file_exists(filename)) {
#ifndef OMB_MMCBLK
		omb_log(LOG_DEBUG, "%-33s: erasing MTD", __FUNCTION__);
		sprintf(cmd, "%s %s 0 0", OMB_FLASHERASE_BIN, OMB_KERNEL_MTD);
		system(cmd);
#endif
	
		omb_log(LOG_DEBUG, "%-33s: restore kernel for image '%s'", __FUNCTION__, item->identifier);
#ifdef OMB_DREAMBOX
		sprintf(cmd, "%s -mno %s %s/%s/.kernels/%s.bin", OMB_NANDWRITE_BIN, OMB_KERNEL_MTD, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
#elif defined(OMB_MMCBLK)
		if (omb_utils_file_exists(OMB_PROC_STB))
			sprintf(cmd, "%s of=%s if=%s/%s/.kernels/%s.bin", OMB_DD_BIN, OMB_KERNEL_MTD, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
#else
		sprintf(cmd, "%s -pm %s %s/%s/.kernels/%s.bin", OMB_NANDWRITE_BIN, OMB_KERNEL_MTD, OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
#endif
		system(cmd);
	}
}

void omb_utils_reboot()
{
	omb_utils_sysvinit(NULL, "6");
}

void omb_utils_sysvinit(omb_device_item *item, const char *args)
{
	if (item == NULL || strcmp(item->identifier, "flash") == 0) {
		execl(OMB_SYSVINIT_BIN, OMB_SYSVINIT_BIN, args, NULL);
	}
	else {
		char path[255];
		char udev[255];
		sprintf(path, "%s/%s/%s", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
		sprintf(udev, "%s/%s/%s/%s", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier, "/etc/init.d/udev");
		if (omb_utils_file_exists(udev))
				system("/etc/init.d/mdev stop");
				
		execl(OMB_CHROOT_BIN, OMB_CHROOT_BIN, path, OMB_INIT_BIN, args, NULL);
	}
}

void smb_utils_initrd_prepare(omb_device_item *item)
{
/*
ROOTDEVICE/UUID and ROOTDIR HAVE TO BE CONFIGURED BY THIS TOOL
this allow to load the root as soon as the device appear.... 

TODO: if timeout happened we could try to do a failover by remounting flash, removing the default file and reboot 
*/


	FILE *fp;
	char tmp[255];
	char cmd[512];
	char uuid[255];
	char rootdev[255]; //keep it dynamic so the executable is more portable
	char kerneldev[255]; //keep it dynamic so the executable is more portable

	sprintf(cmd, "python /usr/lib/enigma2/python/Plugins/Extensions/OpenMultiboot/open-multiboot-branding-helper.py /usr/lib/enigma2/python/ machine_mtd_root");
	fp = popen(cmd, "r");
	while (fgets(rootdev, sizeof(rootdev)-1, fp) != NULL) {
		break;
	}
	rootdev[strcspn(rootdev, "\n")] = 0;
printf("ROOTDEV=%s\n",rootdev);
	pclose(fp);

	sprintf(cmd, "python /usr/lib/enigma2/python/Plugins/Extensions/OpenMultiboot/open-multiboot-branding-helper.py /usr/lib/enigma2/python/ machine_mtd_kernel");
	fp = popen(cmd, "r");
	while (fgets(kerneldev, sizeof(kerneldev)-1, fp) != NULL) {
		break;
	}
	kerneldev[strcspn(kerneldev, "\n")] = 0;
printf("KERNELDEV=%s\n",kerneldev);
	pclose(fp);

// i know better shell then c.. for now we can use shell code
//get device
	sprintf(cmd, "sed -n \"s-\\\([^ ]*\\) %s .*-\\1-1p\" /proc/mounts", OMB_MAIN_DIR);
	fp = popen(cmd, "r");
printf("CMD=%s\n",cmd);
	while (fgets(tmp, sizeof(tmp)-1, fp) != NULL) {
		break;
	}
	tmp[strcspn(tmp, "\n")] = 0;
printf("DEVICE=%s\n",tmp);
	pclose(fp);

//get blkid
	sprintf(cmd, "blkid | sed -n \"s-^%s:.*UUID=\\\"\\\([^\\\"]*\\\)\\\" .*-\\1-p\"", tmp);
//printf("CMD=%s\n",cmd);

	fp = popen(cmd, "r");
	while (fgets(tmp, sizeof(tmp)-1, fp) != NULL) {
		strcpy(uuid,tmp);
	}
	uuid[strcspn(uuid, "\n")] = 0;
printf("UUID=%s\n",uuid);
	pclose(fp);


printf("removing /tmp/kexec_helper\n");
	sprintf(cmd, "rm -Rf /tmp/kexec_helper");
	system(cmd);
printf("creating /tmp/kexec_helper\n");
	sprintf(cmd, "mkdir /tmp/kexec_helper");
	system(cmd);
printf("change dir /tmp/kexec_helper and unpack cpio\n");
	sprintf(cmd, "cd /tmp/kexec_helper && gzip -c -d %s | cpio -i --make-directories", "/usr/share/smartmultiboot/initramfs.cpio.gz");
	system(cmd);


printf("CREATING INIT FILE\n");
	sprintf(tmp, "/tmp/kexec_helper/init");
	fp = fopen(tmp,"w");
	fprintf(fp,"#!/bin/sh -x\n");
	fprintf(fp,"ROOTDIR=/%s/%s\n",OMB_DATA_DIR, item->identifier);
	fprintf(fp,"UUID=%s\n",uuid);
	fprintf(fp,"echo \"SMARTMULTIBOOT: BEGIN INITRAMFS\"\n");
	fprintf(fp,"\n");
	fprintf(fp,"PATH=/sbin:/bin:/usr/sbin:/usr/bin\n");
	fprintf(fp,"\n");
	fprintf(fp,"do_mount_fs() {\n");
	fprintf(fp,"        grep -q \"$1\" /proc/filesystems || return\n");
	fprintf(fp,"        test -d \"$2\" || mkdir -p \"$2\"\n");
	fprintf(fp,"        mount -t \"$1\" \"$1\" \"$2\"\n");
	fprintf(fp,"}\n");
	fprintf(fp,"\n");
	fprintf(fp,"do_mknod() {\n");
	fprintf(fp,"        test -e \"$1\" || mknod \"$1\" \"$2\" \"$3\" \"$4\"\n");
	fprintf(fp,"}\n");
	fprintf(fp,"\n");
	fprintf(fp,"mkdir -p /proc\n");
	fprintf(fp,"mount -t proc proc /proc\n");
	fprintf(fp,"\n");
	fprintf(fp,"do_mount_fs sysfs /sys\n");
	fprintf(fp,"do_mount_fs devtmpfs /dev\n");
	fprintf(fp,"\n");
	fprintf(fp,"mkdir -p /run\n");
	fprintf(fp,"mkdir -p /var/run\n");
	fprintf(fp,"\n");
	fprintf(fp,"do_mknod /dev/console c 5 1\n");
	fprintf(fp,"do_mknod /dev/null c 1 3\n");
	fprintf(fp,"do_mknod /dev/zero c 1 5\n");
	fprintf(fp,"\n");
	fprintf(fp,"blkid\n");
	fprintf(fp,"echo \"SMARTMULTIBOOT: looking for device $UUID\"\n");
	fprintf(fp,"cnt=0\n");
	fprintf(fp,"while [[ $cnt -lt 30 ]];\n");
	fprintf(fp,"do\n");
	fprintf(fp,"	blkdev=$(blkid | sed -n \"s/^\\([^:]*\\):.*UUID=\\\"*$UUID\\\"*.*/\\1/p\")\n");
	fprintf(fp,"	if [[ \"x$blkdev\" != \"x\" ]]; then\n");
	fprintf(fp,"		break\n");
	fprintf(fp,"	fi\n");
	fprintf(fp,"	sleep 1\n");
	fprintf(fp,"	let cnt=cnt+1\n");
	fprintf(fp,"done\n");
	fprintf(fp,"if [[ \"x$blkdev\" = \"x\" ]]; then\n");
	fprintf(fp,"	echo \"SMARTMULTIBOOT: uuid match failed\"\n");
	fprintf(fp,"fi\n");
	fprintf(fp,"\n");
	fprintf(fp,"sleep 1\n");
	fprintf(fp,"mkdir -p /omb\n");
	fprintf(fp,"mount $blkdev /omb\n");
	fprintf(fp,"\n");
	fprintf(fp,"mkdir -p /mnt/target\n");
	fprintf(fp,"mount -o bind /omb$ROOTDIR /mnt/target || echo \"error mounting $ROOTDIR\"\n");
	fprintf(fp,"\n");
	fprintf(fp,"mount -o bind /sys /mnt/target/sys\n");
	fprintf(fp,"mount -o bind /proc /mnt/target/proc\n");
	fprintf(fp,"mount -o bind /dev /mnt/target/dev\n");
	fprintf(fp,"mkdir -p /mnt/target/omb\n");
	fprintf(fp,"mount -o bind /omb /mnt/target/omb\n");

//mount flash in /media/flash on target - needed later to mount the plugin
	fprintf(fp,"mkdir -p /mnt/target/omb_media/flash\n");
	fprintf(fp,"mount -o ro,defaults /dev/%s /mnt/target/omb_media/flash\n", rootdev);

	fprintf(fp,"mkdir -p /mnt/target/omb\n");
	fprintf(fp,"\n");
	//protect kernel in flash
//	fprintf(fp,"mount -o bind /omb/%s/.kernels/%s.bin /mnt/target/%s\n", OMB_DATA_DIR, item->identifier, OMB_KERNEL_MTD);
	fprintf(fp,"mount -o bind /omb/%s/.kernels/%s.bin /mnt/target/dev/%s\n", OMB_DATA_DIR, item->identifier, kerneldev);

	fprintf(fp,"mkdir -p /mnt/target%s\n", OMB_PLUGIN_DIR);
	fprintf(fp,"mount -o bind /mnt/target/omb_media/flash%s /mnt/target%s\n", OMB_PLUGIN_DIR, OMB_PLUGIN_DIR);

	fprintf(fp,"echo \"SMARTMULTIBOOT: END INITRAMFS\"\n");
	fprintf(fp,"exec switch_root /mnt/target /sbin/init\n");
	//fprintf(fp,"exec switch_root /mnt/target /bin/sh\n");
	fclose(fp);

printf("chmod /tmp/kexec_helper/init\n");
	sprintf(cmd, "chmod 0755 /tmp/kexec_helper/init");
	system(cmd);
printf("creating /tmp/kexec_helper.cpio.gz\n");
	sprintf(cmd, "cd /tmp/kexec_helper && find . | cpio -o -H newc | gzip -f -9 -n -c > /tmp/kexec_helper.cpio.gz && sync && sync && sync");
	system(cmd);
printf("end prepare\n");


}

void smb_utils_kexec(omb_device_item *item)
{
	char cmd[512];
	char filename[255];

	if (!item)
		return;
	
	sprintf(filename, "%s/%s/.kernels/%s.bin", OMB_MAIN_DIR, OMB_DATA_DIR, item->identifier);
	if (omb_utils_file_exists(filename)) {
		sprintf(cmd, "/usr/sbin/kexec -d -l %s --initrd=\"/tmp/kexec_helper.cpio.gz\" --command-line=\"$(cat /proc/cmdline)\"", filename);
		printf("%s\n",cmd);
		system(cmd);
		system("/usr/sbin/kexec -d -e");
	}
}

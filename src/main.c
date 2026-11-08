/*
 * PS Vita RE savedata
 * Copyright (C) 2021, Princess of Sleeping
 *
 * Changes (C) 2026, delon5 (https://github.com/delon5/reSavedata):
 *  - every game is redirected, its folder created on first start
 *    (the original redirected only titles whose folder the user had made);
 *  - the folder lives on the official Memory Card: uma0:resavedata when a
 *    storage plugin put another drive (SD2Vita, PSVSD, USB stick) on ux0
 *    and the card on uma0, else ux0:resavedata as before. The drive is
 *    decided when the first game starts, when the mounts are settled, and
 *    kept until reboot.
 *
 * Licensed under the GPL-3.0, see LICENSE.
 */

#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>
#include <psp2kern/fios2.h>
#include <taihen.h>

#define HookImport(module_name, library_nid, func_nid, func_name) taiHookFunctionImportForKernel(KERNEL_PID, &func_name ## _ref, module_name, library_nid, func_nid, func_name ## _patch)
#define HookOffset(modid, seg_index, thumb, offset, func_name) taiHookFunctionOffsetForKernel(KERNEL_PID, &func_name ## _ref, modid, seg_index, offset, thumb, func_name ## _patch);

#define RE_SAVEDATA_DIR "resavedata"

/* "uma0:resavedata" or "ux0:resavedata", empty until decided */
static char rs_base[0x40];

static int dir_exists(const char *path){
	SceIoStat stat;
	return ksceIoGetstat(path, &stat) >= 0;
}

static void ensure_dir(const char *path){
	if(!dir_exists(path))
		ksceIoMkdir(path, 0666);
}

/* The folder's drive: the official Memory Card. With a storage plugin
 * (StorageMgr, YAMT, gamesd, usbmc) the card is uma0 and another drive is
 * ux0; without one the card is ux0 and there is no uma0. */
static const char *rs_path(void){
	if(rs_base[0] != '\0')
		return rs_base;

	snprintf(rs_base, sizeof(rs_base), "%s%s", dir_exists("uma0:") ? "uma0:" : "ux0:", RE_SAVEDATA_DIR);
	ensure_dir(rs_base);
	return rs_base;
}

/* system processes (the shell, "main", and the NPXS… applications) keep
 * the system's savedata handling */
static int is_system_title(SceUID pid, const char *titleid){
	(void)pid;
	return strcmp(titleid, "main") == 0 || strncmp(titleid, "NPXS", 4) == 0;
}

/* the title's folder and its sce_sys, created on first use */
static void ensure_title_dirs(const char *titleid){
	char path[0x100];

	snprintf(path, sizeof(path), "%s/%s", rs_path(), titleid);
	ensure_dir(path);
	snprintf(path, sizeof(path), "%s/%s/sce_sys", rs_path(), titleid);
	ensure_dir(path);
}

int write_file(const char *path, const void *data, size_t size){

	SceUID fd;

	fd = ksceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
	if(fd < 0){
		return fd;
	}

	ksceIoWrite(fd, data, size);
	ksceIoClose(fd);

	return 0;
}

const char sdslot_magic[] = {
	'S', 'D', 'S', 'L',
	0x0, 0x0, 0x0, 0x0,
	0x0, 0x1, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0
};

/* a zero-filled file of the given size, if it does not exist yet */
static void write_blank_file(const char *path, SceSize size, const char *magic, int magic_len, SceSize alloc_size){
	SceUID memid;
	void *mem_base = NULL;

	if(dir_exists(path))
		return;

	memid = ksceKernelAllocMemBlock("ReSavedata", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, alloc_size, NULL);
	if(memid < 0)
		return;

	ksceKernelGetMemBlockBase(memid, &mem_base);
	if(magic_len > 0)
		memcpy(mem_base, magic, magic_len);

	write_file(path, mem_base, size);

	ksceKernelFreeMemBlock(memid);
}

tai_hook_ref_t sceAppMgrInitSafemem_ref;
int sceAppMgrInitSafemem_patch(SceUID pid, int a2, const char *savedata0_sce_sys_path, char *a4, SceSize safemem_size){

	char path[0x100], titleid[0x20], process_path_savedata0[0x20];

	memset(titleid, 0, sizeof(titleid));
	memset(process_path_savedata0, 0, sizeof(process_path_savedata0));

	ksceKernelSysrootGetProcessTitleId(pid, titleid, sizeof(titleid));

	if(titleid[0] == '\0' || is_system_title(pid, titleid)){
		goto tai_continue;
	}

	/* the system's own sce_sys keeps a safemem.dat too */
	snprintf(path, sizeof(path), "%s/%s", savedata0_sce_sys_path, "safemem.dat");
	write_blank_file(path, safemem_size, NULL, 0, safemem_size);

	ksceFiosKernelOverlayResolveSync(pid, 1, "savedata0:", process_path_savedata0, sizeof(process_path_savedata0));

	if(strncmp(process_path_savedata0, "savedata0:", 10) == 0){
		goto tai_continue;
	}

	ensure_title_dirs(titleid);

	snprintf(path, sizeof(path), "%s/%s/sce_sys/%s", rs_path(), titleid, "safemem.dat");
	write_blank_file(path, safemem_size, NULL, 0, safemem_size);

	snprintf(path, sizeof(path), "%s/%s/sce_sys/%s", rs_path(), titleid, "sdslot.dat");
	write_blank_file(path, 0x40400, sdslot_magic, sizeof(sdslot_magic), 0x50000);

tai_continue:
	return TAI_CONTINUE(int, sceAppMgrInitSafemem_ref, pid, a2, savedata0_sce_sys_path, a4, safemem_size);
}

tai_hook_ref_t ksceFiosKernelOverlayAddForProcess_ref;
int ksceFiosKernelOverlayAddForProcess_patch(SceUID pid, SceFiosOverlay *overlay, SceFiosOverlayID *outID){

	SceFiosOverlay loc_overlay;

	if(strcmp(overlay->dst, "savedata0:") == 0){

		char titleid[0x20];
		memset(titleid, 0, sizeof(titleid));
		ksceKernelSysrootGetProcessTitleId(pid, titleid, sizeof(titleid));

		if(titleid[0] != '\0' && !is_system_title(pid, titleid)){
			memcpy(&loc_overlay, overlay, sizeof(loc_overlay));

			/* every game: its folder is made here if it is new */
			ensure_title_dirs(titleid);
			snprintf(loc_overlay.src, sizeof(loc_overlay.src), "%s/%s/", rs_path(), titleid);

			overlay = &loc_overlay;
		}
	}

	return TAI_CONTINUE(int, ksceFiosKernelOverlayAddForProcess_ref, pid, overlay, outID);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args){

	tai_module_info_t info;
	info.size = sizeof(info);

	if(taiGetModuleInfoForKernel(KERNEL_PID, "SceAppMgr", &info) < 0)
		return SCE_KERNEL_START_FAILED;

	switch(info.module_nid){
	case 0x94CEFE4B: // 3.55
		HookOffset(info.modid, 0, 1, 0x2DF9C, sceAppMgrInitSafemem);
		break;
	case 0xDBB29DB7: // 3.60
		HookOffset(info.modid, 0, 1, 0x2E0C4, sceAppMgrInitSafemem);
		break;
	case 0x1C9879D6: // 3.65
		HookOffset(info.modid, 0, 1, 0x2E0AC, sceAppMgrInitSafemem);
		break;
	case 0x54E2E984: // 3.67
	case 0xC3C538DE: // 3.68
		HookOffset(info.modid, 0, 1, 0x2E0BC, sceAppMgrInitSafemem);
		break;
	case 0x321E4852: // 3.69
	case 0x700DA0CD: // 3.70
	case 0xF7846B4E: // 3.71
	case 0xA8E80BA8: // 3.72
	case 0xB299D195: // 3.73
		HookOffset(info.modid, 0, 1, 0x2E0E4, sceAppMgrInitSafemem);
		break;
	default:
		return SCE_KERNEL_START_FAILED;
		break;
	}

	HookImport("SceAppMgr", 0x54D6B9EB, 0x17E65A1C, ksceFiosKernelOverlayAddForProcess);

	return SCE_KERNEL_START_SUCCESS;
}

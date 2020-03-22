#include <stdio.h>
#include "utils/logger.c"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/fs_functions.h"
#include <iosuhax_devoptab.h>
#include <iosuhax.h>
#include <sys/dirent.h>

#define MAX_CONSOLE_LINES_TV    27
#define MAX_CONSOLE_LINES_DRC   18

#define SLCCMPT_DEV_PATH "/dev/slccmpt01"
#define SLCCMPT_MOUNT_PATH "/vol/storage_slccmpt01"

#define SLCCMPT_MODE 0x666

static char* consoleArrayTv[MAX_CONSOLE_LINES_TV];
static char* consoleArrayDrc[MAX_CONSOLE_LINES_DRC];

static void console_print_pos(int x, int y, const char *format, ...)
{
	char* tmp = NULL;

	va_list va;
	va_start(va, format);
	if((vasprintf(&tmp, format, va) >= 0) && tmp)
	{
		if(strlen(tmp) > 79)
			tmp[79] = 0;

		OSScreenPutFontEx(0, x, y, tmp);
		OSScreenPutFontEx(1, x, y, tmp);

	}
	va_end(va);

	if(tmp)
		free(tmp);
}

void console_printf(int newline, const char *format, ...)
{
	char* tmp = NULL;

	va_list va;
	va_start(va, format);
	if((vasprintf(&tmp, format, va) >= 0) && tmp)
	{
		if(newline)
		{
			if(consoleArrayTv[0])
				free(consoleArrayTv[0]);
			if(consoleArrayDrc[0])
				free(consoleArrayDrc[0]);

			for(int i = 1; i < MAX_CONSOLE_LINES_TV; i++)
				consoleArrayTv[i-1] = consoleArrayTv[i];

			for(int i = 1; i < MAX_CONSOLE_LINES_DRC; i++)
				consoleArrayDrc[i-1] = consoleArrayDrc[i];
		}
		else
		{
			if(consoleArrayTv[MAX_CONSOLE_LINES_TV-1])
				free(consoleArrayTv[MAX_CONSOLE_LINES_TV-1]);
			if(consoleArrayDrc[MAX_CONSOLE_LINES_DRC-1])
				free(consoleArrayDrc[MAX_CONSOLE_LINES_DRC-1]);

			consoleArrayTv[MAX_CONSOLE_LINES_TV-1] = NULL;
			consoleArrayDrc[MAX_CONSOLE_LINES_DRC-1] = NULL;
		}

		if(strlen(tmp) > 79)
			tmp[79] = 0;

		consoleArrayTv[MAX_CONSOLE_LINES_TV-1] = (char*)malloc(strlen(tmp) + 1);
		if(consoleArrayTv[MAX_CONSOLE_LINES_TV-1])
			strcpy(consoleArrayTv[MAX_CONSOLE_LINES_TV-1], tmp);

		consoleArrayDrc[MAX_CONSOLE_LINES_DRC-1] = (tmp);
	}
	va_end(va);

	// Clear screens
	OSScreenClearBufferEx(0, 0);
	OSScreenClearBufferEx(1, 0);


	for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
	{
		if(consoleArrayTv[i])
			OSScreenPutFontEx(0, 0, i, consoleArrayTv[i]);
	}

	for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
	{
		if(consoleArrayDrc[i])
			OSScreenPutFontEx(1, 0, i, consoleArrayDrc[i]);
	}

	OSScreenFlipBuffersEx(0);
	OSScreenFlipBuffersEx(1);
}

char* stringReplace(char *search, char *replace, char *string) {
	char* searchStart = strstr(string, search);
	if(searchStart == NULL) {
		return string;
	}

	char* tempString = (char*) malloc(strlen(string) * sizeof(char));
	if(tempString == NULL) {
		return NULL;
	}

	strcpy(tempString, string);

	int len = searchStart - string;
	string[len] = '\0';

	strcat(string, replace);

	len += strlen(search);
	strcat(string, (char*)tempString+len);

	free(tempString);
	
	return string;
}

bool chmod_cancelled = 0;

int CheckCancel(void)
{
	int vpadError = -1;
	VPADData vpad;

	VPADRead(0, &vpad, 1, &vpadError);

	if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & VPAD_BUTTON_B))
	{
		return 1;
	}
	return 0;
}

int fsaFd = -1;

int chmodRecursive(char *pPath, int mode)
{
	if (fsaFd < 0)
	{
		console_printf(1, "Invalid fsaFd %i", fsaFd);
		sleep(2);
		return -1;
	}

	DIR *d;
	struct dirent *dir;
	d = opendir(pPath);
	if (d == NULL)
	{
		console_printf(1, "Cannot open %s", pPath);
		return -1;
	}

	while ((dir = readdir(d)) != NULL)
	{
		if (CheckCancel())
		{
			chmod_cancelled = 1;
			return 1;
		}

		char path[FS_MAX_FULLPATH_SIZE];
		char rootpath[FS_MAX_FULLPATH_SIZE];

		if(strcmp(dir->d_name, "..") == 0 || strcmp(dir->d_name, ".") == 0)
			continue;

		snprintf(path, sizeof(path), "%s/%s", pPath, dir->d_name);

		strcpy(rootpath, path);
		stringReplace("dev:", SLCCMPT_MOUNT_PATH, rootpath);

		int ret = IOSUHAX_FSA_ChangeMode(fsaFd, rootpath, mode);
		if (ret < 0)
		{
			console_printf(1, "chmod error %i for %s", ret, dir->d_name);
		} 
		else
		{
			console_printf(1, "chmod successful for %s", dir->d_name);
		}

		if (dir->d_type & DT_DIR)
		{
			chmodRecursive(path, mode);
		}      
	}
	closedir(d);

	return 0;
}

//just to be able to call async
void someFunc(void *arg)
{
	(void)arg;
}

static int mcp_hook_fd = -1;
int MCPHookOpen()
{
	//take over mcp thread
	mcp_hook_fd = MCP_Open();
	if(mcp_hook_fd < 0)
		return -1;
	IOS_IoctlAsync(mcp_hook_fd, 0x62, (void*)0, 0, (void*)0, 0, someFunc, (void*)0);
	//let wupserver start up
	sleep(1);
	if(IOSUHAX_Open("/dev/mcp") < 0)
		return -1;
	return 0;
}

void MCPHookClose()
{
	if(mcp_hook_fd < 0)
		return;
	//close down wupserver, return control to mcp
	IOSUHAX_Close();
	//wait for mcp to return
	sleep(1);
	MCP_Close(mcp_hook_fd);
	mcp_hook_fd = -1;
}

int Menu_Main(void)
{
	InitOSFunctionPointers();
	InitSocketFunctionPointers();
	InitFSFunctionPointers();
	InitVPadFunctionPointers();

	VPADInit();

	for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
		consoleArrayTv[i] = NULL;

	for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
		consoleArrayDrc[i] = NULL;

	// Prepare screen
	int screen_buf0_size = 0;

	// Init screen and screen buffers
	OSScreenInit();
	screen_buf0_size = OSScreenGetBufferSizeEx(0);
	OSScreenSetBufferEx(0, (void *)0xF4000000);
	OSScreenSetBufferEx(1, (void *)(0xF4000000 + screen_buf0_size));

	OSScreenEnableEx(0, 1);
	OSScreenEnableEx(1, 1);

	// Clear screens
	OSScreenClearBufferEx(0, 0);
	OSScreenClearBufferEx(1, 0);

	// Flip buffers
	OSScreenFlipBuffersEx(0);
	OSScreenFlipBuffersEx(1);

	int vpadError = -1;
	VPADData vpad;

	int res = IOSUHAX_Open(NULL);
	if (res < 0)
		res = MCPHookOpen();
	if(res < 0)
	{
		console_printf(1, "IOSUHAX_open failed: %i\n", res);
		sleep(2);
		return 0;
	}

	fsaFd = IOSUHAX_FSA_Open();
	if(fsaFd < 0)
	{
		console_printf(1, "IOSUHAX_FSA_Open failed: %i\n", fsaFd);
		sleep(2);
		return 0;
	}

	console_print_pos(0, 0, "slccmpt fixer v2 by GaryOderNichts");

	console_print_pos(0, 2, "Press A to set the correct permissions");
	console_print_pos(0, 3, "Hold B to cancel");

	OSScreenFlipBuffersEx(0);
	OSScreenFlipBuffersEx(1);

	while (1)
	{
		VPADRead(0, &vpad, 1, &vpadError);

		if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & VPAD_BUTTON_A))
		{
			OSScreenClearBufferEx(0, 0);
			OSScreenClearBufferEx(1, 0);
			OSScreenFlipBuffersEx(0);
			OSScreenFlipBuffersEx(1);

			int res = mount_fs("dev", fsaFd, SLCCMPT_DEV_PATH, SLCCMPT_MOUNT_PATH);
			if(res < 0)
			{
				console_printf(1, "%i Mount of %s to %s failed", res, SLCCMPT_DEV_PATH, SLCCMPT_MOUNT_PATH);
				sleep(5);
				break;
			}

			chmodRecursive("dev:", SLCCMPT_MODE);
			
			console_printf(1, chmod_cancelled ? "Cancelled!" : "Finished!");

			unmount_fs("dev");

			sleep(5);

			break;
		}

		if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & (VPAD_BUTTON_B | VPAD_BUTTON_HOME)))
		{
			break;
		}
	}
	
	console_printf(1, "Exiting...");

	for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
	{
		if(consoleArrayTv[i])
			free(consoleArrayTv[i]);
	}

	for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
	{
		if(consoleArrayDrc[i])
			free(consoleArrayDrc[i]);
	}

	IOSUHAX_FSA_Close(fsaFd);

	if(mcp_hook_fd >= 0)
		MCPHookClose();
	else
		IOSUHAX_Close();

	return 0;
}


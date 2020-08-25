#include <bootinfo.h>	/* For the boot information structure. */

/* This is the main entry point for the game kernel. At the point this function
 * is called we know very little of the things surrounding our kernel. For that
 * we need to inspect the structure provided to us by the bootloader. */
extern "C" void _start(bootinfo_t *info)
{
	auto early_console = info->console;
}

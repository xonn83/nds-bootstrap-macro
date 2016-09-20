/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "hook.h"
#include "common.h"
#include "sdengine_bin.h"

extern unsigned long cheat_engine_size;
extern unsigned long intr_orig_return_offset;

extern const u8 cheat_engine_start[]; 

static const u32 handlerStartSig[5] = {
	0xe92d4000, 	// push {lr}
	0xe3a0c301, 	// mov  ip, #0x4000000
	0xe28cce21,		// add  ip, ip, #0x210
	0xe51c1008,		// ldr	r1, [ip, #-8]
	0xe3510000		// cmp	r1, #0
};

static const u32 handlerEndSig[4] = {
	0xe59f1008, 	// ldr  r1, [pc, #8]	(IRQ Vector table address)
	0xe7910100,		// ldr  r0, [r1, r0, lsl #2]
	0xe59fe004,		// ldr  lr, [pc, #4]	(IRQ return address)
	0xe12fff10		// bx   r0
};

static const int MAX_HANDLER_SIZE = 50;

static u32* hookInterruptHandler (u32* addr, size_t size) {
	u32* end = addr + size/sizeof(u32);
	int i;
	
	// Find the start of the handler
	while (addr < end) {
		if ((addr[0] == handlerStartSig[0]) && 
			(addr[1] == handlerStartSig[1]) && 
			(addr[2] == handlerStartSig[2]) && 
			(addr[3] == handlerStartSig[3]) && 
			(addr[4] == handlerStartSig[4])) 
		{
			break;
		}
		addr++;
	}
	
	if (addr >= end) {
		return NULL;
	}
	
	// Find the end of the handler
	for (i = 0; i < MAX_HANDLER_SIZE; i++) {
		if ((addr[i+0] == handlerEndSig[0]) && 
			(addr[i+1] == handlerEndSig[1]) && 
			(addr[i+2] == handlerEndSig[2]) && 
			(addr[i+3] == handlerEndSig[3])) 
		{
			break;
		}
	}
	
	if (i >= MAX_HANDLER_SIZE) {
		return NULL;
	}
	
	// Now find the IRQ vector table
	// Make addr point to the vector table address pointer within the IRQ handler
	addr = addr + i + sizeof(handlerEndSig)/sizeof(handlerEndSig[0]);
	
	// Use relative and absolute addresses to find the location of the table in RAM
	u32 tableAddr = addr[0];
	u32 returnAddr = addr[1];
	u32* actualReturnAddr = addr + 2;
	u32* actualTableAddr = actualReturnAddr + (tableAddr - returnAddr)/sizeof(u32);
	
	// The first entry in the table is for the Vblank handler, which is what we want
	return actualTableAddr;
}

int hookNds (const tNDSHeader* ndsHeader, const u32* cheatData, u32* cheatEngineLocation, u32* sdEngineLocation) {
	u32 oldReturn;
	u32 oldSync;
	u32* hookLocation = NULL;

	if (!hookLocation) {
		hookLocation = hookInterruptHandler ((u32*)ndsHeader->arm7destination, ndsHeader->arm7binarySize);
	}
	
	if (!hookLocation) {
		return ERR_HOOK;
	}
	
	oldReturn = *hookLocation;
	oldSync = hookLocation[16];
	
	*hookLocation = (u32)sdEngineLocation+0xC;
	hookLocation[16] = (u32)sdEngineLocation+0xC;
	
	copyLoop (sdEngineLocation, (u32*)sdengine_bin, sdengine_bin_size);
	
	u32 sdmmc_intr_orig_return_offset = *((u32*)sdEngineLocation+0x4);
	u32 sdmmc_intr_sync_orig_return_offset = *((u32*)sdEngineLocation+0x8);
	
	
	sdEngineLocation [sdmmc_intr_orig_return_offset/sizeof(u32)] = oldReturn;
	sdEngineLocation [sdmmc_intr_sync_orig_return_offset/sizeof(u32)] = oldSync;
	
	return ERR_NONE;
}



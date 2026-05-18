#pragma once


#define TVERSION							((WORD) 0x2918)	// Protocol version

#define SM_BASE        (0x1581)  // System message base for server
#define MW_BASE        (0x9001)  // Map server <-> World server message base
#define DM_BASE        (0x5891)  // DB message base for server
#define CS_LOGIN       (0x1987)  // Login server <-> Client message base
#define CS_MAP		   (0x5280)  // Map server <-> Client message base
#define CT_CONTROL     (0x9301)  // Control Server Base
#define CT_PATCH       (0x4201)  // Patch Server <-> Client
#define RW_RELAY       (0x9999)  // Relay Server <-> Client
#define CS_CUSTOM      (0x3312)
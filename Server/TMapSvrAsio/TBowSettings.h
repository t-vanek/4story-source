#pragma once
#if defined(BOW_COMPILE_MODE) || defined(BR_COMPILE_MODE)

#define BOW_INVENTORY_ID                     (BYTE) 16

#define BOW_BP_PER_10SEC					 (BYTE) 10

#define BOW_DEFUGEL_STATUE					 (WORD) 32454
#define BOW_CRAXION_STATUE					 (WORD) 32455
#define BOW_GUARD_SPAWNID                    (WORD) 32456

#define BOW_SWITCH_ID                        (WORD) 476

#define BOW_ITEM_MIN_LEVEL                   (BYTE) 19

#define KILLER_SP_REWARD                     (BYTE) 5
#define PARTY_SP_REWARD                      (BYTE) 2
#define STATUE_SP_REWARD                     (BYTE) 8
#define KILLER_BP_REWARD                     (BYTE) KILLER_SP_REWARD * 20
#define PARTY_BP_REWARD                      (BYTE) PARTY_SP_REWARD  * 20
#define STATUE_BP_REWARD                     (WORD) STATUE_SP_REWARD * 20

#define BOW_RESPAWN_MEDAL_BASE				 (BYTE) 20
#define BOW_RESPAWN_MEDAL_ADDER				 (BYTE) 5

#define MEDAL_BP_RATE						 (BYTE) 100

#define DEFUGEL_EQUIP_EFFECT                 (BYTE) 9
#define CRAXION_EQUIP_EFFECT                 (BYTE) 7

#define MIN_SP_TO_GET_REWARD                 (BYTE) 20
#define BOW_REWARD_ITEM_ID                   (WORD) 7983
#define BOW_REWARD_MAX_LOGOUT_MIN            (BYTE) 3

#define BOW_FIRST_PLACE_TITLE_ID             (WORD) 150

#define BOW_FAILSAFE_SPAWNID                 (WORD) 15003
#define BOW_BP_BASE							 (WORD) 500

#if defined(BR_COMPILE_MODE)
#include "BRSettings.h"
#endif

#endif
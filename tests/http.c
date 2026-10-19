/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main player_app_main
#include "../src/player.c"
#undef main
#include <assert.h>
static uint32_t test_clock(void){return 0;}
int main(int argc,char**argv){assert(argc==2);pSDL_GetTicks=test_clock;fetch_start(argv[1]);assert(fetch_active);pthread_join(fetch.thread,NULL);assert(fetch.result==0&&fetch.http_status==200&&fetch.data);assert(parse_list(fetch.data,fetch.origin)==1);assert(strstr(items[0].uri,"/base/canal.mp4"));free(fetch.data);printf("Authenticated playlist headers + redirect base: PASS\n");return 0;}

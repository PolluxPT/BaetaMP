/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main player_app_main
#include "../src/player.c"
#undef main
#include <assert.h>
static uint32_t ticks(void){return 0;}
static void wait_id(const char*property,int64_t expected){
    for(int i=0;i<500;i++){int64_t id=0;m_wait_event(mpv,.02);if(m_get_property(mpv,property,MPV_FORMAT_INT64,&id)>=0&&id==expected)return;}
    assert(!"track selection did not take effect");
}
int main(int argc,char**argv){
    assert(argc==4);pSDL_GetTicks=ticks;snprintf(app,sizeof(app),"%s",argv[1]);smoke=1;assert(mpv_load());m_request_log_messages(mpv,"no");assert(m_set_option_string(mpv,"vo","null")>=0);
    set_flag("pause",1);const char*load[]={"loadfile",argv[2],"replace",NULL};assert(m_command(mpv,load)>=0);
    int loaded=0;for(int i=0;i<500;i++){mpv_event*e=m_wait_event(mpv,.02);if(e->event_id==MPV_EVENT_FILE_LOADED){loaded=1;break;}}assert(loaded);
    playing=file_loaded=1;page=1;selected=3;tracks_open(0);assert(page==9&&track_count==3);
    int choice=-1;for(int i=1;i<track_count;i++)if(strstr(tracks[i].name,"Inglês"))choice=i;assert(choice>0);int64_t id=tracks[choice].id;tracks_select(choice);wait_id("sid",id);assert(page==1&&selected==3&&playing);
    tracks_open(0);tracks_select(0);int disabled=0;for(int i=0;i<100;i++){char*sid=NULL;m_wait_event(mpv,.02);if(m_get_property(mpv,"sid",MPV_FORMAT_STRING,&sid)>=0){disabled=!strcmp(sid,"no");m_free(sid);if(disabled)break;}}assert(disabled);
    tracks_open(1);assert(track_count==2);choice=-1;for(int i=0;i<track_count;i++)if(strstr(tracks[i].name,"Inglês"))choice=i;assert(choice>=0);id=tracks[choice].id;tracks_select(choice);wait_id("aid",id);
    const char*external[]={"sub-add",argv[3],"select",NULL};assert(m_command(mpv,external)>=0);
    int found=0;for(int i=0;i<100;i++){m_wait_event(mpv,.02);tracks_open(0);for(int j=1;j<track_count;j++)if(strstr(tracks[j].detail,"Externa"))found=1;tracks_close();if(found)break;}assert(found);
    m_terminate_destroy(mpv);mpv=NULL;puts("Real MKV: language/title metadata, subtitle/audio IDs, subtitles off, external SRT: PASS");return 0;
}

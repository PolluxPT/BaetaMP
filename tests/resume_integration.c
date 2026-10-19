/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main player_app_main
#include "../src/player.c"
#undef main
#include <assert.h>
static uint32_t ticks(void){return 0;}
static double wait_position(double minimum){
    for(int n=0;n<200;n++){
        mpv_event*event=m_wait_event(mpv,0.02);(void)event;
        double position=0;if(m_get_property(mpv,"time-pos",MPV_FORMAT_DOUBLE,&position)>=0&&position>=minimum)return position;
    }
    return -1;
}
int main(int argc,char**argv){
    assert(argc==3);pSDL_GetTicks=ticks;snprintf(app,sizeof(app),"%s",argv[1]);snprintf(uri,sizeof(uri),"%s",argv[2]);
    smoke=1;assert(mpv_load());smoke=0;assert(md5_sum);
    uint8_t digest[16];md5_sum(digest,(const uint8_t*)"abc",3);const uint8_t expected[]={0x90,0x01,0x50,0x98,0x3c,0xd2,0x4f,0xb0,0xd6,0x96,0x3f,0x7d,0x28,0xe1,0x7f,0x72};assert(!memcmp(digest,expected,16));
    assert(m_set_option_string(mpv,"vo","null")>=0);set_flag("pause",1);
    const char*load[]={"loadfile",uri,"replace","start=4.200000",NULL};assert(m_command(mpv,load)>=0);
    double position=wait_position(4.0);assert(position>=4&&position<5);playing=1;save_position();
    char path[4096];assert(resume_path(uri,path,sizeof(path)));assert(access(path,F_OK)==0);assert(resume_position(uri)>=4&&resume_position(uri)<5);
    const char*stop[]={"stop",NULL};assert(m_command(mpv,stop)>=0);playing=0;
    // Automatic resume is disabled: even with a watch-later record, start=0 wins.
    const char*restart[]={"loadfile",uri,"replace","start=0.000000",NULL};assert(m_command(mpv,restart)>=0);
    position=wait_position(0);assert(position>=0&&position<1);
    assert(m_command(mpv,stop)>=0);
    const char*old=getenv("RGDS_LEGACY_APP");assert(old);char previous[4096],old_path[4096];size_t prefix=strlen(app);assert(!strncmp(uri,app,prefix));
    int length=snprintf(previous,sizeof(previous),"%s%s",old,uri+prefix);assert(length>0&&(size_t)length<sizeof(previous));
    assert(resume_path(previous,old_path,sizeof(old_path)));assert(rename(path,old_path)==0);
    assert(resume_position(uri)>=4&&resume_position(uri)<5);resume_clear(uri);assert(access(old_path,F_OK)<0);
    m_terminate_destroy(mpv);mpv=NULL;
    puts("Real libmpv MD5/watch-later compatibility, explicit resume/start zero and migrated path: PASS");return 0;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main player_app_main
#include "../src/player.c"
#undef main
#include <assert.h>
static uint32_t ticks(void){return 0;}
static mpv_node root;
static mpv_node rows[12],fields[12][8];
static mpv_node_list maps[12],array;
static int requests,commands,freed,fail,shown,hidden;
static char property[32],value[32];
static char*keys[]={"type","id","lang","title","codec","selected","external","forced"};
static void row(int n,const char*type,int64_t id,const char*lang,const char*title,int active,int external){
    fields[n][0]=(mpv_node){.format=MPV_FORMAT_STRING,.u.string=(char*)type};fields[n][1]=(mpv_node){.format=MPV_FORMAT_INT64,.u.int64=id};
    fields[n][2]=(mpv_node){.format=MPV_FORMAT_STRING,.u.string=(char*)lang};fields[n][3]=(mpv_node){.format=MPV_FORMAT_STRING,.u.string=(char*)title};
    fields[n][4]=(mpv_node){.format=MPV_FORMAT_STRING,.u.string="aac"};fields[n][5]=(mpv_node){.format=MPV_FORMAT_FLAG,.u.flag=active};
    fields[n][6]=(mpv_node){.format=MPV_FORMAT_FLAG,.u.flag=external};fields[n][7]=(mpv_node){.format=MPV_FORMAT_FLAG,.u.flag=1};
    maps[n]=(mpv_node_list){.num=8,.keys=keys,.values=fields[n]};rows[n]=(mpv_node){.format=MPV_FORMAT_NODE_MAP,.u.list=&maps[n]};
}
static int get(mpv_handle*h,const char*p,mpv_format format,void*out){(void)h;assert(!strcmp(p,"track-list")&&format==MPV_FORMAT_NODE);requests++;if(fail)return -1;*(mpv_node*)out=root;return 0;}
static void release(mpv_node*n){(void)n;freed++;}
static int cmd(mpv_handle*h,uint64_t id,const char*const*args){(void)h;(void)id;assert(!strcmp(args[0],"set"));snprintf(property,sizeof(property),"%s",args[1]);snprintf(value,sizeof(value),"%s",args[2]);commands++;return 0;}
static void show(SDL_Window*w){(void)w;shown++;}
static void hide(SDL_Window*w){(void)w;hidden++;}
int main(void){
    pSDL_GetTicks=ticks;m_get_property=get;m_free_node_contents=release;m_command_async=cmd;mpv=(mpv_handle*)&root;
    pSDL_ShowWindow=show;pSDL_RaiseWindow=show;pSDL_HideWindow=hide;
    row(0,"video",1,"","",0,0);row(1,"sub",2,"eng","SDH",0,0);row(2,"sub",27,"pt-BR","Português Brasil",1,0);row(3,"sub",105,"","Film.srt",0,1);row(4,"audio",7,"por","Stereo",1,0);row(5,"audio",90,"eng","Commentary",0,0);
    array=(mpv_node_list){.num=6,.values=rows};root=(mpv_node){.format=MPV_FORMAT_NODE_ARRAY,.u.list=&array};
    playing=file_loaded=1;page=2;selected=4;button(2);assert(page==9&&track_count==4&&selected==2&&tracks[2].id==27&&!tracks[0].active);assert(strstr(tracks[2].name,"Português")&&strstr(tracks[2].detail,"pt-BR"));assert(strstr(tracks[3].name,"Film.srt")&&strstr(tracks[3].detail,"Externa"));
    activate(1);assert(commands==1&&!strcmp(property,"sid")&&!strcmp(value,"2")&&page==2&&selected==4&&playing);
    button(2);activate(0);assert(commands==2&&!strcmp(value,"no")&&playing);button(3);assert(track_audio&&track_count==2&&selected==0);activate(1);assert(commands==3&&!strcmp(property,"aid")&&!strcmp(value,"90"));
    options_open();selected=6;activate(6);assert(page==9);button(1);assert(page==5&&selected==6&&playing);activate(7);assert(track_audio);button(7);assert(page==5&&selected==6&&playing);options_close();
    fail=1;int before=freed;tracks_open(0);assert(page==2&&freed==before);fail=0;file_loaded=0;before=requests;tracks_open(0);assert(requests==before&&page==2);file_loaded=1;
    language=1;tracks_open(0);assert(!strcmp(tracks[0].name,"Off")&&strstr(tracks[1].name,"English"));button(1);
    // Pagination, no channel seek while picking, and touch targets track IDs.
    for(int i=0;i<12;i++){row(i,"audio",100+i,"eng","Audio",i==9,0);}array.num=12;tracks_open(1);assert(selected==9&&nav_count()==13);navigate(-1,0);assert(selected==2);navigate(1,0);assert(selected==9);before=commands;button(4);assert(commands==before&&page==9);click(100,150+70+10);assert(!strcmp(value,"108")&&page==2&&selected==4);
    // Lower-screen playback exposes the menu temporarily, then returns to video.
    screenmode=1;tracks_open(1);assert(page==9&&shown&&hidden);button(1);assert(page==2&&playing);screenmode=0;
    array.num=0;tracks_open(1);assert(track_count==0&&nav_count()==1);activate(0);assert(page==2);tracks_open(0);assert(track_count==1&&tracks[0].active);activate(-1);assert(page==9);button(1);
    char temp[]="/tmp/baeta-tracks-XXXXXX";assert(mkdtemp(temp));snprintf(app,sizeof(app),"%s",temp);char config[4096];path_join(config,sizeof(config),temp,"config");assert(mkdir(config,0700)==0);
    page=2;selected=0;count=1;snprintf(items[0].title,sizeof(items[0].title),"Channel");snprintf(items[0].uri,sizeof(items[0].uri),"https://example.org/live");options_open();selected=7;tracks_open(1);assert(page==9);save_menu_session();playing=0;page=0;count=0;restore_menu_session();assert(page==2&&count==1&&selected==0);rmdir(config);rmdir(temp);
    assert(requests==freed+1);puts("Track IDs, filtering, languages, flags, external SRT, off, cancel, paging, touch, lower screen: PASS");return 0;
}

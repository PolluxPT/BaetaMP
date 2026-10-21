/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include "sdl-api.h"
#include "mpv/client.h"
#include "mpv/render.h"
#include "mpv/render_gl.h"
#include "curl/curl.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "translations.h"

#define MAX_ITEMS 2048
#define MAX_FAV 128
typedef struct { char title[256], uri[4096]; int folder; } Item;
static Item items[MAX_ITEMS], favorites[MAX_FAV];
static int count, selected, page, running=1, playing, paused, screenmode, font_size=36, volume=65, image_mode=1, controls_selected;
static char playback_error[192];
static int warnings_logged;
static int failure_pending;
static int option_page,option_selected;
static const char*image_names[]={"proporção original","ecrã cheio (esticar)","preencher (cortar)"};
static int fav_count, keyboard_mode, keysel, shiftkey, fetch_active, preview, smoke, file_loaded;
static int touchfd=-1, keysfd=-1, tx,ty,tdown,tprev,tslot,backlight_saved=-1;
static uint32_t volume_repeat;
static int lower_saved=-1, lower_off, stop_pending, awakefd=-1;
static int touch_inhibited;
static const char *upper_light="/sys/class/backlight/backlight1/brightness", *lower_light="/sys/class/backlight/backlight/brightness";
static const char *volume_node="/sys/class/anbernic_misc/openbor_volume";
static uint32_t stop_deadline, awake_tick;
static double calibration[6]={1,0,0,0,1,0};
static char app[4096], directory[4096], uri[4096], title[256], url[4096], note[256];
static char playlist_origin[4096];
static uint32_t note_until, axis_until, redraw_until;
static uint32_t stats_at, smoke_began;
static int stress, stress_stops, stress_wait;
static int shutting_down, test_eof;
static uint32_t stress_next;
static SDL_Window *menu, *video;
static SDL_Renderer *renderer, *vrenderer;
static SDL_Texture *vtexture;
static void *glcontext, *font, *smallfont, *largefont;
static unsigned char *pixels;
static mpv_handle *mpv;
static void (*md5_sum)(uint8_t*,const uint8_t*,size_t);
static mpv_render_context *render_context;
static int software_video;
static volatile sig_atomic_t interrupted;
static atomic_int render_pending;
static void (*gl_clear_color)(float,float,float,float);
static void (*gl_clear)(unsigned int);
static void (*gl_disable)(unsigned int);
static void (*gl_bind_framebuffer)(unsigned int,unsigned int);
static void (*gl_viewport)(int,int,int,int);
static void (*gl_color_mask)(unsigned char,unsigned char,unsigned char,unsigned char);

#define EXTRA(ret,name,args) static ret (*p##name) args
EXTRA(int,SDL_SetHint,(const char*,const char*));
EXTRA(void,SDL_DisableScreenSaver,(void));EXTRA(void,SDL_EnableScreenSaver,(void));
EXTRA(int,SDL_IsScreenSaverEnabled,(void));EXTRA(void,SDL_HideWindow,(SDL_Window*));
EXTRA(int,SDL_ShowCursor,(int));EXTRA(int,SDL_GL_SetAttribute,(int,int));
EXTRA(void*,SDL_GL_CreateContext,(SDL_Window*));EXTRA(void,SDL_GL_DeleteContext,(void*));
EXTRA(int,SDL_GL_SetSwapInterval,(int));EXTRA(int,SDL_RenderSetLogicalSize,(SDL_Renderer*,int,int));
EXTRA(int,SDL_RenderSetClipRect,(SDL_Renderer*,const SDL_Rect*));
static int (*ttfInit)(void);static void *(*ttfOpen)(const char*,int);
static SDL_Surface *(*ttfRender)(void*,const char*,SDL_Color);
static void (*ttfClose)(void*);static void (*ttfQuit)(void);

#define MP(ret,name,args) static ret (*m_##name) args
MP(mpv_handle*,create,(void));MP(int,initialize,(mpv_handle*));
MP(int,set_option_string,(mpv_handle*,const char*,const char*));
MP(int,set_property,(mpv_handle*,const char*,mpv_format,void*));
MP(int,get_property,(mpv_handle*,const char*,mpv_format,void*));
MP(int,command,(mpv_handle*,const char*const*));
MP(int,command_async,(mpv_handle*,uint64_t,const char*const*));
MP(mpv_event*,wait_event,(mpv_handle*,double));MP(void,terminate_destroy,(mpv_handle*));MP(void,free,(void*));MP(void,free_node_contents,(mpv_node*));
MP(int,render_context_create,(mpv_render_context**,mpv_handle*,mpv_render_param*));
MP(int,render_context_render,(mpv_render_context*,mpv_render_param*));
MP(void,render_context_free,(mpv_render_context*));
MP(void,render_context_set_update_callback,(mpv_render_context*,void(*)(void*),void*));
MP(uint64_t,render_context_update,(mpv_render_context*));
MP(void,render_context_report_swap,(mpv_render_context*));
MP(int,request_log_messages,(mpv_handle*,const char*));

typedef struct { char *data;size_t length;char address[4096],origin[4096];pthread_t thread;atomic_int done,cancel;int result;long http_status; } Fetch;
static Fetch fetch;
static CURL *(*c_init)(void);static CURLcode (*c_setopt)(CURL*,CURLoption,...);
static CURLcode (*c_perform)(CURL*);static void (*c_cleanup)(CURL*);
static int (*c_global_init)(long);
static struct curl_slist *(*c_append)(struct curl_slist*,const char*);
static void (*c_list_free)(struct curl_slist*);
static CURLcode (*c_getinfo)(CURL*,CURLINFO,...);
typedef struct {char url[4096],ua[1024],referer[2048],origin[1024],cookie[2048],authorization[2048];} HttpRequest;
static const char *default_user_agent="Mozilla/5.0 (X11; Linux aarch64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36 BaetaMP/0.4";

static void notify(const char *s){snprintf(note,sizeof(note),"%s",s);note_until=pSDL_GetTicks()+6000;}
static void sig_handler(int sig){(void)sig;interrupted=1;}
static void path_join(char *out,size_t capacity,const char *base,const char *name){
    size_t a=strlen(base),b=strlen(name);
    if(a+b+2>capacity){out[0]=0;return;}
    memcpy(out,base,a);out[a++]='/';memcpy(out+a,name,b+1);
}
static int read_num(const char *path){int v=-1;FILE*f=fopen(path,"r");if(f){if(fscanf(f,"%d",&v)!=1)v=-1;fclose(f);}return v;}
static void write_num(const char *path,int v){if(v<0||v>255)return;FILE*f=fopen(path,"w");if(f){fprintf(f,"%d\n",v);fclose(f);}}
static void restore_light(void){
    if(backlight_saved>=0){write_num(upper_light,backlight_saved);backlight_saved=-1;}
    if(lower_saved>=0){write_num(lower_light,lower_saved);lower_saved=-1;}
    lower_off=0;
}
static void toggle_lower(void){
    if(!playing||screenmode||stop_pending)return;
    const char *path=lower_light;
    if(!lower_off){
        lower_saved=read_num(path);
        if(lower_saved<0){notify(tr("Não foi possível controlar o ecrã inferior."));return;}
        write_num(path,0);lower_off=1;touch_inhibited=tdown;
        fprintf(stderr,"SELECT: ecrã inferior apagado (guardado %d).\n",lower_saved);
    }else{
        write_num(path,lower_saved>0?lower_saved:70);lower_off=0;
        fprintf(stderr,"SELECT: ecrã inferior aceso.\n");
    }
}
static void awake_stop(void){
    if(awakefd>=0){ioctl(awakefd,UI_DEV_DESTROY);close(awakefd);awakefd=-1;fprintf(stderr,"Keepawake: dispositivo temporario removido.\n");}
}
static void awake_start(void){
    if(awakefd>=0)return;
    const char*protocol=getenv("RGDS_IDLE_INHIBIT_PROTOCOL");
    if(!protocol||strcmp(protocol,"0"))return;
    awakefd=open("/dev/uinput",O_WRONLY|O_NONBLOCK|O_CLOEXEC);
    if(awakefd<0)awakefd=open("/dev/input/uinput",O_WRONLY|O_NONBLOCK|O_CLOEXEC);
    if(awakefd<0){fprintf(stderr,"Keepawake: uinput indisponivel, errno=%d.\n",errno);return;}
    int ok=ioctl(awakefd,UI_SET_EVBIT,EV_KEY)==0&&ioctl(awakefd,UI_SET_EVBIT,EV_SYN)==0;
    ok=ok&&ioctl(awakefd,UI_SET_KEYBIT,KEY_F24)==0;
    // Keyboard classification only; these advertised keys are never emitted.
    ok=ok&&ioctl(awakefd,UI_SET_KEYBIT,KEY_A)==0&&ioctl(awakefd,UI_SET_KEYBIT,KEY_Z)==0&&ioctl(awakefd,UI_SET_KEYBIT,KEY_SPACE)==0&&ioctl(awakefd,UI_SET_KEYBIT,KEY_ENTER)==0;
    struct uinput_setup setup={0};setup.id.bustype=BUS_USB;setup.id.vendor=0x1209;setup.id.product=0x3568;snprintf(setup.name,UINPUT_MAX_NAME_SIZE,"MediaPlayer RGDS Keep Awake");
    ok=ok&&ioctl(awakefd,UI_DEV_SETUP,&setup)==0&&ioctl(awakefd,UI_DEV_CREATE)==0;
    if(!ok){fprintf(stderr,"Keepawake: criacao falhou, errno=%d.\n",errno);close(awakefd);awakefd=-1;return;}
    awake_tick=pSDL_GetTicks()+1000;
    fprintf(stderr,"Keepawake: fallback temporario F24; compositor sem idle-inhibit.\n");
}
static void awake_pump(void){
    if(!playing||awakefd<0||pSDL_GetTicks()<awake_tick)return;
    struct input_event e[4];memset(e,0,sizeof(e));
    e[0].type=EV_KEY;e[0].code=KEY_F24;e[0].value=1;
    e[1].type=EV_SYN;e[1].code=SYN_REPORT;
    e[2].type=EV_KEY;e[2].code=KEY_F24;e[2].value=0;
    e[3].type=EV_SYN;e[3].code=SYN_REPORT;
    if(write(awakefd,e,sizeof(e))!=(ssize_t)sizeof(e)){fprintf(stderr,"Keepawake: falha ao renovar atividade.\n");awake_stop();}
    awake_tick=pSDL_GetTicks()+15000;
}
static int is_url(const char*s){const char *prefix[]={"http://","https://","rtsp://","rtmp://","rtmps://","udp://","rtp://",NULL};for(int i=0;prefix[i];i++)if(!strncasecmp(s,prefix[i],strlen(prefix[i])))return 1;return 0;}
static int hex_digit(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static int decode_option(char*out,size_t cap,const char*in){size_t n=0;for(size_t i=0;in[i];i++){unsigned char c=(unsigned char)in[i];if(c=='%'&&in[i+1]&&in[i+2]){int a=hex_digit(in[i+1]),b=hex_digit(in[i+2]);if(a>=0&&b>=0){c=(unsigned char)(a*16+b);i+=2;}}else if(c=='+')c=' ';if(c<32||c==127||n+1>=cap)return 0;out[n++]=c;}out[n]=0;return 1;}
static int append_http_option(char*options,size_t cap,const char*key,const char*value){
    char encoded[4096];size_t n=0;const char*hex="0123456789ABCDEF";
    for(const unsigned char*p=(const unsigned char*)value;*p;p++){
        if(*p<32||*p==127)return 0;
        if((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||strchr("-._~",*p)){if(n+1>=sizeof(encoded))return 0;encoded[n++]=*p;}
        else{if(n+3>=sizeof(encoded))return 0;encoded[n++]='%';encoded[n++]=hex[*p>>4];encoded[n++]=hex[*p&15];}
    }
    encoded[n]=0;size_t have=strlen(options),need=strlen(key)+n+1+(have?1:0);if(have+need>=cap)return 0;
    if(have)strcat(options,"&");
    strcat(options,key);strcat(options,"=");strcat(options,encoded);return 1;
}
static int http_request(const char*source,HttpRequest*r){
    memset(r,0,sizeof(*r));if(strlen(source)>=sizeof(r->url))return 0;snprintf(r->url,sizeof(r->url),"%s",source);snprintf(r->ua,sizeof(r->ua),"%s",default_user_agent);
    char*pipe=strchr(r->url,'|');if(!pipe||!is_url(source))return 1;*pipe++=0;char*save=NULL;
    for(char*pair=strtok_r(pipe,"&",&save);pair;pair=strtok_r(NULL,"&",&save)){
        char*equal=strchr(pair,'=');if(!equal)continue;*equal++=0;char key[96],value[4096];if(!decode_option(key,sizeof(key),pair)||!decode_option(value,sizeof(value),equal))return 0;
        char*dest=NULL;size_t size=0;
        if(!strcasecmp(key,"User-Agent")||!strcasecmp(key,"http-user-agent")){dest=r->ua;size=sizeof(r->ua);}
        else if(!strcasecmp(key,"Referer")||!strcasecmp(key,"Referrer")||!strcasecmp(key,"http-referrer")){dest=r->referer;size=sizeof(r->referer);}
        else if(!strcasecmp(key,"Origin")){dest=r->origin;size=sizeof(r->origin);}
        else if(!strcasecmp(key,"Cookie")||!strcasecmp(key,"Cookies")){dest=r->cookie;size=sizeof(r->cookie);}
        else if(!strcasecmp(key,"Authorization")){dest=r->authorization;size=sizeof(r->authorization);}
        if(dest){size_t length=strlen(value);if(length>=size)return 0;memcpy(dest,value,length+1);}
    }
    return 1;
}
static int merge_http_options(char*uri,size_t cap,const char*defaults){
    if(!defaults[0])return 1;
    char joined[4096];const char*pipe=strchr(uri,'|');size_t core=pipe?(size_t)(pipe-uri):strlen(uri),a=strlen(defaults),b=pipe?strlen(pipe+1):0;
    if(core+a+b+3>=cap||core+a+b+3>=sizeof(joined))return 0;
    memcpy(joined,uri,core);joined[core++]='|';memcpy(joined+core,defaults,a);core+=a;
    if(b){joined[core++]='&';memcpy(joined+core,pipe+1,b);core+=b;}
    joined[core]=0;memcpy(uri,joined,core+1);return 1;
}
static const char *extension(const char*s){const char*p=strrchr(s,'.');return p?p:"";}
static int is_list(const char*s){return !strcasecmp(extension(s),".m3u")||!strcasecmp(extension(s),".m3u8");}
static int is_video(const char*s){const char *exts[]={".mp4",".mkv",".avi",".mov",".webm",".m4v",".mpg",".mpeg",".ts",".m2ts",".flv",".wmv",NULL};for(int i=0;exts[i];i++)if(!strcasecmp(extension(s),exts[i]))return 1;return is_list(s);}
static void settings_save(void){char p[4096],t[4096];path_join(p,sizeof(p),app,"config/settings.txt");path_join(t,sizeof(t),app,"config/settings.new");FILE*f=fopen(t,"w");if(!f)return;fprintf(f,"%d %d %d %d %d\n",screenmode,font_size,volume,image_mode,language);if(fclose(f)==0)rename(t,p);}
static void settings_load(void){char p[4096];path_join(p,sizeof(p),app,"config/settings.txt");FILE*f=fopen(p,"r");int s,z,v,m,l;language=0;if(f){int fields=fscanf(f,"%d %d %d %d %d",&s,&z,&v,&m,&l);if(fields>=3){if(s>=0&&s<=1)screenmode=s;if(z>=20&&z<=64)font_size=z;if(v>=0&&v<=100)volume=v;if(fields>=4&&m>=0&&m<=2)image_mode=m;if(fields>=5&&l>=0&&l<=1)language=l;}fclose(f);}}
// Locale-independent ordering for Portuguese/English UTF-8 channel names.
// Fold case and Latin-1 accents; leave the displayed title and URL unchanged.
static unsigned favorite_sort_char(const unsigned char**cursor){
    const unsigned char*p=*cursor;
    while(*p){
        unsigned c=*p++;
        if(c>=0xc2&&c<=0xdf&&(*p&0xc0)==0x80){c=((c&31)<<6)|(*p++&63);}
        else if(c>=0xe0&&c<=0xef&&p[0]&&p[1]&&(p[0]&0xc0)==0x80&&(p[1]&0xc0)==0x80){c=((c&15)<<12)|((p[0]&63)<<6)|(p[1]&63);p+=2;}
        else if(c>=0xf0&&c<=0xf4&&p[0]&&p[1]&&p[2]&&(p[0]&0xc0)==0x80&&(p[1]&0xc0)==0x80&&(p[2]&0xc0)==0x80){c=((c&7)<<18)|((p[0]&63)<<12)|((p[1]&63)<<6)|(p[2]&63);p+=3;}
        if(c>=0x300&&c<=0x36f)continue; // Decomposed accents, e.g. A + acute.
        if(c>='A'&&c<='Z')c+=32;
        if(c>=0xc0&&c<=0xde&&c!=0xd7)c+=32;
        if(c>=0xe0&&c<=0xe5)c='a';else if(c==0xe7)c='c';
        else if(c>=0xe8&&c<=0xeb)c='e';else if(c>=0xec&&c<=0xef)c='i';
        else if(c==0xf1)c='n';else if((c>=0xf2&&c<=0xf6)||c==0xf8)c='o';
        else if(c>=0xf9&&c<=0xfc)c='u';else if(c==0xfd||c==0xff)c='y';
        *cursor=p;return c;
    }
    *cursor=p;return 0;
}
static int compare_favorite(const void*a,const void*b){
    const Item*x=a,*y=b;const unsigned char*p=(const unsigned char*)x->title,*q=(const unsigned char*)y->title;
    for(;;){unsigned c=favorite_sort_char(&p),d=favorite_sort_char(&q);if(c!=d)return c<d?-1:1;if(!c)break;}
    int tie=strcmp(x->title,y->title);return tie?tie:strcmp(x->uri,y->uri);
}
static void favorites_sort(void){qsort(favorites,fav_count,sizeof(Item),compare_favorite);}
static void favorites_load(void){char p[4096];path_join(p,sizeof(p),app,"config/favoritos.txt");FILE*f=fopen(p,"r");if(!f)return;char line[4608];while(fav_count<MAX_FAV&&fgets(line,sizeof(line),f)){char*sep=strchr(line,'\t');if(!sep)continue;*sep++=0;sep[strcspn(sep,"\r\n")]=0;if(!is_url(sep))continue;snprintf(favorites[fav_count].title,256,"%.255s",line);snprintf(favorites[fav_count++].uri,4096,"%s",sep);}fclose(f);favorites_sort();}
static int favorite_pending=-1;
static int favorites_write(int skip){
    char p[4096],t[4096];path_join(p,sizeof(p),app,"config/favoritos.txt");path_join(t,sizeof(t),app,"config/favoritos.new");FILE*f=fopen(t,"w");if(!f)return 0;
    int ok=1;for(int i=0;i<fav_count;i++)if(i!=skip&&fprintf(f,"%s\t%s\n",favorites[i].title,favorites[i].uri)<0)ok=0;
    if(fclose(f)!=0)ok=0;
    if(ok&&rename(t,p)==0)return 1;
    unlink(t);return 0;
}
static void favorites_open(int index){page=3;count=fav_count;memcpy(items,favorites,count*sizeof(Item));selected=index;if(selected>=count)selected=count?count-1:0;if(selected<0)selected=0;favorite_pending=-1;}
static void favorite_confirm(int index){if(playing||page!=3||index<0||index>=fav_count)return;favorite_pending=index;page=6;selected=0;}
static void favorite_cancel(void){favorites_open(favorite_pending);}
static void favorite_remove(void){
    int i=favorite_pending;if(playing||page!=6||i<0||i>=fav_count)return;
    if(!favorites_write(i)){favorite_cancel();notify(tr("Não foi possível remover. Favorito mantido."));return;}
    memmove(&favorites[i],&favorites[i+1],(fav_count-i-1)*sizeof(Item));fav_count--;favorites_open(i);notify(tr("Favorito removido."));
}
static void favorite_add(void){if(!is_url(uri)){notify(tr("Os favoritos guardam endereços de rede."));return;}for(int i=0;i<fav_count;i++)if(!strcmp(uri,favorites[i].uri)){notify(tr("Este endereço já está nos favoritos."));return;}if(fav_count==MAX_FAV){notify(tr("Limite de favoritos atingido."));return;}snprintf(favorites[fav_count].title,256,"%.255s",title);for(char*p=favorites[fav_count].title;*p;p++)if(*p=='\t'||*p=='\n'||*p=='\r')*p=' ';snprintf(favorites[fav_count++].uri,4096,"%s",uri);if(favorites_write(-1)){favorites_sort();notify(tr("Guardado nos favoritos."));}else{fav_count--;notify(tr("Não foi possível guardar."));}}

static int compare_item(const void*a,const void*b){const Item*x=a,*y=b;if(x->folder!=y->folder)return y->folder-x->folder;return strcasecmp(x->title,y->title);}
static void browse(const char *dir){snprintf(directory,sizeof(directory),"%s",dir);count=selected=0;page=1;DIR*d=opendir(dir);if(!d){notify(tr("Não foi possível abrir esta pasta."));return;}struct dirent*de;while(count<MAX_ITEMS&&(de=readdir(d))){if(de->d_name[0]=='.')continue;char p[4096];path_join(p,sizeof(p),dir,de->d_name);if(!p[0])continue;struct stat st;if(stat(p,&st)<0)continue;if(!S_ISDIR(st.st_mode)&&(!S_ISREG(st.st_mode)||!is_video(de->d_name)))continue;Item*i=&items[count++];snprintf(i->uri,sizeof(i->uri),"%s",p);snprintf(i->title,sizeof(i->title),"%.255s",de->d_name);i->folder=S_ISDIR(st.st_mode);}closedir(d);qsort(items,count,sizeof(Item),compare_item);if(count==MAX_ITEMS)notify(tr("Mostrados os primeiros 2048 elementos."));}
static void relative_uri(char *out,size_t cap,const char*origin,const char*s){
    if(s[0]=='/'&&s[1]=='/'&&is_url(origin)){const char*colon=strchr(origin,':');snprintf(out,cap,"%.*s:%s",(int)(colon-origin),origin,s);return;}
    if(is_url(s)||s[0]=='/'){if(is_url(origin)&&s[0]=='/'&&!is_url(s)){const char*p=strstr(origin,"://");p=p?strchr(p+3,'/'):NULL;size_t n=p?(size_t)(p-origin):strlen(origin);snprintf(out,cap,"%.*s%s",(int)n,origin,s);}else snprintf(out,cap,"%s",s);return;}
    char base[4096];snprintf(base,sizeof(base),"%s",origin);char*p=strrchr(base,'/');if(p)*p=0;else snprintf(base,sizeof(base),".");path_join(out,cap,base,s);
}
static int parse_list(char *data,const char *origin){
    count=selected=0;char channel[256]="",options[3072]="";char *save=NULL;
    for(char*line=strtok_r(data,"\n",&save);line&&count<MAX_ITEMS;line=strtok_r(NULL,"\n",&save)){
        if((unsigned char)line[0]==0xef&&(unsigned char)line[1]==0xbb&&(unsigned char)line[2]==0xbf)line+=3;
        while(*line==' '||*line=='\t')line++;
        line[strcspn(line,"\r")]=0;
        if(!strncmp(line,"#EXT-X-",7))return -1; // HLS segments belong to the player, not the channel browser.
        if(!strncmp(line,"#EXTINF:",8)){char*p=strchr(line,',');snprintf(channel,sizeof(channel),"%.255s",p?p+1:"Canal");continue;}
        if(!strncmp(line,"#EXTVLCOPT:",11)){
            char*key=line+11,*value=strchr(key,'=');if(!value)continue;*value++=0;const char*header=NULL;
            if(!strcasecmp(key,"http-user-agent"))header="User-Agent";
            else if(!strcasecmp(key,"http-referrer")||!strcasecmp(key,"http-referer"))header="Referer";
            else if(!strcasecmp(key,"http-origin"))header="Origin";
            else if(!strcasecmp(key,"http-cookie"))header="Cookie";
            if(header&&!append_http_option(options,sizeof(options),header,value))notify(tr("Um cabeçalho da lista é demasiado longo."));
            continue;
        }
        if(!strncmp(line,"#KODIPROP:inputstream.adaptive.stream_headers=",46)){
            const char*extra=line+46;size_t n=strlen(options),len=strlen(extra);if(n+len+2<sizeof(options)){if(n)strcat(options,"&");strcat(options,extra);}continue;
        }
        if(!*line||*line=='#')continue;
        Item*i=&items[count];relative_uri(i->uri,sizeof(i->uri),origin,line);
        if(!merge_http_options(i->uri,sizeof(i->uri),options)){options[0]=channel[0]=0;continue;}
        options[0]=0;
        if(!i->uri[0])continue;
        if(strstr(i->uri,"://")&&!is_url(i->uri))continue;
        if(!channel[0])snprintf(channel,sizeof(channel),"Canal %d",count+1);
        snprintf(i->title,sizeof(i->title),"%s",channel);i->folder=0;count++;channel[0]=0;
    }
    page=2;snprintf(playlist_origin,sizeof(playlist_origin),"%s",origin);if(!count)notify(tr("A lista não contém canais reconhecidos."));return count;
}
static char* read_file(const char*p){FILE*f=fopen(p,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long size=ftell(f);if(size<0||size>4*1024*1024){fclose(f);return NULL;}rewind(f);char*b=calloc((size_t)size+1,1);if(b&&fread(b,1,size,f)!=(size_t)size){free(b);b=NULL;}fclose(f);return b;}
static size_t fetch_write(char*p,size_t a,size_t b,void*unused){(void)unused;size_t n=a*b;if(n>4*1024*1024-fetch.length)return 0;char*d=realloc(fetch.data,fetch.length+n+1);if(!d)return 0;fetch.data=d;memcpy(d+fetch.length,p,n);fetch.length+=n;d[fetch.length]=0;return n;}
static int fetch_progress(void*a,curl_off_t b,curl_off_t c,curl_off_t d,curl_off_t e){(void)a;(void)b;(void)c;(void)d;(void)e;return atomic_load(&fetch.cancel);}
static void *fetch_worker(void*unused){(void)unused;CURL*c=c_init();if(!c){fetch.result=1;atomic_store(&fetch.done,1);return NULL;}
    HttpRequest request;if(!http_request(fetch.address,&request)){fetch.result=CURLE_URL_MALFORMAT;c_cleanup(c);atomic_store(&fetch.done,1);return NULL;}
    snprintf(fetch.origin,sizeof(fetch.origin),"%s",request.url);
    struct curl_slist*headers=NULL;const char*names[]={"Referer","Origin","Cookie","Authorization"};const char*values[]={request.referer,request.origin,request.cookie,request.authorization};
    for(int i=0;i<4;i++)if(values[i][0]){char line[2304];snprintf(line,sizeof(line),"%s: %s",names[i],values[i]);struct curl_slist*next=c_append(headers,line);if(!next){c_list_free(headers);c_cleanup(c);fetch.result=CURLE_OUT_OF_MEMORY;atomic_store(&fetch.done,1);return NULL;}headers=next;}
    c_setopt(c,CURLOPT_URL,request.url);c_setopt(c,CURLOPT_WRITEFUNCTION,fetch_write);c_setopt(c,CURLOPT_HTTPHEADER,headers);
    c_setopt(c,CURLOPT_TIMEOUT,18L);c_setopt(c,CURLOPT_CONNECTTIMEOUT,8L);c_setopt(c,CURLOPT_FOLLOWLOCATION,1L);c_setopt(c,CURLOPT_MAXREDIRS,5L);
    c_setopt(c,CURLOPT_NOSIGNAL,1L);c_setopt(c,CURLOPT_FAILONERROR,1L);c_setopt(c,CURLOPT_USERAGENT,request.ua);
    c_setopt(c,CURLOPT_PROTOCOLS,(long)(CURLPROTO_HTTP|CURLPROTO_HTTPS));c_setopt(c,CURLOPT_REDIR_PROTOCOLS,(long)(CURLPROTO_HTTP|CURLPROTO_HTTPS));
    c_setopt(c,CURLOPT_NOPROGRESS,0L);c_setopt(c,CURLOPT_XFERINFOFUNCTION,fetch_progress);
    fetch.result=c_perform(c);c_getinfo(c,CURLINFO_RESPONSE_CODE,&fetch.http_status);char*effective=NULL;
    if(c_getinfo(c,CURLINFO_EFFECTIVE_URL,&effective)==CURLE_OK&&effective&&strlen(effective)<sizeof(fetch.origin))snprintf(fetch.origin,sizeof(fetch.origin),"%s",effective);
    fprintf(stderr,"Playlist HTTP: status=%ld curl=%d\n",fetch.http_status,fetch.result);
    c_list_free(headers);c_cleanup(c);atomic_store(&fetch.done,1);return NULL;
}
static void fetch_start(const char*address){if(strncasecmp(address,"http://",7)&&strncasecmp(address,"https://",8)){notify(tr("Listas remotas: utiliza HTTP ou HTTPS."));return;}void*lib=dlopen("libcurl.so.4",RTLD_NOW|RTLD_LOCAL);if(!lib){notify(tr("A biblioteca de rede não está disponível."));return;}
#define CURL_LOAD(var,name) *(void**)(&var)=dlsym(lib,name);if(!var){notify(tr("Rede indisponível."));return;}
    CURL_LOAD(c_init,"curl_easy_init");CURL_LOAD(c_setopt,"curl_easy_setopt");CURL_LOAD(c_perform,"curl_easy_perform");CURL_LOAD(c_cleanup,"curl_easy_cleanup");CURL_LOAD(c_global_init,"curl_global_init");CURL_LOAD(c_append,"curl_slist_append");CURL_LOAD(c_list_free,"curl_slist_free_all");CURL_LOAD(c_getinfo,"curl_easy_getinfo");
    if(c_global_init(CURL_GLOBAL_DEFAULT)){notify(tr("Não foi possível iniciar a rede."));return;}
    memset(&fetch,0,sizeof(fetch));snprintf(fetch.address,sizeof(fetch.address),"%s",address);
    if(pthread_create(&fetch.thread,NULL,fetch_worker,NULL)){notify(tr("Não foi possível carregar a lista."));return;}fetch_active=1;notify(tr("A carregar lista… B cancela."));
}
static void command(const char*a,const char*b,const char*c){const char*cmd[]={a,b,c,NULL};if(mpv){int rc=m_command_async(mpv,0,cmd);if(rc<0)notify(tr("O leitor não aceitou o comando."));}}
static void set_double(const char*p,double n){if(mpv)m_set_property(mpv,p,MPV_FORMAT_DOUBLE,&n);}
static void set_flag(const char*p,int n){if(mpv)m_set_property(mpv,p,MPV_FORMAT_FLAG,&n);}
static void apply_image_mode(void){
    if(!mpv)return;
    double aspect=image_mode==1?4.0/3.0:-1.0,panscan=image_mode==2?1.0:0.0;
    int a=m_set_property(mpv,"video-aspect-override",MPV_FORMAT_DOUBLE,&aspect);
    int b=m_set_property(mpv,"panscan",MPV_FORMAT_DOUBLE,&panscan);
    fprintf(stderr,"Image mode: %d aspect=%d panscan=%d\n",image_mode,a,b);
}
static int apply_http_request(const HttpRequest*r){
    char*ua=(char*)r->ua;
    int rc=m_set_property(mpv,"options/user-agent",MPV_FORMAT_STRING,&ua);
    mpv_node values[4];char lines[4][2304];int count_headers=0;
    const char*names[]={"Referer","Origin","Cookie","Authorization"};const char*content[]={r->referer,r->origin,r->cookie,r->authorization};
    for(int i=0;i<4;i++)if(content[i][0]){snprintf(lines[count_headers],sizeof(lines[count_headers]),"%s: %s",names[i],content[i]);values[count_headers].format=MPV_FORMAT_STRING;values[count_headers].u.string=lines[count_headers];count_headers++;}
    mpv_node_list list={.num=count_headers,.values=values};mpv_node node={.format=MPV_FORMAT_NODE_ARRAY,.u.list=&list};
    int headers=m_set_property(mpv,"options/http-header-fields",MPV_FORMAT_NODE,&node);
    fprintf(stderr,"HTTP compatibility: ua=%d headers=%d fields=%d\n",rc,headers,count_headers);
    return rc>=0&&headers>=0;
}
static int log_http_status(const char*text,int code){
    const char*patterns[]={"HTTP error %d","HTTP %d","HTTP/1.0 %d","HTTP/1.1 %d","HTTP/2 %d","Server returned %d",NULL};
    for(int i=0;patterns[i];i++){char pattern[64];snprintf(pattern,sizeof(pattern),patterns[i],code);if(strcasestr(text,pattern))return 1;}return 0;
}
static void classify_mpv_warning(const char*text){
    const char*reason=NULL;
    if(log_http_status(text,401))reason=tr("HTTP 401: autenticação recusada.");
    else if(log_http_status(text,403))reason=tr("HTTP 403: o servidor recusou a ligação.");
    else if(log_http_status(text,404))reason=tr("HTTP 404: o endereço não foi encontrado.");
    else if(log_http_status(text,429))reason=tr("HTTP 429: limite de pedidos do servidor.");
    else if(strcasestr(text,"certificate")||strcasestr(text,"TLS handshake"))reason=tr("Falha de certificado ou ligação TLS.");
    else if(strcasestr(text,"protocol not found"))reason=tr("Protocolo não suportado pelo motor.");
    else if(strcasestr(text,"Invalid data found")||strcasestr(text,"Failed to recognize"))reason=tr("Formato de stream não reconhecido.");
    else if(strcasestr(text,"timed out"))reason=tr("O servidor não respondeu a tempo.");
    else if(strcasestr(text,"decoder")&&strcasestr(text,"not found"))reason=tr("O motor não encontrou o descodificador.");
    if(reason){snprintf(playback_error,sizeof(playback_error),"%s",reason);if(warnings_logged++<16)fprintf(stderr,"Playback warning: %s\n",reason);}
}
static void render_update(void*unused){(void)unused;atomic_store(&render_pending,1);}
static void*get_gl(void*unused,const char*name){(void)unused;return pSDL_GL_GetProcAddress(name);}
static int video_setup(void){
    if(render_context)return 1;
    int display=screenmode?1:0;if(pSDL_GetNumVideoDisplays()<2)display=0;
    pSDL_GL_SetAttribute(21,4);pSDL_GL_SetAttribute(17,2);pSDL_GL_SetAttribute(18,0);pSDL_GL_SetAttribute(5,1);
    video=pSDL_CreateWindow("BaetaMP · Vídeo",0x2fff0000|display,0x2fff0000|display,1024,768,2|8|16);
    if(video)glcontext=pSDL_GL_CreateContext(video);
    if(glcontext){pSDL_GL_MakeCurrent(video,glcontext);pSDL_GL_SetSwapInterval(1);
        *(void**)(&gl_clear_color)=get_gl(NULL,"glClearColor");
        *(void**)(&gl_clear)=get_gl(NULL,"glClear");
        *(void**)(&gl_disable)=get_gl(NULL,"glDisable");
        *(void**)(&gl_bind_framebuffer)=get_gl(NULL,"glBindFramebuffer");
        *(void**)(&gl_viewport)=get_gl(NULL,"glViewport");
        *(void**)(&gl_color_mask)=get_gl(NULL,"glColorMask");
        mpv_opengl_init_params init={get_gl,NULL};mpv_render_param params[]={{MPV_RENDER_PARAM_API_TYPE,(void*)MPV_RENDER_API_TYPE_OPENGL},{MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,&init},{MPV_RENDER_PARAM_INVALID,NULL}};
        int rc=m_render_context_create(&render_context,mpv,params);fprintf(stderr,"Render OpenGL ES: %d\n",rc);
        if(rc<0){pSDL_GL_DeleteContext(glcontext);glcontext=NULL;pSDL_DestroyWindow(video);video=NULL;}
    }else if(video){pSDL_DestroyWindow(video);video=NULL;}
    if(!render_context){software_video=1;video=pSDL_CreateWindow("BaetaMP · Vídeo",0x2fff0000|display,0x2fff0000|display,1024,768,8|16);if(!video)return 0;
        vrenderer=pSDL_CreateRenderer(video,-1,1);if(!vrenderer)return 0;pSDL_RenderSetLogicalSize(vrenderer,1024,768);
        vtexture=pSDL_CreateTexture(vrenderer,0x16161804,1,1024,768); // SDL_PIXELFORMAT_RGB888 = bgr0 on little endian.
        if(!vtexture||posix_memalign((void**)&pixels,64,1024*768*4))return 0;
        mpv_render_param params[]={{MPV_RENDER_PARAM_API_TYPE,(void*)MPV_RENDER_API_TYPE_SW},{MPV_RENDER_PARAM_INVALID,NULL}};
        int rc=m_render_context_create(&render_context,mpv,params);fprintf(stderr,"Render software: %d\n",rc);if(rc<0)return 0;
    }
    pSDL_SetWindowFullscreen(video,FULLSCREEN_DESKTOP);
    m_render_context_set_update_callback(render_context,render_update,NULL);
    fprintf(stderr,"Ecra video: %d; screensaver SDL disponivel\n",pSDL_GetWindowDisplayIndex(video));return 1;
}
static void video_destroy(void){
    fprintf(stderr,"Video cleanup: begin\n");
    if(render_context){if(glcontext)pSDL_GL_MakeCurrent(video,glcontext);m_render_context_set_update_callback(render_context,NULL,NULL);m_render_context_free(render_context);render_context=NULL;fprintf(stderr,"Video cleanup: render context freed\n");}
    atomic_store(&render_pending,0);
    if(vtexture){pSDL_DestroyTexture(vtexture);vtexture=NULL;}if(vrenderer){pSDL_DestroyRenderer(vrenderer);vrenderer=NULL;}free(pixels);pixels=NULL;
    if(glcontext){pSDL_GL_MakeCurrent(video,NULL);pSDL_GL_DeleteContext(glcontext);glcontext=NULL;}
    if(video){pSDL_DestroyWindow(video);video=NULL;}
    software_video=0;fprintf(stderr,"Video cleanup: done\n");
}
static int mpv_load(void){if(mpv)return 1;void*lib=dlopen("libmpv.so.2",RTLD_NOW|RTLD_LOCAL);if(!lib){notify(tr("libmpv não está disponível."));fprintf(stderr,"libmpv load: %s\n",dlerror());return 0;}
#define ML(retname) *(void**)(&m_##retname)=dlsym(lib,"mpv_"#retname);if(!m_##retname)return 0
    ML(create);ML(initialize);ML(set_option_string);ML(set_property);ML(get_property);ML(command);ML(command_async);ML(wait_event);ML(terminate_destroy);ML(free);ML(render_context_create);ML(render_context_render);ML(render_context_free);ML(render_context_set_update_callback);ML(render_context_update);ML(render_context_report_swap);ML(request_log_messages);ML(free_node_contents);
    *(void**)(&md5_sum)=dlsym(lib,"av_md5_sum");
    mpv=m_create();if(!mpv)return 0;
    const char*opts[][2]={{"config","no"},{"vo","libmpv"},{"hwdec","auto-safe"},{"ao","alsa"},{"audio-device","alsa/default"},{"terminal","no"},{"msg-level","all=warn,ffmpeg=debug,stream=debug"},{"input-default-bindings","no"},{"input-vo-keyboard","no"},{"sub-auto","fuzzy"},{"sub-codepage","auto"},{"sub-font","DejaVu Sans"},{"sub-border-size","2"},{"sub-shadow-offset","1"},{"resume-playback","no"},{"demuxer-max-bytes","48MiB"},{"demuxer-max-back-bytes","8MiB"},{"cache-secs","12"},{"network-timeout","20"},{"keep-open","no"},{NULL,NULL}};
    for(int i=0;opts[i][0];i++){int rc=m_set_option_string(mpv,opts[i][0],opts[i][1]);if(rc<0)fprintf(stderr,"Option %s: %d\n",opts[i][0],rc);}
    if(smoke){m_set_option_string(mpv,"ao","null");m_set_option_string(mpv,"resume-playback","no");}
    char p[4096];path_join(p,sizeof(p),app,"config/retomar");m_set_option_string(mpv,"watch-later-directory",p);path_join(p,sizeof(p),app,"fonts");m_set_option_string(mpv,"sub-fonts-dir",p);
    int rc=m_initialize(mpv);fprintf(stderr,"mpv_initialize: %d\n",rc);if(rc<0){m_terminate_destroy(mpv);mpv=NULL;notify(tr("Falha ao iniciar mpv."));return 0;}
    int log_result=m_request_log_messages(mpv,"debug");fprintf(stderr,"Client log request: %d\n",log_result);apply_image_mode();
    set_double("volume",volume);set_double("sub-font-size",font_size);return 1;
}
// mpv 0.35 names watch-later files with the uppercase MD5 of the absolute path.
static int resume_path(const char*address,char*out,size_t size){
    if(!md5_sum||is_url(address)||address[0]!='/')return 0;
    uint8_t digest[16];char name[64]="config/retomar/";md5_sum(digest,(const uint8_t*)address,strlen(address));
    for(int i=0;i<16;i++)snprintf(name+15+i*2,3,"%02X",digest[i]);
    path_join(out,size,app,name);return 1;
}
static double resume_read(const char*path){
    FILE*f=fopen(path,"r");if(!f)return 0;char line[512];double position=0;
    while(fgets(line,sizeof(line),f)){double value;char trailing;if(sscanf(line,"start=%lf %c",&value,&trailing)==1&&isfinite(value)&&value>0&&value<315360000){position=value;break;}}
    fclose(f);return position;
}
static const char*legacy_app(int index){return getenv(index==0?"RGDS_LEGACY_APP":"RGDS_LEGACY_APP_2");}
static double resume_position(const char*address){
    char path[4096];if(!resume_path(address,path,sizeof(path)))return 0;
    if(access(path,F_OK)==0)return resume_read(path);
    size_t prefix=strlen(app);
    for(int i=0;i<2;i++){
        const char*old=legacy_app(i);
        if(old&&*old&&!strncmp(address,app,prefix)&&address[prefix]=='/'){
            char previous[4096];int n=snprintf(previous,sizeof(previous),"%s%s",old,address+prefix);
            if(n>0&&(size_t)n<sizeof(previous)&&resume_path(previous,path,sizeof(path))&&access(path,F_OK)==0)return resume_read(path);
        }
    }
    return 0;
}
static void resume_clear(const char*address){
    char path[4096];if(resume_path(address,path,sizeof(path)))unlink(path);
    size_t prefix=strlen(app);
    for(int i=0;i<2;i++){
        const char*old=legacy_app(i);
        if(old&&*old&&!strncmp(address,app,prefix)&&address[prefix]=='/'){
            char previous[4096];int n=snprintf(previous,sizeof(previous),"%s%s",old,address+prefix);
            if(n>0&&(size_t)n<sizeof(previous)&&resume_path(previous,path,sizeof(path)))unlink(path);
        }
    }
}
static Item resume_item;
static double resume_saved;
static int resume_page,resume_selected;
static void resume_open(const char*address,const char*name,double position){
    snprintf(resume_item.uri,sizeof(resume_item.uri),"%s",address);snprintf(resume_item.title,sizeof(resume_item.title),"%.255s",name);
    resume_saved=position;resume_page=page;resume_selected=selected;page=8;selected=0;
}
static void resume_cancel(void){page=resume_page;selected=resume_selected;}
static void resume_take(int choice,Item*item,double*position){*item=resume_item;*position=choice==0?resume_saved:0;resume_cancel();}
static void save_position(void){if(mpv&&playing&&!smoke&&!is_url(uri)){const char*c[]={"write-watch-later-config",NULL};m_command(mpv,c);}}
typedef struct {char magic[16];int page,selected,count;char directory[4096],error[192];} MenuSession;
#define MAX_TRACKS 256
typedef struct {int64_t id;int active;char name[384],detail[256];} TrackChoice;
static TrackChoice tracks[MAX_TRACKS+1];
static int track_count,track_audio,track_return_page,track_return_selected;
static void tracks_close(void);
static int nav_count(void);
static void save_menu_session(void){
    char path[4096],temp[4096];path_join(path,sizeof(path),app,"config/menu-session.bin");path_join(temp,sizeof(temp),app,"config/menu-session.new");
    MenuSession session={0};snprintf(session.magic,sizeof(session.magic),"RGDS_MENU_V2");snprintf(session.error,sizeof(session.error),"%s",playback_error);
    int source_page=page==9?track_return_page:page;
    int source_selected=page==9?track_return_selected:selected;
    int current_page=source_page==5?option_page:source_page;
    session.page=(current_page>=1&&current_page<=3)?current_page:0;session.selected=source_page==5?option_selected:source_selected;
    session.count=session.page==2?count:0;snprintf(session.directory,sizeof(session.directory),"%s",directory);
    FILE*f=fopen(temp,"wb");if(!f)return;int ok=fwrite(&session,sizeof(session),1,f)==1;
    if(ok&&session.count)ok=fwrite(items,sizeof(Item),session.count,f)==(size_t)session.count;
    if(fclose(f)!=0)ok=0;
    if(ok)rename(temp,path);
}
static void migrated_path(char*address,size_t size){
    for(int i=0;i<2;i++){
        const char*old=legacy_app(i);if(!old||!*old)continue;size_t n=strlen(old);
        if(!strncmp(address,old,n)&&(address[n]=='/'||address[n]==0)){
            char updated[4096];int len=snprintf(updated,sizeof(updated),"%s%s",app,address+n);
            if(len>=0&&(size_t)len<size)snprintf(address,size,"%s",updated);
            return;
        }
    }
}
static void restore_menu_session(void){
    char path[4096];path_join(path,sizeof(path),app,"config/menu-session.bin");FILE*f=fopen(path,"rb");if(!f)return;
    MenuSession session;int ok=fread(&session,sizeof(session),1,f)==1;
    ok=ok&&!memcmp(session.magic,"RGDS_MENU_V2",13)&&session.page>=0&&session.page<=3&&session.count>=0&&session.count<=MAX_ITEMS&&memchr(session.directory,0,sizeof(session.directory))&&memchr(session.error,0,sizeof(session.error));
    if(ok&&session.page==2){ok=fread(items,sizeof(Item),session.count,f)==(size_t)session.count;for(int i=0;ok&&i<session.count;i++)ok=memchr(items[i].uri,0,sizeof(items[i].uri))&&memchr(items[i].title,0,sizeof(items[i].title));}
    fclose(f);unlink(path);if(!ok)return;
    migrated_path(session.directory,sizeof(session.directory));for(int i=0;i<session.count;i++)migrated_path(items[i].uri,sizeof(items[i].uri));
    if(session.page==1)browse(session.directory);
    else if(session.page==2){page=2;count=session.count;}
    else if(session.page==3){page=3;count=fav_count;memcpy(items,favorites,count*sizeof(Item));}
    else page=0;
    selected=session.selected;if(selected<0||selected>=nav_count())selected=0;
    fprintf(stderr,"Menu session restored: page=%d selected=%d\n",page,selected);
    if(session.error[0])notify(session.error);
}
static void finish_playback(void){
    if(page==9)tracks_close();
    playing=paused=stop_pending=failure_pending=0;stats_at=0;
    touch_inhibited=tdown;
    awake_stop();restore_light();pSDL_EnableScreenSaver();
    const char*restart=getenv("RGDS_RESTART_AFTER_VIDEO");
    if(!shutting_down&&restart&&!strcmp(restart,"1")){
        settings_save();save_menu_session();
        fprintf(stderr,"UI restart: exit 42; fresh process requested after video.\n");
        // Kernel closes the Wayland/GPU client. Avoid reusing or drawing into
        // the stock Mali session after video teardown; the launcher owns retry.
        _exit(42);
    }
    // Retire the libmpv surface before drawing the software menu again.
    video_destroy();
    pSDL_ShowWindow(menu);pSDL_RaiseWindow(menu);settings_save();
    fprintf(stderr,"Reproducao terminada; menu restaurado; screensaver reposto.\n");
}
static void stop_playback(void){
    if(!playing||stop_pending)return;
    playback_error[0]=0;
    save_position();stop_pending=1;stop_deadline=pSDL_GetTicks()+5000;
    const char*c[]={"stop",NULL};int rc=m_command_async(mpv,0,c);
    fprintf(stderr,"Stop solicitado (async): %d\n",rc);
    if(rc<0)finish_playback();
}
static void play_now(const char*address,const char*name,double position){HttpRequest request;if(!http_request(address,&request)){notify(tr("URL ou cabeçalhos inválidos."));return;}if(!mpv_load())return;if(!video_setup()){notify(tr("Não foi possível abrir a janela de vídeo."));video_destroy();return;}
    if(stop_pending){notify(tr("A terminar o vídeo…"));return;}
    save_position();snprintf(uri,sizeof(uri),"%s",address);snprintf(title,sizeof(title),"%.255s",name);playing=1;paused=0;file_loaded=0;controls_selected=0;warnings_logged=0;playback_error[0]=0;set_flag("pause",0);set_double("sub-delay",0);
    apply_image_mode();apply_http_request(&request);
    pSDL_ShowWindow(video);pSDL_RaiseWindow(video);pSDL_DisableScreenSaver();
    awake_start();
    fprintf(stderr,"Abrir %s; screensaver desativado: %s\n",is_url(address)?"rede":"ficheiro local",pSDL_IsScreenSaverEnabled()?"nao":"sim");
    char start[64];snprintf(start,sizeof(start),"start=%.6f",position);const char*c[]={"loadfile",request.url,"replace",is_url(address)?NULL:start,NULL};int rc=m_command_async(mpv,0,c);if(rc<0){stop_playback();notify(tr("Não foi possível abrir o conteúdo."));}
    notify(tr("A abrir… B regressa à lista."));
}
static void play(const char*address,const char*name){
    if(!playing&&!smoke&&!is_url(address)){if(!mpv_load())return;double position=resume_position(address);if(position>0){resume_open(address,name,position);return;}}
    play_now(address,name,0);
}
static void step_channel(int delta){if(page!=2&&page!=3){notify(tr("L/R mudam de canal numa lista IPTV."));return;}if(count){selected=(selected+delta+count)%count;play(items[selected].uri,items[selected].title);}}
static void toggle_pause(void){if(!playing)return;paused=!paused;set_flag("pause",paused);}
static void adjust_volume(int delta){volume+=delta;if(volume<0)volume=0;if(volume>100)volume=100;set_double("volume",volume);write_num(volume_node,(volume+5)/10);char message[64];snprintf(message,sizeof(message),"Volume: %d%%",volume);notify(message);fprintf(stderr,"Volume: %d%%\n",volume);}
static void volume_pump(void){
    if(keysfd<0)return;
    struct input_event e;
    for(int n=0;n<128&&read(keysfd,&e,sizeof(e))==sizeof(e);n++){
        if(e.type!=EV_KEY||(e.code!=KEY_VOLUMEUP&&e.code!=KEY_VOLUMEDOWN)||(e.value!=1&&e.value!=2))continue;
        uint32_t now=pSDL_GetTicks();if(e.value==2&&now-volume_repeat<180)continue;volume_repeat=now;
        adjust_volume(e.code==KEY_VOLUMEUP?5:-5);
    }
}
static void keyboard_open(int mode){keyboard_mode=mode;page=4;selected=keysel=0;shiftkey=0;url[0]=0;}
static const char*keyrows[]={"qwertyuiop","asdfghjkl-","zxcvbnm._/","0123456789",":?=&%+@#[]"};
static void keyboard_accept(void){if(!is_url(url)){notify(tr("Introduz um URL http(s), rtsp, rtmp, udp ou rtp."));return;}page=0;selected=0;if(keyboard_mode==2)fetch_start(url);else play(url,tr("URL direto"));}
static void keyboard_key(int k){size_t n=strlen(url);if(k<50){if(n<sizeof(url)-2){char c=keyrows[k/10][k%10];if(shiftkey&&c>='a'&&c<='z')c-=32;url[n]=c;url[n+1]=0;}}else switch(k){case 50:shiftkey=!shiftkey;break;case 51:if(n)url[n-1]=0;break;case 52:snprintf(url,sizeof(url),"http://");break;case 53:snprintf(url,sizeof(url),"https://");break;case 54:url[0]=0;break;case 55:keyboard_accept();break;}}
static void options_open(void){option_page=page;option_selected=selected;page=5;selected=0;}
static void options_close(void){page=option_page;selected=option_selected;}

static int exit_page,exit_selected;
static void exit_open(void){if(playing||page==7)return;exit_page=page;exit_selected=selected;page=7;selected=0;}
static void exit_cancel(void){page=exit_page;selected=exit_selected;}
static int shoulder_seeks(void){int source=page==5?option_page:page;return !is_url(uri)||(source!=2&&source!=3);}
static void shoulder(int delta){if(shoulder_seeks())command("seek",delta<0?"-10":"10","relative");else if(page==5){int index=selected;page=option_page;selected=option_selected;step_channel(delta);option_selected=selected;page=5;selected=index;}else step_channel(delta);}

static const mpv_node*node_field(const mpv_node*node,const char*key){
    if(!node||node->format!=MPV_FORMAT_NODE_MAP||!node->u.list)return NULL;
    mpv_node_list*list=node->u.list;for(int i=0;i<list->num;i++)if(list->keys&&list->keys[i]&&!strcmp(list->keys[i],key))return &list->values[i];return NULL;
}
static const char*node_text(const mpv_node*node,const char*key){const mpv_node*v=node_field(node,key);return v&&v->format==MPV_FORMAT_STRING&&v->u.string?v->u.string:"";}
static int node_flag(const mpv_node*node,const char*key){const mpv_node*v=node_field(node,key);return v&&v->format==MPV_FORMAT_FLAG&&v->u.flag;}
static const char*track_language(const char*code){
    const char*names[][3]={{"pt","por","Português"},{"en","eng","Inglês"},{"es","spa","Espanhol"},{"fr","fra","Francês"},{"de","deu","Alemão"},{"it","ita","Italiano"},{"ja","jpn","Japonês"},{"ko","kor","Coreano"},{"zh","zho","Chinês"},{"ru","rus","Russo"},{"ar","ara","Árabe"},{"hi","hin","Hindi"},{"nl","nld","Neerlandês"},{"pl","pol","Polaco"},{"tr","tur","Turco"}};
    if(!strcasecmp(code,"pob"))return tr("Português (Brasil)");
    if(!strcasecmp(code,"fre"))code="fr";else if(!strcasecmp(code,"ger"))code="de";else if(!strcasecmp(code,"chi"))code="zh";else if(!strcasecmp(code,"dut"))code="nl";
    if(!*code||!strcasecmp(code,"und"))return tr("Idioma não indicado");
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);i++)if(!strcasecmp(code,names[i][1])||(!strncasecmp(code,names[i][0],2)&&(code[2]==0||code[2]=='-'||code[2]=='_')))return tr(names[i][2]);
    return code;
}
static void tracks_parse(const mpv_node*root){
    track_count=0;if(!track_audio){tracks[0]=(TrackChoice){.id=-1,.active=1};snprintf(tracks[0].name,sizeof(tracks[0].name),"%s",tr("Desligadas"));track_count=1;}
    if(!root||root->format!=MPV_FORMAT_NODE_ARRAY||!root->u.list)return;
    mpv_node_list*list=root->u.list;
    for(int i=0;i<list->num&&track_count<MAX_TRACKS+(track_audio?0:1);i++){
        const mpv_node*node=&list->values[i];if(strcmp(node_text(node,"type"),track_audio?"audio":"sub"))continue;
        const mpv_node*id=node_field(node,"id");if(!id||id->format!=MPV_FORMAT_INT64||id->u.int64<=0)continue;
        TrackChoice*t=&tracks[track_count++];*t=(TrackChoice){.id=id->u.int64,.active=node_flag(node,"selected")};
        const char*lang=node_text(node,"lang"),*name=node_text(node,"title"),*codec=node_text(node,"codec");
        snprintf(t->name,sizeof(t->name),"%s%s%.180s",track_language(lang),*name?" · ":"",name);
        snprintf(t->detail,sizeof(t->detail),"#%lld%s%.32s%s%.48s%s%s%s%s",(long long)t->id,*lang?" · ":"",lang,*codec?" · ":"",codec,node_flag(node,"external")?" · ":"",node_flag(node,"external")?tr("Externa"):"",node_flag(node,"forced")?" · ":"",node_flag(node,"forced")?tr("Forçada"):"");
        if(!track_audio&&t->active)tracks[0].active=0;
    }
}
static void tracks_close(void){
    page=track_return_page;selected=track_return_selected;
    if(playing&&screenmode){pSDL_HideWindow(menu);pSDL_ShowWindow(video);pSDL_RaiseWindow(video);}
}
static void tracks_open(int audio){
    if(!playing||!file_loaded||stop_pending){notify(tr("Abre um vídeo e aguarda que carregue."));return;}
    mpv_node root={0};if(m_get_property(mpv,"track-list",MPV_FORMAT_NODE,&root)<0){notify(tr("Não foi possível obter as faixas."));return;}
    if(page!=9){track_return_page=page;track_return_selected=selected;}
    track_audio=audio;tracks_parse(&root);m_free_node_contents(&root);page=9;selected=0;note_until=0;
    for(int i=0;i<track_count;i++)if(tracks[i].active){selected=i;break;}
    if(screenmode){pSDL_HideWindow(video);pSDL_ShowWindow(menu);pSDL_RaiseWindow(menu);}
}
static void tracks_select(int index){
    if(index==track_count){tracks_close();return;}
    if(index<0||index>=track_count||!playing||stop_pending)return;
    char id[32];if(tracks[index].id<0)snprintf(id,sizeof(id),"no");else snprintf(id,sizeof(id),"%lld",(long long)tracks[index].id);
    command("set",track_audio?"aid":"sid",id);tracks_close();
}

static void activate(int index){
    if(page==9){tracks_select(index);return;}
    if(page==8){if(index<0||index>1)return;Item item;double position;resume_take(index,&item,&position);if(index==1)resume_clear(item.uri);play_now(item.uri,item.title,position);return;}
    if(page==7){if(index==1)running=0;else if(index==0)exit_cancel();return;}
    if(page==6){if(index==1)favorite_remove();else if(index==0)favorite_cancel();return;}
    if(playing&&page!=5){switch(index){case 0:toggle_pause();break;case 1:options_open();break;case 2:stop_playback();break;case 3:favorite_add();break;}return;}
    if(page==0){char p[4096];switch(index){case 0:path_join(p,sizeof(p),app,"Filmes");browse(p);break;case 1:path_join(p,sizeof(p),app,"IPTV");browse(p);break;case 2:keyboard_open(1);break;case 3:keyboard_open(2);break;case 4:favorites_open(0);break;case 5:options_open();break;case 6:exit_open();break;}return;}
    if(page==1||page==2||page==3){if(index<0||index>=count)return;Item item=items[index];if(item.folder){browse(item.uri);return;}if(page==1&&is_list(item.uri)){char*b=read_file(item.uri);if(!b){notify(tr("Não foi possível ler a lista (máximo 4 MB)."));return;}int rc=parse_list(b,item.uri);free(b);if(rc<0){browse(directory);play(item.uri,item.title);}return;}play(item.uri,item.title);return;}
    if(page==4){keyboard_key(index);return;}
    if(page==5){switch(index){case 0:if(playing){notify(tr("Para mudar de ecrã, termina a reprodução."));break;}screenmode=!screenmode;video_destroy();break;case 1:image_mode=(image_mode+1)%3;apply_image_mode();break;case 2:font_size+=4;if(font_size>64)font_size=20;set_double("sub-font-size",font_size);break;case 3:if(playing)command("add","sub-delay","-0.1");else notify(tr("Abre um filme para sincronizar legendas."));break;case 4:if(playing)command("add","sub-delay","0.1");else notify(tr("Abre um filme para sincronizar legendas."));break;case 5:if(volume>=100)volume=0;else adjust_volume(5);set_double("volume",volume);break;case 6:tracks_open(0);break;case 7:tracks_open(1);break;case 8:language=!language;note_until=0;break;case 9:options_close();break;}settings_save();}
}
static void back(void){if(page==9){tracks_close();return;}if(page==8){resume_cancel();return;}if(page==7){exit_cancel();return;}if(page==6){favorite_cancel();return;}if(fetch_active){atomic_store(&fetch.cancel,1);notify(tr("A cancelar…"));return;}if(playing){stop_playback();return;}if(page==1){char p[4096];snprintf(p,sizeof(p),"%s",directory);char*s=strrchr(p,'/');if(s&&s!=p){*s=0;browse(p);}else{page=0;selected=0;}return;}if(page){page=0;selected=0;}else exit_open();}
static void label(const char*s,int x,int y,void*face,SDL_Color color,int width){SDL_Surface*surface=ttfRender(face,s,color);if(!surface)return;SDL_Texture*t=pSDL_CreateTextureFromSurface(renderer,surface);pSDL_FreeSurface(surface);if(!t)return;int w,h;pSDL_QueryTexture(t,NULL,NULL,&w,&h);SDL_Rect dst={x,y,w,h},clip={x,y,width,h};pSDL_RenderSetClipRect(renderer,&clip);pSDL_RenderCopy(renderer,t,NULL,&dst);pSDL_RenderSetClipRect(renderer,NULL);pSDL_DestroyTexture(t);}
static void fill(int x,int y,int w,int h,int active){
    if(w<=0||h<=0)return;
    pSDL_SetRenderDrawColor(renderer,active?22:27,active?115:40,active?139:57,255);
    /* Round every UI control; retain the thin playback progress bar. */
    int radius=h>10?h/4:0;if(radius>18)radius=18;if(radius>w/2)radius=w/2;
    SDL_Rect middle={x,y+radius,w,h-2*radius};pSDL_RenderFillRect(renderer,&middle);
    for(int row=0;row<radius;row++){
        double dy=radius-row-0.5;
        int inset=(int)ceil(radius-sqrt(radius*radius-dy*dy)-0.5);
        SDL_Rect top={x+inset,y+row,w-2*inset,1};
        SDL_Rect bottom={x+inset,y+h-1-row,w-2*inset,1};
        pSDL_RenderFillRect(renderer,&top);pSDL_RenderFillRect(renderer,&bottom);
    }
}
static const SDL_Color white={242,247,253,255},muted={164,187,207,255},accent={85,221,227,255};
static void draw(void){
    pSDL_SetRenderDrawColor(renderer,12,21,33,255);pSDL_RenderClear(renderer);
    label("BaetaMP",36,24,largefont,white,650);label("for RGDS+",715,42,smallfont,accent,280);
    if(fetch_active){label(tr("A carregar a lista IPTV…"),45,240,font,white,920);label(tr("B cancela · limite de 18 segundos"),45,310,smallfont,muted,920);goto footer;}
    if(page==9){
        label(track_audio?tr("Escolher áudio"):tr("Escolher legendas"),36,104,font,white,950);
        int start=selected/7*7;for(int i=start;i<=track_count&&i<start+7;i++){
            int y=150+(i-start)*70;fill(36,y,950,66,selected==i);
            if(i==track_count){label(tr("Voltar"),54,y+16,font,white,900);continue;}
            char name[420];snprintf(name,sizeof(name),"%s%s",tracks[i].active?"✓  ":"",tracks[i].name);label(name,54,y+2,font,white,910);label(tracks[i].detail,54,y+37,smallfont,muted,910);
        }
        if(track_audio&&!track_count)label(tr("Não há faixas de áudio disponíveis."),36,660,smallfont,muted,940);
        else {char text[64];snprintf(text,sizeof(text),"%d / %d",selected+1,track_count+1);label(text,840,660,smallfont,accent,160);}
    }else if(page==8){
        label(tr("Retomar filme"),36,130,largefont,white,950);
        label(resume_item.title,36,215,font,white,950);char text[128];int seconds=(int)resume_saved;snprintf(text,sizeof(text),tr("Paraste em %02d:%02d:%02d"),seconds/3600,seconds/60%60,seconds%60);label(text,36,285,smallfont,muted,950);
        const char*names[]={tr("Continuar"),tr("Ver do Início")};for(int i=0;i<2;i++){int x=36+i*488;fill(x,390,464,100,selected==i);label(names[i],x+25,422,font,white,410);}
    }else if(page==7){
        label(tr("Sair da aplicação?"),36,130,largefont,white,950);
        label(tr("Queres fechar o BaetaMP?"),36,250,font,white,950);
        const char*names[]={tr("Cancelar"),tr("Sair")};for(int i=0;i<2;i++){int x=36+i*488;fill(x,390,464,100,selected==i);label(names[i],x+25,422,font,white,410);}
    }else if(page==6){
        label(tr("Remover favorito?"),36,130,largefont,white,950);
        if(favorite_pending>=0&&favorite_pending<fav_count)label(favorites[favorite_pending].title,36,220,font,white,950);
        label(tr("Este favorito será retirado da lista."),36,285,smallfont,muted,950);
        const char*names[]={tr("Cancelar"),tr("Remover")};for(int i=0;i<2;i++){int x=36+i*488;fill(x,390,464,100,selected==i);label(names[i],x+25,422,font,white,410);}
    }else if(playing&&page!=5){
        label(title,36,110,font,white,940);double pos=0,duration=0,delay=0;m_get_property(mpv,"time-pos",MPV_FORMAT_DOUBLE,&pos);m_get_property(mpv,"duration",MPV_FORMAT_DOUBLE,&duration);m_get_property(mpv,"sub-delay",MPV_FORMAT_DOUBLE,&delay);
        char text[160];snprintf(text,sizeof(text),"%02d:%02d:%02d / %02d:%02d:%02d    Volume %d%%",(int)pos/3600,(int)pos/60%60,(int)pos%60,(int)duration/3600,(int)duration/60%60,(int)duration%60,volume);label(text,36,166,smallfont,muted,940);
        fill(36,218,950,10,0);if(duration>0){double ratio=pos/duration;if(ratio<0)ratio=0;if(ratio>1)ratio=1;fill(36,218,(int)(950*ratio),10,1);}
        const char*buttons[]={paused?tr("Retomar"):tr("Pausa"),tr("Legendas / Opções"),tr("Terminar Vídeo"),tr("Guardar Favorito")};
        for(int i=0;i<4;i++){int x=36+(i%2)*488,y=284+(i/2)*142;fill(x,y,464,112,controls_selected==i);label(buttons[i],x+20,y+37,font,white,430);}
        snprintf(text,sizeof(text),tr("Legenda: %d · desfasamento %+.1f s · X áudio · Y legendas"),font_size,delay);label(text,36,644,smallfont,muted,950);
    }else if(page==0){const char*names[]={tr("Filmes no cartão"),tr("Listas IPTV no cartão"),tr("Abrir URL direto"),tr("Lista IPTV por URL"),tr("Favoritos"),tr("Opções"),tr("Sair")};label(tr("Filmes com legendas e televisão por Internet"),36,110,smallfont,muted,950);for(int i=0;i<7;i++){int y=170+i*66;fill(36,y,950,56,selected==i);label(names[i],56,y+10,font,white,900);}}
    else if(page==1||page==2||page==3){label(page==1?directory:page==2?tr("Canais IPTV"):tr("Favoritos"),36,110,smallfont,muted,940);if(!count){label(page==3?tr("Ainda não há favoritos."):tr("Esta pasta ou lista está vazia."),36,225,font,white,950);label(tr("B volta atrás · START abre o menu principal"),36,290,smallfont,muted,950);}int start=selected/8*8;for(int i=start;i<count&&i<start+8;i++){int y=164+(i-start)*61;fill(36,y,950,53,selected==i);char t[320];snprintf(t,sizeof(t),"%s%s",items[i].folder?"▸  ":"",items[i].title);label(t,54,y+9,font,white,page==3?704:910);if(page==3){fill(786,y+5,190,43,0);label(tr("Remover"),804,y+13,smallfont,accent,160);}}char t[60];snprintf(t,sizeof(t),"%d / %d",count?selected+1:0,count);label(t,840,664,smallfont,accent,160);}
    else if(page==4){label(keyboard_mode==2?tr("URL da lista IPTV"):tr("URL direto do vídeo ou canal"),36,105,smallfont,muted,950);fill(36,148,950,66,0);const char*tail=url;size_t n=strlen(url);if(n>52)tail=url+n-52;label(*url?tail:"https://…",50,163,font,white,920);
        for(int i=0;i<50;i++){int x=36+(i%10)*96,y=244+(i/10)*64;fill(x,y,86,54,keysel==i);char s[2]={keyrows[i/10][i%10],0};if(shiftkey&&s[0]>='a'&&s[0]<='z')s[0]-=32;label(s,x+29,y+7,font,white,50);}
        const char*special[]={shiftkey?"abc":"ABC",tr("Apagar"),"http://","https://",tr("Limpar"),tr("Abrir")};for(int i=0;i<6;i++){int x=36+i*160;fill(x,582,150,64,keysel==50+i);label(special[i],x+10,602,smallfont,white,132);}label(tr("X apaga · Y maiúsculas · START abre · B cancela"),36,661,smallfont,muted,950);
    }else if(page==5){char a[100],b[100],c[100],d[128];snprintf(a,sizeof(a),tr("Vídeo no ecrã %s"),screenmode?tr("inferior"):tr("superior"));snprintf(b,sizeof(b),tr("Tamanho das legendas: %d"),font_size);snprintf(c,sizeof(c),"Volume: %d%%",volume);snprintf(d,sizeof(d),tr("Imagem: %s"),tr(image_names[image_mode]));const char*names[]={a,d,b,tr("Legendas: − 0,1 segundos"),tr("Legendas: + 0,1 segundos"),c,tr("Escolher legendas"),tr("Escolher áudio"),language?"Language: EN / PT":"Idioma: PT / EN",tr("Voltar")};for(int i=0;i<10;i++){int y=130+i*52;fill(36,y,950,44,selected==i);label(names[i],54,y+5,font,white,900);}label(tr("Ecrã cheio estica · Preencher corta os lados · Original mantém proporção"),36,660,smallfont,muted,950);}

footer:
    if(pSDL_GetTicks()<note_until)label(note,36,700,smallfont,accent,950);else label(page==9?tr("A escolhe · B volta · esquerda/direita muda de página"):page==3?tr("A abre · X remove · B volta · ou toque em Remover"):page==8?tr("Escolhe Continuar ou Ver do Início · B volta"):page==7?tr("Escolhe Cancelar ou Sair · B cancela"):page==6?tr("Escolhe Cancelar ou Remover · B cancela"):playing?tr("A pausa · B termina · SELECT ecrã · VOL+/− volume"):tr("D-pad + A ou toque · B volta · START menu"),36,711,smallfont,muted,950);
    pSDL_RenderPresent(renderer);
}
static int nav_count(void){if(page==9)return track_count+1;if(page==6||page==7||page==8)return 2;if(page==5)return 10;if(playing)return 4;if(page==0)return 7;if(page==4)return 56;return count;}
static void navigate(int dx,int dy){if(fetch_active)return;if(page==9){int n=track_count+1;if(dy)selected=(selected+dy+n)%n;if(dx){selected+=dx*7;if(selected<0)selected=0;if(selected>=n)selected=n-1;}return;}if(page==6||page==7||page==8){if(dx||dy)selected=1-selected;return;}if(page==4){if(dy){if(keysel<50){int next=keysel+dy*10;if(next<0)next=50+keysel%6;if(next>=50)next=50+keysel%6;keysel=next;}else keysel=dy<0?40+(keysel-50):keysel-50;}if(dx){int n=keysel<50?10:6,start=keysel<50?(keysel/10)*10:50;keysel=start+(keysel-start+dx+n)%n;}return;}if(playing&&page!=5){if(dx)command("seek",dx<0?"-10":"10","relative");if(dy)adjust_volume(dy<0?5:-5);return;}int n=nav_count();if(n&&dy)selected=(selected+dy+n)%n;if(n&&dx&&(page==1||page==2||page==3)){selected+=dx*8;if(selected<0)selected=0;if(selected>=n)selected=n-1;}}
static void button(int b){fprintf(stderr,"Botao: %d\n",b);if(b==15||b==16){if(keysfd<0)adjust_volume(b==16?5:-5);return;}if(b==6){toggle_lower();return;}if(b==1){back();return;}if(fetch_active)return;if(page==9){if(b==0)tracks_select(selected);else if(b==7)tracks_close();else if(b==2)tracks_open(0);else if(b==3)tracks_open(1);return;}if(page==8){if(b==0)activate(selected);else if(b==7)resume_cancel();return;}if(page==7){if(b==0)activate(selected);else if(b==7)exit_cancel();return;}if(page==6){if(b==0)activate(selected);else if(b==7)favorite_cancel();return;}if(b==2&&page==3&&!playing){favorite_confirm(selected);return;}if(page==4){if(b==0)keyboard_key(keysel);else if(b==2)keyboard_key(51);else if(b==3)keyboard_key(50);else if(b==7)keyboard_accept();return;}if(b==0){if(playing&&page!=5)toggle_pause();else activate(selected);}else if(b==2&&playing)tracks_open(0);else if(b==3&&playing)tracks_open(1);else if(b==4&&playing)shoulder(-1);else if(b==5&&playing)shoulder(1);else if(b==7){if(playing&&page==5){options_close();}else if(playing){options_open();}else{page=0;selected=0;}}}
static void click(int x,int y){if(fetch_active||lower_off||stop_pending)return;if(page==9){if(x>=36&&x<986&&y>=150&&y<640&&(y-150)%70<66){int i=selected/7*7+(y-150)/70;if(i<=track_count){selected=i;tracks_select(i);}}return;}if(playing&&screenmode){toggle_pause();return;}if(page==6||page==7||page==8){if(y>=390&&y<490&&x>=36&&x<988){int i=(x-36)/488;if((x-36)%488<464){selected=i;activate(i);}}return;}if(page==3&&x>=786&&x<976&&y>=164&&y<652){int row=(y-164)/61;if((y-164)%61>=5&&(y-164)%61<48)favorite_confirm(selected/8*8+row);return;}if(page==4){if(y>=244&&y<564){int row=(y-244)/64,col=(x-36)/96;if(x>=36&&col<10&&row<5){keysel=row*10+col;keyboard_key(keysel);}}else if(y>=582&&y<646&&x>=36&&x<996){keysel=50+(x-36)/160;if(keysel<56)keyboard_key(keysel);}return;}if(playing&&page!=5){if(y>=284&&y<568&&x>=36){int col=(x-36)/488,row=(y-284)/142;if(col<2&&row<2&&(y-284)%142<112){controls_selected=row*2+col;activate(controls_selected);}}return;}int top=page==0?170:page==5?130:164,step=page==0?66:page==5?52:61;if(y>=top&&x>=36&&x<986){int i=(y-top)/step;if(page==1||page==2||page==3)i+=selected/8*8;if((page!=1&&page!=2&&page!=3)||(y<top+step*8)){if(i<nav_count()){selected=i;activate(i);}}}}
static void touch_pump(void){if(touchfd<0)return;struct input_event e;for(int n=0;n<256&&read(touchfd,&e,sizeof(e))==sizeof(e);n++){if(e.type==EV_ABS){if(e.code==ABS_MT_SLOT)tslot=e.value;else if(!tslot){if(e.code==ABS_MT_POSITION_X)tx=e.value;else if(e.code==ABS_MT_POSITION_Y)ty=e.value;else if(e.code==ABS_MT_TRACKING_ID)tdown=e.value>=0;}}else if(e.type==EV_SYN&&e.code==SYN_REPORT){if(lower_off&&tdown)touch_inhibited=1;if(tprev&&!tdown&&!touch_inhibited){double u=tx/1024.,v=ty/768.;int x=(calibration[0]*u+calibration[1]*v+calibration[2])*1023,y=(calibration[3]*u+calibration[4]*v+calibration[5])*767;click(x,y);}if(!tdown)touch_inhibited=0;tprev=tdown;}}}
static void render_video(void){if(!playing||!render_context||!atomic_exchange(&render_pending,0))return;if(glcontext)pSDL_GL_MakeCurrent(video,glcontext);m_render_context_update(render_context);
    if(glcontext){pSDL_GL_MakeCurrent(video,glcontext);int w,h;pSDL_GL_GetDrawableSize(video,&w,&h);mpv_opengl_fbo fbo={0,w,h,0};int flip=1;mpv_render_param p[]={{MPV_RENDER_PARAM_OPENGL_FBO,&fbo},{MPV_RENDER_PARAM_FLIP_Y,&flip},{MPV_RENDER_PARAM_INVALID,NULL}};if(gl_bind_framebuffer)gl_bind_framebuffer(0x8D40,0);
        if(gl_viewport)gl_viewport(0,0,w,h);
        if(gl_disable)gl_disable(0x0C11);
        if(gl_color_mask)gl_color_mask(1,1,1,1);
        if(gl_clear_color)gl_clear_color(0,0,0,1);
        if(gl_clear)gl_clear(0x4000);
        m_render_context_render(render_context,p);
        if(smoke&&!stress){
            static int captured[2];double pos=0;m_get_property(mpv,"time-pos",MPV_FORMAT_DOUBLE,&pos);
            for(int index=0;index<2;index++)if(!captured[index]&&pos>=(index?6.5:3.5)){
                void(*read_pixels)(int,int,int,int,unsigned int,unsigned int,void*)=NULL;
                *(void**)(&read_pixels)=get_gl(NULL,"glReadPixels");
                unsigned char *buffer=malloc((size_t)w*h*4);
                if(read_pixels&&buffer){read_pixels(0,0,w,h,0x1908,0x1401,buffer);char path[64];snprintf(path,sizeof(path),"test-subtitles-%d.ppm",index);FILE*f=fopen(path,"wb");if(f){fprintf(f,"P6\n%d %d\n255\n",w,h);for(int row=h-1;row>=0;row--)for(int col=0;col<w;col++)fwrite(buffer+((size_t)row*w+col)*4,1,3,f);fclose(f);}}
                free(buffer);captured[index]=1;
            }
        }
        pSDL_GL_SwapWindow(video);m_render_context_report_swap(render_context);}
    else{int size[]={1024,768};size_t stride=1024*4;mpv_render_param p[]={{MPV_RENDER_PARAM_SW_SIZE,size},{MPV_RENDER_PARAM_SW_FORMAT,(void*)"bgr0"},{MPV_RENDER_PARAM_SW_STRIDE,&stride},{MPV_RENDER_PARAM_SW_POINTER,pixels},{MPV_RENDER_PARAM_INVALID,NULL}};memset(pixels,0,1024*768*4);m_render_context_render(render_context,p);pSDL_UpdateTexture(vtexture,NULL,pixels,stride);pSDL_RenderCopy(vrenderer,vtexture,NULL,NULL);pSDL_RenderPresent(vrenderer);}
}
static void mpv_events(void){if(!mpv)return;for(int i=0;i<100;i++){mpv_event*e=m_wait_event(mpv,0);if(!e||e->event_id==MPV_EVENT_NONE)break;if(e->event_id==MPV_EVENT_FILE_LOADED&&playing){fprintf(stderr,"FILE_LOADED\n");file_loaded=1;apply_image_mode();playback_error[0]=0;stats_at=pSDL_GetTicks()+1500;note_until=0;if(screenmode){pSDL_HideWindow(menu);if(backlight_saved<0)backlight_saved=read_num(upper_light);write_num(upper_light,0);}else{pSDL_ShowWindow(menu);pSDL_RaiseWindow(menu);}atomic_store(&render_pending,1);}
        else if(e->event_id==MPV_EVENT_LOG_MESSAGE){mpv_event_log_message*message=e->data;if(message&&message->text)classify_mpv_warning(message->text);}
        else if(e->event_id==MPV_EVENT_END_FILE){mpv_event_end_file*end=e->data;fprintf(stderr,"END_FILE reason=%d error=%d\n",end?(int)end->reason:-1,end?end->error:0);int reason=end?(int)end->reason:-1,error=end?end->error:0;if(reason==MPV_END_FILE_REASON_STOP){if(stop_pending)finish_playback();continue;}if(reason==MPV_END_FILE_REASON_REDIRECT)continue;if(error<0){if(!playback_error[0])snprintf(playback_error,sizeof(playback_error),"%s",tr("Falha na reprodução. Consulta o log."));failure_pending=stop_pending=1;stop_deadline=pSDL_GetTicks()+350;continue;}else playback_error[0]=0;if(reason==MPV_END_FILE_REASON_EOF&&!is_url(uri))resume_clear(uri);if(playing)finish_playback();}
        else if(e->event_id==MPV_EVENT_COMMAND_REPLY&&e->error<0){fprintf(stderr,"Comando mpv: %d\n",e->error);notify(tr("Não foi possível executar a operação."));}}
}
static void screenshot(const char*path){unsigned char *buf=malloc(1024*768*4);if(!buf)return;if(pSDL_RenderReadPixels(renderer,NULL,RGBA32,buf,1024*4)==0){FILE*f=fopen(path,"wb");if(f){fprintf(f,"P6\n1024 768\n255\n");for(int i=0;i<1024*768;i++)fwrite(buf+i*4,1,3,f);fclose(f);}}free(buf);}

int main(int argc,char**argv){
    if(argc<2||strlen(argv[1])>=sizeof(app))return 1;
    snprintf(app,sizeof(app),"%s",argv[1]);preview=argc>2&&!strcmp(argv[2],"--preview");stress=argc>3&&!strcmp(argv[2],"--stress");test_eof=argc>3&&!strcmp(argv[2],"--eof-test");smoke=argc>3&&(!strcmp(argv[2],"--smoke")||stress||test_eof);
    signal(SIGINT,sig_handler);signal(SIGTERM,sig_handler);signal(SIGHUP,sig_handler);settings_load();favorites_load();
    void*sdl=dlopen("libSDL2-2.0.so.0",RTLD_NOW|RTLD_LOCAL);void*ttf=dlopen("libSDL2_ttf-2.0.so.0",RTLD_NOW|RTLD_LOCAL);if(!sdl||!ttf||!api_load(sdl,1)){fprintf(stderr,"SDL load failed: %s\n",dlerror());return 1;}
#define SL(name) *(void**)(&p##name)=dlsym(sdl,#name);if(!p##name)return 1
    SL(SDL_SetHint);SL(SDL_DisableScreenSaver);SL(SDL_EnableScreenSaver);SL(SDL_IsScreenSaverEnabled);SL(SDL_HideWindow);SL(SDL_ShowCursor);SL(SDL_GL_SetAttribute);SL(SDL_GL_CreateContext);SL(SDL_GL_DeleteContext);SL(SDL_GL_SetSwapInterval);SL(SDL_RenderSetLogicalSize);SL(SDL_RenderSetClipRect);
#define TL(var,name) *(void**)(&var)=dlsym(ttf,name);if(!var)return 1
    TL(ttfInit,"TTF_Init");TL(ttfOpen,"TTF_OpenFont");TL(ttfRender,"TTF_RenderUTF8_Blended");TL(ttfClose,"TTF_CloseFont");TL(ttfQuit,"TTF_Quit");
    pSDL_SetHint("SDL_VIDEO_ALLOW_SCREENSAVER","1");pSDL_SetHint("SDL_TOUCH_MOUSE_EVENTS","0");pSDL_SetHint("SDL_MOUSE_TOUCH_EVENTS","0");
    if(pSDL_Init(0x20|0x200|0x4000)<0||ttfInit()<0){fprintf(stderr,"SDL init: %s\n",pSDL_GetError());return 1;}pSDL_EnableScreenSaver();pSDL_ShowCursor(0);
    char fp[4096];path_join(fp,sizeof(fp),app,"fonts/DejaVuSans.ttf");font=ttfOpen(fp,29);smallfont=ttfOpen(fp,23);largefont=ttfOpen(fp,43);if(!font||!smallfont||!largefont){fprintf(stderr,"Font failed\n");pSDL_Quit();return 1;}
    int display=pSDL_GetNumVideoDisplays()>1?1:0;menu=pSDL_CreateWindow("BaetaMP RG DS+",0x2fff0000|display,0x2fff0000|display,1024,768,4|16);if(!menu)return 1;
    if(!preview)pSDL_SetWindowFullscreen(menu,FULLSCREEN_DESKTOP);
    renderer=pSDL_CreateRenderer(menu,-1,1);if(!renderer)return 1;pSDL_RenderSetLogicalSize(renderer,1024,768);pSDL_JoystickOpen(0);fprintf(stderr,"Menu display=%d\n",pSDL_GetWindowDisplayIndex(menu));
    keysfd=open("/dev/input/event5",O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(keysfd>=0){char name[128]={0};if(ioctl(keysfd,EVIOCGNAME(sizeof(name)),name)<0||!strstr(name,"ANBERNIC-rk3568-keys")){close(keysfd);keysfd=-1;}}
    fprintf(stderr,"Volume evdev: %s\n",keysfd>=0?"event5 (KEY_VOLUMEUP/DOWN)":"fallback SDL 15/16");
    touchfd=open("/dev/input/by-path/platform-fe5e0000.i2c-event",O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(touchfd<0)touchfd=open("/dev/input/event1",O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    const char *cal=getenv("RGDS_TOUCH_CALIBRATION");if(cal){FILE*f=fopen(cal,"r");if(f){char magic[64];double c[6];if(fscanf(f,"%63s %lf %lf %lf %lf %lf %lf",magic,c,c+1,c+2,c+3,c+4,c+5)==7&&!strcmp(magic,"RGDS_TOUCH_CALIBRATION_V1")){int valid=1;for(int i=0;i<6;i++)if(!isfinite(c[i]))valid=0;if(valid)memcpy(calibration,c,sizeof(c));}fclose(f);}}
    restore_menu_session();
    if(preview){
        for(int lang=0;lang<2;lang++){
            language=lang;page=0;selected=0;note_until=0;char path[128];
#define PREVIEW(name) snprintf(path,sizeof(path),"preview-%s-%s.ppm",lang?"en":"pt",name);draw();screenshot(path)
            PREVIEW("menu");exit_open();PREVIEW("exit");exit_cancel();
            resume_open("/example/film.mp4","Film with a saved position",3672);PREVIEW("resume");resume_cancel();
            fav_count=2;snprintf(favorites[0].title,256,"RTP — Notícias e programas");snprintf(favorites[0].uri,4096,"https://example.org/rtp");snprintf(favorites[1].title,256,"Favorite added by mistake");snprintf(favorites[1].uri,4096,"https://example.org/test");
            favorites_open(1);PREVIEW("favorites");favorite_confirm(1);PREVIEW("remove");favorite_cancel();fav_count=0;
            keyboard_open(1);snprintf(url,sizeof(url),"https://example.org/channel");PREVIEW("url");page=5;selected=8;PREVIEW("settings");
            if(mpv_load()){playing=1;page=1;snprintf(title,sizeof(title),"Film / IPTV Channel");note_until=0;PREVIEW("playback");
                track_return_page=5;track_return_selected=6;track_audio=0;track_count=4;page=9;selected=1;
                tracks[0]=(TrackChoice){.id=-1};snprintf(tracks[0].name,sizeof(tracks[0].name),"%s",tr("Desligadas"));
                tracks[1]=(TrackChoice){.id=2,.active=1};snprintf(tracks[1].name,sizeof(tracks[1].name),"%s · Portugal",tr("Português"));snprintf(tracks[1].detail,sizeof(tracks[1].detail),"#2 · pt-PT · subrip");
                tracks[2]=(TrackChoice){.id=7};snprintf(tracks[2].name,sizeof(tracks[2].name),"%s · SDH",tr("Inglês"));snprintf(tracks[2].detail,sizeof(tracks[2].detail),"#7 · eng · ass");
                tracks[3]=(TrackChoice){.id=15};snprintf(tracks[3].name,sizeof(tracks[3].name),"%s · Film.srt",tr("Português (Brasil)"));snprintf(tracks[3].detail,sizeof(tracks[3].detail),"#15 · pt-BR · subrip · %s",tr("Externa"));PREVIEW("subtitles");
                track_audio=1;track_count=2;tracks[0]=tracks[1];tracks[0].id=3;snprintf(tracks[0].detail,sizeof(tracks[0].detail),"#3 · por · aac");tracks[1]=tracks[2];tracks[1].id=11;snprintf(tracks[1].detail,sizeof(tracks[1].detail),"#11 · eng · aac");selected=0;PREVIEW("audio");playing=0;page=0;}
#undef PREVIEW
        }
        running=0;
    }
    if(smoke){play(argv[3],"Teste de reprodução 720p");smoke_began=pSDL_GetTicks();stress_next=smoke_began+1200;}
    while(running&&!interrupted){
        if(fetch_active&&atomic_load(&fetch.done)){pthread_join(fetch.thread,NULL);fetch_active=0;if(!atomic_load(&fetch.cancel)){if(fetch.result||!fetch.data)notify(tr("Não foi possível descarregar a lista."));else{int rc=parse_list(fetch.data,fetch.origin);if(rc<0){page=0;play(fetch.address,"Stream HLS");}}}free(fetch.data);fetch.data=NULL;}
        mpv_events();volume_pump();touch_pump();awake_pump();SDL_Event e;while(pSDL_PollEvent(&e)){
            if(e.type==0x100){if(playing||fetch_active)back();else exit_open();continue;}
            if(e.type==0x300){int key;memcpy(&key,e.bytes+20,4);if(key==27)back();else if(key==13)button(0);else if(key==1073741906)navigate(0,-1);else if(key==1073741905)navigate(0,1);else if(key==1073741904)navigate(-1,0);else if(key==1073741903)navigate(1,0);else if(key==1073741952&&keysfd<0)adjust_volume(5);else if(key==1073741953&&keysfd<0)adjust_volume(-5);}
            else if(e.type==0x603)button(e.bytes[12]);
            else if(e.type==0x602){int h=e.bytes[13];if(h&1)navigate(0,-1);if(h&4)navigate(0,1);if(h&8)navigate(-1,0);if(h&2)navigate(1,0);}
            else if(e.type==0x600&&pSDL_GetTicks()>=axis_until){int16_t v;memcpy(&v,e.bytes+16,2);if(abs(v)>16000){if(e.bytes[12]==0)navigate(v<0?-1:1,0);if(e.bytes[12]==1)navigate(0,v<0?-1:1);axis_until=pSDL_GetTicks()+220;}}
            else if(touchfd<0&&e.type==0x402){int x,y;memcpy(&x,e.bytes+20,4);memcpy(&y,e.bytes+24,4);click(x,y);}
        }
        render_video();uint32_t now=pSDL_GetTicks();
        if(playing&&stats_at&&now>=stats_at){stats_at=0;if(smoke&&pixels){FILE*f=fopen("test-video.bgr0","wb");if(f){fwrite(pixels,1,1024*768*4,f);fclose(f);}}const char*props[]={"video-codec","audio-codec","hwdec-current","video-params/w","video-params/h","sid",NULL};for(int i=0;props[i];i++){char*v=NULL;if(m_get_property(mpv,props[i],MPV_FORMAT_STRING,&v)>=0&&v){fprintf(stderr,"%s: %s\n",props[i],v);m_free(v);}}}
        if(stress&&stress_stops<3&&now>=stress_next){
            if(playing&&file_loaded&&!stop_pending){stop_playback();stress_stops++;stress_wait=1;stress_next=now+200;}
            else if(!playing&&stress_wait){play(argv[3],"Teste reabrir / terminar");stress_wait=0;stress_next=now+1200;}
        }else if(stress&&stress_stops==3&&!playing){running=0;}
        if(smoke&&now-smoke_began>(stress?15000:test_eof?13000:7000)){running=0;fprintf(stderr,"SMOKE file_loaded=%d\n",file_loaded);}
        if(stop_pending&&now>=stop_deadline){fprintf(stderr,"%s\n",failure_pending?"End diagnostics collected.":"Stop: limite de espera atingido.");finish_playback();}
        if(now>=redraw_until){if(!(playing&&((screenmode&&page!=9)||lower_off)))draw();redraw_until=now+100;}pSDL_Delay(5);
    }
    if(fetch_active){atomic_store(&fetch.cancel,1);pthread_join(fetch.thread,NULL);free(fetch.data);}
    fprintf(stderr,"Application shutdown: begin\n");
    shutting_down=1;
    stop_playback();
    while(playing&&pSDL_GetTicks()<stop_deadline){mpv_events();render_video();pSDL_Delay(5);}
    if(playing)finish_playback();
    awake_stop();restore_light();pSDL_EnableScreenSaver();if(video||render_context)video_destroy();
    if(mpv){fprintf(stderr,"mpv destroy: begin\n");m_terminate_destroy(mpv);fprintf(stderr,"mpv destroy: done\n");}
    if(touchfd>=0)close(touchfd);
    if(keysfd>=0)close(keysfd);
    settings_save();ttfClose(font);ttfClose(smallfont);ttfClose(largefont);ttfQuit();pSDL_DestroyRenderer(renderer);pSDL_DestroyWindow(menu);pSDL_Quit();return smoke&&!file_loaded?2:0;
}

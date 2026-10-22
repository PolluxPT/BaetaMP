/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main player_app_main
#include "../src/player.c"
#undef main
#include <assert.h>
static uint32_t test_ticks(void){return 0;}
static int seek_calls;
static char seek_amount[16];
static int test_command_async(mpv_handle*handle,uint64_t id,const char*const*args){(void)handle;(void)id;assert(!strcmp(args[0],"seek"));assert(!strcmp(args[2],"relative"));snprintf(seek_amount,sizeof(seek_amount),"%s",args[1]);seek_calls++;return 0;}
// Deterministic digest stub lets ARM64 unit tests exercise both old/new keys.
static void test_digest(uint8_t*out,const uint8_t*data,size_t len){memset(out,0,16);for(size_t i=0;i<len;i++)out[i%16]=(uint8_t)(out[i%16]*33+data[i]);}
int main(void){
    pSDL_GetTicks=test_ticks;
    char out[4096];
    relative_uri(out,sizeof(out),"/cartao/listas/canais.m3u","../filme.mp4");
    assert(!strcmp(out,"/cartao/listas/../filme.mp4"));
    relative_uri(out,sizeof(out),"https://example.org/listas/canais.m3u","canal.ts");
    assert(!strcmp(out,"https://example.org/listas/canal.ts"));
    relative_uri(out,sizeof(out),"https://example.org/listas/canais.m3u","/canal.ts");
    assert(!strcmp(out,"https://example.org/canal.ts"));
    char list[]="\xef\xbb\xbf#EXTM3U\r\n#EXTINF:-1,Canal Português\r\nhttps://example.org/a?x=1&y=2\r\n# comentario\n#EXTINF:-1,Segundo\nlocal.mp4\n";
    assert(parse_list(list,"/cartao/canais.m3u")==2);
    assert(!strcmp(items[0].title,"Canal Português"));
    assert(!strcmp(items[0].uri,"https://example.org/a?x=1&y=2"));
    assert(!strcmp(items[1].uri,"/cartao/local.mp4"));
    char hls[]="#EXTM3U\n#EXT-X-TARGETDURATION:6\n#EXTINF:6,\nsegment.ts\n";
    assert(parse_list(hls,"https://example.org/live.m3u8")==-1);
    HttpRequest request;
    char enhanced[]="#EXTM3U\n#EXTINF:-1,Canal HTTP\n#EXTVLCOPT:http-user-agent=BaetaTest 1.0\n#EXTVLCOPT:http-referrer=https://allowed.example/?a=1&b=2\nhttps://example.org/live.m3u8|Origin=https%3A%2F%2Forigin.example\n#EXTINF:-1,Outro\nhttps://example.org/second\n";
    assert(parse_list(enhanced,"https://example.org/list.m3u")==2);assert(http_request(items[0].uri,&request));
    assert(!strcmp(request.url,"https://example.org/live.m3u8"));assert(!strcmp(request.ua,"BaetaTest 1.0"));assert(!strcmp(request.referer,"https://allowed.example/?a=1&b=2"));assert(!strcmp(request.origin,"https://origin.example"));
    assert(http_request(items[1].uri,&request)&&!strcmp(request.ua,default_user_agent)&&!request.referer[0]);
    char kodi[]="#EXTM3U\n#EXTINF:-1,Kodi Headers\n#KODIPROP:inputstream.adaptive.stream_headers=User-Agent=BaetaTest&Referer=https%3A%2F%2Fallowed.example\nhttps://example.org/stream\n";
    assert(parse_list(kodi,"/test.m3u")==1&&http_request(items[0].uri,&request));assert(!strcmp(request.ua,"BaetaTest")&&!strcmp(request.referer,"https://allowed.example"));
    assert(!http_request("https://example.org/test|User-Agent=bad%0d%0aInjected%3Ayes",&request));
    char priority[]="#EXTM3U\n#EXTINF:-1,Preferencia URL\n#EXTVLCOPT:http-user-agent=Default\nhttps://example.org/live|User-Agent=Specific\n";
    assert(parse_list(priority,"/test.m3u")==1&&http_request(items[0].uri,&request));assert(!strcmp(request.ua,"Specific"));
    relative_uri(out,sizeof(out),"https://example.org/list.m3u","//cdn.example.org/test.ts");assert(!strcmp(out,"https://cdn.example.org/test.ts"));
    char reject[]="#EXTM3U\n#EXTINF:-1,Bad\nexec://touch%20bad\n#EXTINF:-1,Good\nhttps://example.org/good\n";
    assert(parse_list(reject,"/cartao/test.m3u")==1);
    assert(is_url("rtsp://example.org/live"));assert(!is_url("file:///etc/passwd"));
    assert(is_video("FILME.MKV"));assert(is_list("canais.M3U8"));assert(!is_video("filme.srt"));
    keyboard_open(1);keyboard_key(53);assert(!strcmp(url,"https://"));keyboard_key(0);assert(!strcmp(url,"https://q"));keyboard_key(51);assert(!strcmp(url,"https://"));
    keysel=0;navigate(0,-1);assert(keysel>=50&&keysel<56);navigate(0,1);assert(keysel<50);
    char temp[]="/tmp/rgds-test-XXXXXX";assert(mkdtemp(temp));
    char lower[4096],stockvol[4096];path_join(lower,sizeof(lower),temp,"lower");path_join(stockvol,sizeof(stockvol),temp,"volume");
    lower_light=lower;volume_node=stockvol;write_num(lower_light,70);
    playing=1;page=0;screenmode=0;paused=0;
    button(6);assert(lower_off&&read_num(lower_light)==0);
    click(100,290);assert(!paused); // Dark controls must not pause the film.
    button(6);assert(!lower_off&&read_num(lower_light)==70);
    button(6);restore_light();assert(!lower_off&&read_num(lower_light)==70);
    screenmode=1;button(6);assert(!lower_off&&read_num(lower_light)==70);screenmode=0;
    int touchpipe[2];assert(pipe(touchpipe)==0);touchfd=touchpipe[0];fcntl(touchfd,F_SETFL,O_NONBLOCK);
    button(6);assert(lower_off);
    struct input_event gesture[4];memset(gesture,0,sizeof(gesture));
    gesture[0].type=EV_ABS;gesture[0].code=ABS_MT_POSITION_X;gesture[0].value=100;
    gesture[1].type=EV_ABS;gesture[1].code=ABS_MT_POSITION_Y;gesture[1].value=290;
    gesture[2].type=EV_ABS;gesture[2].code=ABS_MT_TRACKING_ID;gesture[2].value=0;
    gesture[3].type=EV_SYN;gesture[3].code=SYN_REPORT;
    assert(write(touchpipe[1],gesture,sizeof(gesture))==sizeof(gesture));touch_pump();assert(touch_inhibited);
    button(6);assert(!lower_off);gesture[0]=gesture[2];gesture[0].value=-1;gesture[1]=gesture[3];
    assert(write(touchpipe[1],gesture,2*sizeof(*gesture))==2*sizeof(*gesture));touch_pump();assert(!paused&&!touch_inhibited);
    close(touchfd);close(touchpipe[1]);touchfd=-1;restore_light();
    volume=65;button(16);assert(volume==70);button(15);assert(volume==65);
    volume=100;button(16);assert(volume==100);volume=0;button(15);assert(volume==0);
    int pipefd[2];assert(pipe(pipefd)==0);keysfd=pipefd[0];fcntl(keysfd,F_SETFL,O_NONBLOCK);
    volume=65;button(16);assert(volume==65); // SDL duplicate is ignored when raw input is available.
    struct input_event e={0};e.type=EV_KEY;e.code=KEY_VOLUMEUP;e.value=1;assert(write(pipefd[1],&e,sizeof(e))==sizeof(e));volume_pump();assert(volume==70);
    e.code=KEY_VOLUMEDOWN;assert(write(pipefd[1],&e,sizeof(e))==sizeof(e));volume_pump();assert(volume==65);
    close(keysfd);close(pipefd[1]);keysfd=-1;unlink(lower);unlink(stockvol);
    snprintf(app,sizeof(app),"%s",temp);char config[4096],films[4096],movie[4096];path_join(config,sizeof(config),temp,"config");path_join(films,sizeof(films),temp,"Filmes");assert(mkdir(config,0700)==0);assert(mkdir(films,0700)==0);
    path_join(movie,sizeof(movie),films,"Teste.mp4");FILE*file=fopen(movie,"w");assert(file);fclose(file);
    playing=0;browse(films);assert(count==1);save_menu_session();page=0;count=0;restore_menu_session();assert(page==1&&count==1&&!strcmp(directory,films));
    page=2;count=2;selected=1;snprintf(items[0].title,256,"Canal um");snprintf(items[0].uri,4096,"https://example.org/one");snprintf(items[1].title,256,"Canal dois");snprintf(items[1].uri,4096,"https://example.org/two");save_menu_session();page=count=selected=0;restore_menu_session();assert(page==2&&count==2&&selected==1&&!strcmp(items[1].uri,"https://example.org/two"));
    // Persistence, safe cancellation, touch targeting, deletion order and write failures.
    for(int i=0;i<3;i++){snprintf(uri,sizeof(uri),"https://example.org/fav%d|User-Agent=Test",i);snprintf(title,sizeof(title),"Favorito %d",i);favorite_add();}
    assert(fav_count==3);favorites_open(1);button(2);assert(page==6&&selected==0&&favorite_pending==1);
    button(0);assert(page==3&&selected==1&&fav_count==3); // A defaults to Cancelar.
    button(2);button(1);assert(page==3&&selected==1&&fav_count==3);
    click(850,164+61+20);assert(page==6&&favorite_pending==1); // Row removal does not play.
    navigate(1,0);assert(selected==1);button(0);assert(page==3&&count==2&&selected==1&&fav_count==2);
    assert(strstr(favorites[0].uri,"fav0|")&&strstr(favorites[1].uri,"fav2|"));
    fav_count=0;favorites_load();assert(fav_count==2&&strstr(favorites[1].uri,"fav2|"));
    char favpath[4096],newpath[4096];path_join(favpath,sizeof(favpath),config,"favoritos.txt");path_join(newpath,sizeof(newpath),config,"favoritos.new");
    assert(mkdir(newpath,0700)==0);favorites_open(0);favorite_confirm(0);activate(1);assert(page==3&&fav_count==2&&strstr(favorites[0].uri,"fav0|"));
    fav_count=0;favorites_load();assert(fav_count==2);assert(rmdir(newpath)==0);
    favorites_open(0);favorite_confirm(0);activate(1);assert(fav_count==1&&selected==0&&strstr(favorites[0].uri,"fav2|"));
    favorite_confirm(0);click(600,430);assert(page==3&&fav_count==0&&count==0&&selected==0);
    fav_count=0;favorites_load();assert(fav_count==0);button(2);assert(page==3);activate(0);assert(!playing);
    page=2;favorite_confirm(0);assert(page==2);playing=1;page=3;favorite_confirm(0);assert(page==3);playing=0;
    // Repeated B can only open/cancel exit; A defaults to Cancelar.
    page=0;selected=4;running=1;button(1);assert(page==7&&selected==0&&running);
    button(0);assert(page==0&&selected==4&&running);
    for(int i=0;i<8;i++){button(1);assert(running);}
    assert(page==0);activate(6);assert(page==7&&selected==0);navigate(1,0);button(1);assert(page==0&&running);
    activate(6);navigate(0,1);button(0);assert(!running);running=1;page=0;
    click(100,170+6*66+20);assert(page==7&&running);click(600,430);assert(!running);running=1;page=0;
    // L/R send the same relative seeks as D-pad in films, including Options.
    m_command_async=test_command_async;mpv=(mpv_handle*)&seek_calls;playing=1;page=1;snprintf(uri,sizeof(uri),"/test/filme.mp4");
    button(4);assert(seek_calls==1&&!strcmp(seek_amount,"-10"));button(5);assert(seek_calls==2&&!strcmp(seek_amount,"10"));
    navigate(-1,0);assert(seek_calls==3&&!strcmp(seek_amount,"-10"));
    page=5;option_page=1;button(5);assert(seek_calls==4&&!strcmp(seek_amount,"10"));
    page=2;assert(shoulder_seeks());snprintf(uri,sizeof(uri),"https://example.org/live");assert(!shoulder_seeks());
    page=3;assert(!shoulder_seeks());page=5;option_page=3;assert(!shoulder_seeks());option_page=0;assert(shoulder_seeks());
    mpv=NULL;playing=0;page=0;
    md5_sum=test_digest;char resume_dir[4096],resume_file[4096],legacy_movie[4096],old_key[4096];
    path_join(resume_dir,sizeof(resume_dir),config,"retomar");assert(mkdir(resume_dir,0700)==0);
    assert(resume_path(movie,resume_file,sizeof(resume_file)));assert(strstr(resume_file,"/config/retomar/")&&strlen(strrchr(resume_file,'/')+1)==32);
    file=fopen(resume_file,"w");assert(file);fputs("start=125.250000\n",file);fclose(file);assert(resume_position(movie)==125.25);
    resume_open(movie,"Teste",125.25);assert(page==8&&selected==0);button(1);assert(page==0&&running);
    resume_open(movie,"Teste",125.25);navigate(1,0);assert(selected==1);Item chosen;double position;resume_take(selected,&chosen,&position);assert(position==0&&!strcmp(chosen.uri,movie)&&page==0);
    resume_open(movie,"Teste",125.25);resume_take(0,&chosen,&position);assert(position==125.25&&page==0);
    resume_clear(movie);assert(resume_position(movie)==0&&!is_url(movie));assert(!resume_path("https://example.org/movie",resume_file,sizeof(resume_file)));
    setenv("RGDS_LEGACY_APP","/old/MediaPlayer_RGDSPlus",1);snprintf(legacy_movie,sizeof(legacy_movie),"/old/MediaPlayer_RGDSPlus/Filmes/Teste.mp4");
    assert(resume_path(legacy_movie,old_key,sizeof(old_key)));file=fopen(old_key,"w");assert(file);fputs("start=77.5\n",file);fclose(file);assert(resume_position(movie)==77.5);
    assert(resume_path(movie,resume_file,sizeof(resume_file)));file=fopen(resume_file,"w");assert(file);fputs("start=0\n",file);fclose(file);assert(resume_position(movie)==0);
    resume_clear(movie);assert(access(old_key,F_OK)<0&&access(resume_file,F_OK)<0);
    file=fopen(resume_file,"w");assert(file);fputs("start=nan\nstart=-2\nstart=inf\n",file);fclose(file);assert(resume_position(movie)==0);unlink(resume_file);
    snprintf(legacy_movie,sizeof(legacy_movie),"/old/MediaPlayer_RGDSPlus/Filmes/Teste.mp4");migrated_path(legacy_movie,sizeof(legacy_movie));assert(!strcmp(legacy_movie,movie));
    unsetenv("RGDS_LEGACY_APP");rmdir(resume_dir);
    assert(mkdir(resume_dir,0700)==0);
    setenv("RGDS_LEGACY_APP","/old/BaetaMediaPlayer",1);setenv("RGDS_LEGACY_APP_2","/old/MediaPlayer_RGDSPlus",1);
    snprintf(legacy_movie,sizeof(legacy_movie),"/old/MediaPlayer_RGDSPlus/Filmes/Teste.mp4");assert(resume_path(legacy_movie,old_key,sizeof(old_key)));file=fopen(old_key,"w");assert(file);fputs("start=42\n",file);fclose(file);assert(resume_position(movie)==42);
    snprintf(legacy_movie,sizeof(legacy_movie),"/old/BaetaMediaPlayer/Filmes/Teste.mp4");assert(resume_path(legacy_movie,resume_file,sizeof(resume_file)));file=fopen(resume_file,"w");assert(file);fputs("start=21\n",file);fclose(file);assert(resume_position(movie)==21);resume_clear(movie);assert(access(old_key,F_OK)<0&&access(resume_file,F_OK)<0);
    snprintf(legacy_movie,sizeof(legacy_movie),"/old/BaetaMediaPlayer/Filmes/Teste.mp4");migrated_path(legacy_movie,sizeof(legacy_movie));assert(!strcmp(legacy_movie,movie));unsetenv("RGDS_LEGACY_APP");unsetenv("RGDS_LEGACY_APP_2");rmdir(resume_dir);
    // Unsorted favorites from different sources become A-Z without changing metadata.
    file=fopen(favpath,"w");assert(file);
    fputs("zebra\thttps://source-b.example/z\nÁgua\thttps://source-a.example/water|User-Agent=Keep%20Me\nbeta\thttps://source-b.example/b\nalfa\thttps://source-a.example/a\nÇinema\thttps://source-c.example/c\nÉvora\thttps://source-a.example/e\nRTP 2\thttps://source-b.example/2\nrtp 1\thttps://source-c.example/1\n",file);fclose(file);
    fav_count=0;favorites_load();assert(fav_count==8);const char*sorted[]={"Água","alfa","beta","Çinema","Évora","rtp 1","RTP 2","zebra"};
    for(int i=0;i<8;i++){assert(!strcmp(favorites[i].title,sorted[i]));}
    assert(strstr(favorites[0].uri,"User-Agent=Keep%20Me"));
    snprintf(title,sizeof(title),"Antena 1");snprintf(uri,sizeof(uri),"https://source-d.example/antena");favorite_add();assert(fav_count==9&&!strcmp(favorites[2].title,"Antena 1"));
    favorites_open(3);assert(!strcmp(items[3].title,"beta"));favorite_confirm(3);activate(1);assert(fav_count==8);for(int i=0;i<fav_count;i++)assert(strcmp(favorites[i].uri,"https://source-b.example/b"));
    fav_count=0;favorites_load();assert(fav_count==8&&!strcmp(favorites[0].title,"Água")&&!strcmp(favorites[7].title,"zebra"));
    // Equal names with different sources stay separate; repeat-add remains a no-op.
    snprintf(title,sizeof(title),"alfa");snprintf(uri,sizeof(uri),"https://source-z.example/a");favorite_add();assert(fav_count==9&&!strcmp(favorites[1].title,"alfa")&&!strcmp(favorites[2].title,"alfa"));favorite_add();assert(fav_count==9);
    assert(mkdir(newpath,0700)==0);snprintf(title,sizeof(title),"AAA");snprintf(uri,sizeof(uri),"https://source-e.example/failure");favorite_add();assert(fav_count==9&&!strcmp(favorites[0].title,"Água"));assert(rmdir(newpath)==0);
    fav_count=0;favorites_load();assert(fav_count==9&&!strcmp(favorites[0].title,"Água"));
    // Decomposed UTF-8 and truncated sequences are deterministic and safe.
    const unsigned char*fold=(const unsigned char*)"A\xcc\x81";assert(favorite_sort_char(&fold)=='a');assert(favorite_sort_char(&fold)==0);
    const unsigned char*bad=(const unsigned char*)"\xf0";assert(favorite_sort_char(&bad)==0xf0);assert(favorite_sort_char(&bad)==0);
    // Legacy settings remain valid; language survives a fresh process and UI restart.
    char settings[4096];path_join(settings,sizeof(settings),config,"settings.txt");
    file=fopen(settings,"w");assert(file);fputs("0 40 75\n",file);fclose(file);language=1;settings_load();assert(language==1&&font_size==40&&volume==75);
    file=fopen(settings,"w");assert(file);fputs("0 36 65 2\n",file);fclose(file);language=1;settings_load();assert(language==1&&image_mode==2);
    language=0;settings_save();language=1;settings_load();assert(language==0);
    page=0;selected=4;options_open();assert(nav_count()==10);selected=8;button(0);assert(language==1&&page==5&&selected==8);
    assert(!strcmp(tr("Favoritos"),"Favorites"));assert(!strcmp(tr("Ver do Início"),"Play from Beginning"));
    assert(!strcmp(tr("My film.srt"),"My film.srt"));notify(tr("Favorito removido."));assert(!strcmp(note,"Favorite removed."));
    language=0;settings_load();assert(language==1&&font_size==36&&volume==65&&image_mode==2);
    selected=9;button(0);assert(page==0&&selected==4);options_open();
    click(100,130+8*52+10);assert(language==0&&page==5);assert(!strcmp(tr("Favoritos"),"Favoritos"));
    navigate(0,1);assert(selected==9);button(0);assert(page==0&&selected==4);
    // Same toggle works in playback Options and does not alter the source selection.
    playing=1;page=1;selected=3;options_open();selected=8;button(0);assert(language==1&&playing&&page==5&&option_page==1&&option_selected==3);selected=9;button(0);assert(page==1&&selected==3&&playing);playing=0;
    language=0;settings_load();assert(language==1);classify_mpv_warning("HTTP 403");assert(strstr(playback_error,"server refused"));
    file=fopen(settings,"w");assert(file);fputs("0 36 65 1 99\n",file);fclose(file);settings_load();assert(language==1);
    unlink(settings);language=0;settings_load();assert(language==1);
    unlink(favpath);unlink(movie);rmdir(films);rmdir(config);rmdir(temp);
    printf("Playlist/headers HTTP, HLS, URLs, teclado, SELECT, volume, sessao e favoritos, confirmacao de saida, L/R e retoma/migracao e idiomas PT/EN e favoritos A-Z: PASS\n");return 0;
}

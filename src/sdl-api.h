/* SPDX-License-Identifier: GPL-3.0-or-later */
/* SDL ABI helper inherited from the RG DS+ ScummVM package. */
#ifndef RGDS_SDL_API_H
#define RGDS_SDL_API_H
#include <stdint.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_Surface SDL_Surface;
typedef struct {int x,y,w,h;} SDL_Rect;
typedef struct {uint8_t r,g,b,a;} SDL_Color;
typedef union {uint32_t type; uint64_t align; unsigned char bytes[64];} SDL_Event;
#define RGBA32 0x16762004u
#define FULLSCREEN_DESKTOP 0x1001u
#define API(ret,name,args) static ret (*p##name) args
API(int,SDL_Init,(uint32_t)); API(void,SDL_Quit,(void)); API(const char*,SDL_GetError,(void));
API(int,SDL_GetNumVideoDisplays,(void));API(int,SDL_GetDisplayBounds,(int,SDL_Rect*));
API(SDL_Window*,SDL_CreateWindow,(const char*,int,int,int,int,uint32_t));API(void,SDL_DestroyWindow,(SDL_Window*));
API(int,SDL_SetWindowFullscreen,(SDL_Window*,uint32_t));API(void,SDL_RaiseWindow,(SDL_Window*));API(void,SDL_ShowWindow,(SDL_Window*));
API(uint32_t,SDL_GetWindowID,(SDL_Window*));API(int,SDL_GetWindowDisplayIndex,(SDL_Window*));
API(SDL_Renderer*,SDL_CreateRenderer,(SDL_Window*,int,uint32_t));API(void,SDL_DestroyRenderer,(SDL_Renderer*));
API(SDL_Window*,SDL_RenderGetWindow,(SDL_Renderer*));API(int,SDL_GetRendererOutputSize,(SDL_Renderer*,int*,int*));
API(int,SDL_SetRenderDrawColor,(SDL_Renderer*,uint8_t,uint8_t,uint8_t,uint8_t));API(int,SDL_RenderClear,(SDL_Renderer*));
API(int,SDL_RenderFillRect,(SDL_Renderer*,const SDL_Rect*));API(int,SDL_RenderCopy,(SDL_Renderer*,SDL_Texture*,const SDL_Rect*,const SDL_Rect*));
API(void,SDL_RenderPresent,(SDL_Renderer*));API(int,SDL_RenderReadPixels,(SDL_Renderer*,const SDL_Rect*,uint32_t,void*,int));
API(SDL_Texture*,SDL_CreateTexture,(SDL_Renderer*,uint32_t,int,int,int));API(int,SDL_UpdateTexture,(SDL_Texture*,const SDL_Rect*,const void*,int));
API(SDL_Texture*,SDL_CreateTextureFromSurface,(SDL_Renderer*,SDL_Surface*));API(void,SDL_DestroyTexture,(SDL_Texture*));API(void,SDL_FreeSurface,(SDL_Surface*));
API(int,SDL_QueryTexture,(SDL_Texture*,uint32_t*,int*,int*,int*));API(int,SDL_PollEvent,(SDL_Event*));
API(uint32_t,SDL_GetTicks,(void));API(void,SDL_Delay,(uint32_t));API(void*,SDL_JoystickOpen,(int));
API(void*,SDL_GL_GetCurrentContext,(void));API(void*,SDL_GL_GetProcAddress,(const char*));
API(int,SDL_GL_MakeCurrent,(SDL_Window*,void*));API(void,SDL_GL_GetDrawableSize,(SDL_Window*,int*,int*));API(void,SDL_GL_SwapWindow,(SDL_Window*));
static int api_load(void *lib,int all) {
#define LOAD(name) do { *(void**)(&p##name)=dlsym(lib,#name); if(!p##name){fprintf(stderr,"Missing SDL symbol: %s\n",#name);return 0;} } while(0)
 LOAD(SDL_Init);LOAD(SDL_Quit);LOAD(SDL_GetError);LOAD(SDL_GetNumVideoDisplays);LOAD(SDL_GetDisplayBounds);
 LOAD(SDL_CreateWindow);LOAD(SDL_DestroyWindow);LOAD(SDL_SetWindowFullscreen);LOAD(SDL_RaiseWindow);LOAD(SDL_ShowWindow);
 LOAD(SDL_GetWindowID);LOAD(SDL_GetWindowDisplayIndex);LOAD(SDL_CreateRenderer);LOAD(SDL_DestroyRenderer);LOAD(SDL_RenderGetWindow);LOAD(SDL_GetRendererOutputSize);
 LOAD(SDL_SetRenderDrawColor);LOAD(SDL_RenderClear);LOAD(SDL_RenderFillRect);LOAD(SDL_RenderCopy);LOAD(SDL_RenderPresent);LOAD(SDL_RenderReadPixels);
 LOAD(SDL_CreateTexture);LOAD(SDL_UpdateTexture);LOAD(SDL_CreateTextureFromSurface);LOAD(SDL_DestroyTexture);LOAD(SDL_FreeSurface);LOAD(SDL_QueryTexture);
 LOAD(SDL_PollEvent);LOAD(SDL_GetTicks);LOAD(SDL_Delay);LOAD(SDL_JoystickOpen);
 if(all){LOAD(SDL_GL_GetCurrentContext);LOAD(SDL_GL_GetProcAddress);LOAD(SDL_GL_MakeCurrent);LOAD(SDL_GL_GetDrawableSize);LOAD(SDL_GL_SwapWindow);}
#undef LOAD
 return 1;
}
#endif

#include "font.h"
#include "SDL_gpu.h"

const font_s font_load(const char* filename, unsigned rows, unsigned columns, SDL_Color color) {
    
    SDL_Surface* surf = GPU_LoadSurface(filename);
    if (surf == NULL || surf->format->BytesPerPixel < 3) return (font_s){NULL, 0, 0};

    SDL_LockSurface(surf);

    for (unsigned y = 0; y < surf->h; y++)
        for (unsigned x = 0; x < surf->w; x++) {
            // data + (y*width + sizeof(uint32_t)*x) 
            void* pixel = surf->pixels + (y*surf->pitch + 4*x);

            SDL_Color pcol;
            SDL_GetRGBA(*(Uint32*)pixel, surf->format, &pcol.r, &pcol.g, &pcol.b, &pcol.a);

            *(Uint32*)pixel = SDL_MapRGBA(surf->format, color.r, color.g, color.b, pcol.a);

        }

    SDL_UnlockSurface(surf);

    GPU_Image* img = GPU_CopyImageFromSurface(surf);    
    SDL_FreeSurface(surf);

    return (font_s){img, rows, columns, img->w/columns, img->h/rows};

}

void font_destroy(font_s font) {
    GPU_FreeImage(font.img);
}

//void font_draw_char(const font_t font, char c);

void font_draw_string(GPU_Target* target, int x, int y, float scale, const font_s font, const char* str, const unsigned (*cindex)(const char)) {
    if (font.img == NULL || target == NULL || str == NULL) return;

    for (const char* c = str; *c != '\0'; c++) {

        const unsigned pos = cindex(*c);

        unsigned posx = (pos % font.cols) * font.char_w;
        unsigned posy = (pos / font.cols) * font.char_h;

        GPU_Rect src_rect = GPU_MakeRect(posx, posy, font.char_w, font.char_h);
        GPU_Rect dst_rect = GPU_MakeRect((float)(x+font.char_w*(c-str)*scale), (float)y, font.char_w*scale, font.char_h*scale);

        //GPU_BlitScale(font.img, &src_rect, target, x+char_width*(c-str)*scale, y, scale, scale);
        GPU_BlitRect(font.img, &src_rect, target, &dst_rect);

    }
    
}

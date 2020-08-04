#include "plot.h"
#include "parser.h" // compute
#include "renderer.h" // camera macros
#include "console.h" // settings
#include "error.h"
#include <math.h> // isnormal

int pointf_compare(const pointf* p1, const pointf* p2) {
    return (p1->x < p2->x ? -1 : p1->x > p2->x ? 1 : 1);
}

void graph(double start, double end, unsigned numsteps, set_s* dst) {
    if (dst == NULL || numsteps < 2) return;

    if (numsteps > SET_MAXLENGTH) numsteps = SET_MAXLENGTH;

    dst->length = numsteps;

    double step = (end-start)/(numsteps-1);
    size_t i = 0;
    for (double x = start; i < numsteps; x += step, i++) {
        dst->coords[i].x = x;
        compute(&dst->coords[i].y, dst->formula, &x);
    }

}

void plot(GPU_Target* target, set_s* s) {
    if (s == NULL || !s->shown || s->length < 2) return;

    GPU_SetLineThickness((float)s->linewidth);

    pointi curr, last; // save the last point so we dont have to calculate the same point again
    last = WORLD2CAMCART(s->coords[0]);
    for (size_t i = 1; i < s->length; i++) {
        
        if (isnan(s->coords[i].y))
            continue;

        curr = WORLD2CAMCART(s->coords[i]);

        // Draw the point
        if (s->linewidth == 1)
            GPU_Pixel(target, curr.x, curr.y, s->col_line);
        else
            GPU_CircleFilled(target, curr.x, curr.y, (float)s->linewidth/2.0f, s->col_line);

        if (s->plot_type != PT_FUNCTION)
            GPU_Line(target, curr.x, curr.y, last.x, last.y, s->col_line);
    
        last = curr;
    }
}

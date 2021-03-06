/*
 * state.cc
 *
 * track PDF states
 *
 * Copyright (C) 2012,2013 Lu Wang <coolwanglu@gmail.com>
 */

#include <cmath>
#include <algorithm>

#include "HTMLRenderer.h"
#include "TextLineBuffer.h"
#include "util/namespace.h"
#include "util/math.h"

namespace pdf2htmlEX {

using std::max;
using std::abs;

void HTMLRenderer::updateAll(GfxState * state) 
{ 
    all_changed = true; 
    updateTextPos(state);
}
void HTMLRenderer::updateRise(GfxState * state)
{
    rise_changed = true;
}
void HTMLRenderer::updateTextPos(GfxState * state) 
{
    text_pos_changed = true;
    cur_tx = state->getLineX(); 
    cur_ty = state->getLineY(); 
}
void HTMLRenderer::updateTextShift(GfxState * state, double shift) 
{
    text_pos_changed = true;
    cur_tx -= shift * 0.001 * state->getFontSize() * state->getHorizScaling(); 
}
void HTMLRenderer::updateFont(GfxState * state) 
{
    font_changed = true; 
}
void HTMLRenderer::updateCTM(GfxState * state, double m11, double m12, double m21, double m22, double m31, double m32) 
{
    ctm_changed = true; 
}
void HTMLRenderer::updateTextMat(GfxState * state) 
{
    text_mat_changed = true; 
}
void HTMLRenderer::updateHorizScaling(GfxState * state)
{
    hori_scale_changed = true;
}
void HTMLRenderer::updateCharSpace(GfxState * state)
{
    letter_space_changed = true;
}
void HTMLRenderer::updateWordSpace(GfxState * state)
{
    word_space_changed = true;
}
void HTMLRenderer::updateRender(GfxState * state) 
{
    // currently Render is traced for color only
    // might need something like render_changed later
    fill_color_changed = true; 
    stroke_color_changed = true; 
}
void HTMLRenderer::updateFillColorSpace(GfxState * state) 
{
    fill_color_changed = true; 
}
void HTMLRenderer::updateStrokeColorSpace(GfxState * state) 
{
    stroke_color_changed = true; 
}
void HTMLRenderer::updateFillColor(GfxState * state) 
{
    fill_color_changed = true; 
}
void HTMLRenderer::updateStrokeColor(GfxState * state) 
{
    stroke_color_changed = true; 
}
void HTMLRenderer::reset_state()
{
    draw_text_scale = 1.0;

    cur_font_size = 0.0;
    
    memcpy(cur_text_tm, ID_MATRIX, sizeof(cur_text_tm));

    // reset html_state
    cur_html_state.font_info = install_font(nullptr);
    cur_html_state.font_size = 0;
    cur_html_state.fill_color.transparent = true;
    cur_html_state.stroke_color.transparent = true;
    cur_html_state.letter_space = 0;
    cur_html_state.word_space = 0;
    cur_html_state.rise = 0;
    cur_html_state.x = 0;
    cur_html_state.y = 0;
    memcpy(cur_html_state.transform_matrix, ID_MATRIX, sizeof(cur_html_state.transform_matrix));

    cur_tx  = cur_ty  = 0;
    draw_tx = draw_ty = 0;

    reset_state_change();
    all_changed = true;
}
void HTMLRenderer::reset_state_change()
{
    all_changed = false;

    rise_changed = false;
    text_pos_changed = false;

    font_changed = false;
    ctm_changed = false;
    text_mat_changed = false;
    hori_scale_changed = false;

    letter_space_changed = false;
    word_space_changed = false;

    fill_color_changed = false;
    stroke_color_changed = false;
}
void HTMLRenderer::check_state_change(GfxState * state)
{
    // DEPENDENCY WARNING
    // don't adjust the order of state checking 
    
    new_line_state = NLS_NONE;

    bool need_recheck_position = false;
    bool need_rescale_font = false;
    bool draw_text_scale_changed = false;

    // text position
    // we've been tracking the text position positively in the update*** functions
    if(all_changed || text_pos_changed)
    {
        need_recheck_position = true;
    }

    // font name & size
    if(all_changed || font_changed)
    {
        const FontInfo * new_font_info = install_font(state->getFont());

        if(!(new_font_info->id == cur_html_state.font_info->id))
        {
            // The width of the type 3 font text, if shown, is likely to be wrong
            // So we will create separate (absolute positioned) blocks for them, such that it won't affect other text
            // TODO: consider the font matrix and estimate the metrics  
            if(new_font_info->is_type3 || cur_html_state.font_info->is_type3)
            {
                new_line_state = max<NewLineState>(new_line_state, NLS_DIV);
            }
            else
            {
                new_line_state = max<NewLineState>(new_line_state, NLS_SPAN);
            }
            cur_html_state.font_info = new_font_info;
        }

        double new_font_size = state->getFontSize();
        if(!equal(cur_font_size, new_font_size))
        {
            need_rescale_font = true;
            cur_font_size = new_font_size;
        }
    }  

    // backup the current ctm for need_recheck_position
    double old_tm[6];
    memcpy(old_tm, cur_text_tm, sizeof(old_tm));

    // ctm & text ctm & hori scale
    if(all_changed || ctm_changed || text_mat_changed || hori_scale_changed)
    {
        double new_text_tm[6];

        const double * m1 = state->getCTM();
        const double * m2 = state->getTextMat();
        double hori_scale = state->getHorizScaling();

        new_text_tm[0] = (m1[0] * m2[0] + m1[2] * m2[1]) * hori_scale;
        new_text_tm[1] = (m1[1] * m2[0] + m1[3] * m2[1]) * hori_scale;
        new_text_tm[2] = m1[0] * m2[2] + m1[2] * m2[3];
        new_text_tm[3] = m1[1] * m2[2] + m1[3] * m2[3];
        new_text_tm[4] = m1[0] * m2[4] + m1[2] * m2[5] + m1[4]; 
        new_text_tm[5] = m1[1] * m2[4] + m1[3] * m2[5] + m1[5];
        //new_text_tm[4] = new_text_tm[5] = 0;

        if(!tm_equal(new_text_tm, cur_text_tm))
        {
            need_recheck_position = true;
            need_rescale_font = true;
            memcpy(cur_text_tm, new_text_tm, sizeof(cur_text_tm));
        }
    }

    // draw_text_tm, draw_text_scale
    // depends: font size & ctm & text_ctm & hori scale
    if(need_rescale_font)
    {
        /*
         * Rescale the font
         * If the font-size is 1, and the matrix is [10,0,0,10,0,0], we would like to change it to
         * font-size == 10 and matrix == [1,0,0,1,0,0], 
         * such that it will be easy and natrual for web browsers
         */
        double new_draw_text_tm[6];
        memcpy(new_draw_text_tm, cur_text_tm, sizeof(new_draw_text_tm));

        // see how the tm (together with text_scale_factor2) would change the vector (0,1)
        double new_draw_text_scale = 1.0/text_scale_factor2 * hypot(new_draw_text_tm[2], new_draw_text_tm[3]);

        double new_draw_font_size = cur_font_size;
        if(is_positive(new_draw_text_scale))
        {
            // scale both font size and matrix 
            new_draw_font_size *= new_draw_text_scale;
            for(int i = 0; i < 4; ++i)
                new_draw_text_tm[i] /= new_draw_text_scale;
        }
        else
        {
            new_draw_text_scale = 1.0;
        }

        if(!is_positive(new_draw_font_size))
        {
            // CSS cannot handle flipped pages
            new_draw_font_size *= -1;

            for(int i = 0; i < 4; ++i)
                new_draw_text_tm[i] *= -1;
        }

        if(!(equal(new_draw_text_scale, draw_text_scale)))
        {
            draw_text_scale_changed = true;
            draw_text_scale = new_draw_text_scale;
        }

        if(!equal(new_draw_font_size, cur_html_state.font_size))
        {
            new_line_state = max<NewLineState>(new_line_state, NLS_SPAN);
            cur_html_state.font_size = new_draw_font_size;
        }

        if(!tm_equal(new_draw_text_tm, cur_html_state.transform_matrix, 4))
        {
            new_line_state = max<NewLineState>(new_line_state, NLS_DIV);
            memcpy(cur_html_state.transform_matrix, new_draw_text_tm, sizeof(cur_html_state.transform_matrix));
        }
    }

    // see if the new line is compatible with the current line with proper position shift
    // don't bother doing the heavy job when (new_line_state == NLS_DIV)
    // depends: rise & text position & transformation
    if(need_recheck_position && (new_line_state < NLS_DIV))
    {
        // try to transform the old origin under the new TM
        /*
         * CurTM * (cur_tx, cur_ty, 1)^T = OldTM * (draw_tx + dx, draw_ty + dy, 1)^T
         *
         * the first 4 elements of CurTM and OldTM should be the same
         * otherwise the following text cannot be parallel
         *
         * CurTM[4] - OldTM[4] = OldTM[0] * (draw_tx + dx - cur_tx) + OldTM[2] * (draw_ty + dy - cur_ty)
         * CurTM[5] - OldTM[5] = OldTM[1] * (draw_tx + dx - cur_tx) + OldTM[3] * (draw_ty + dy - cur_ty)
         *
         * For horizontal text, set dy = 0, and try to solve dx
         * If dx can be solved, we can simply append a x-offset without creating a new line
         *
         * TODO, writing mode, set dx = 0 and solve dy
         * TODO, try to merge when cur_tm and old_tm are proportional
         */

        bool merged = false;
        double dx = 0;
        if(tm_equal(old_tm, cur_text_tm, 4))
        {
            double lhs1 = cur_text_tm[4] - old_tm[4] - old_tm[2] * (draw_ty - cur_ty) - old_tm[0] * (draw_tx - cur_tx);
            double lhs2 = cur_text_tm[5] - old_tm[5] - old_tm[3] * (draw_ty - cur_ty) - old_tm[1] * (draw_tx - cur_tx);

            if(equal(old_tm[0] * lhs2, old_tm[1] * lhs1))
            {
                if(!equal(old_tm[0], 0))
                {
                    dx = lhs1 / old_tm[0];
                    merged = true;
                }
                else if (!equal(old_tm[1], 0))
                {
                    dx = lhs2 / old_tm[1];
                    merged = true;
                }
                else
                {
                    if((equal(lhs1,0)) && (equal(lhs2,0)))
                    {
                        // free
                        dx = 0;
                        merged = true;
                    }
                    // else fail
                }
            }
            //else no solution
        }
        // else force new line

        if(merged)
        {
            text_line_buf->append_offset(dx * draw_text_scale);
            draw_tx = cur_tx;
            draw_ty = cur_ty;
        }
        else
        {
            new_line_state = max<NewLineState>(new_line_state, NLS_DIV);
        }
    }

    // letter space
    // depends: draw_text_scale
    if(all_changed || letter_space_changed || draw_text_scale_changed)
    {
        double new_letter_space = state->getCharSpace() * draw_text_scale;
        if(!equal(new_letter_space, cur_html_state.letter_space))
        {
            cur_html_state.letter_space = new_letter_space;
            new_line_state = max<NewLineState>(new_line_state, NLS_SPAN);
        }
    }

    // word space
    // depends draw_text_scale
    if(all_changed || word_space_changed || draw_text_scale_changed)
    {
        double new_word_space = state->getWordSpace() * draw_text_scale;
        if(!equal(new_word_space, cur_html_state.word_space))
        {
            cur_html_state.word_space = new_word_space;
            new_line_state = max<NewLineState>(new_line_state, NLS_SPAN);
        }
    }

    // fill color
    if((!(param->fallback)) && (all_changed || fill_color_changed))
    {
        // * PDF Spec. Table 106 –Text rendering modes
        static const char FILL[8] = { true, false, true, false, true, false, true, false };
        
        int idx = state->getRender();
        assert((idx >= 0) && (idx < 8));
        Color new_fill_color;
        if(FILL[idx])
        {
            new_fill_color.transparent = false;
            state->getFillRGB(&new_fill_color.rgb);
        }
        else
        {
            new_fill_color.transparent = true;
        }
        if(!(new_fill_color == cur_html_state.fill_color))
        {
            cur_html_state.fill_color = new_fill_color;
            new_line_state = max<NewLineState>(new_line_state, NLS_SPAN);
        }
    }

    // stroke color
    if((!(param->fallback)) && (all_changed || stroke_color_changed))
    {
        // * PDF Spec. Table 106 –  Text rendering modes
        static const char STROKE[8] = { false, true, true, false, false, true, true, false };
        
        int idx = state->getRender();
        assert((idx >= 0) && (idx < 8));
        Color new_stroke_color;
        // stroke
        if(STROKE[idx])
        {
            new_stroke_color.transparent = false;
            state->getStrokeRGB(&new_stroke_color.rgb);
        }
        else
        {
            new_stroke_color.transparent = true;
        }
        if(!(new_stroke_color == cur_html_state.stroke_color))
        {
            cur_html_state.stroke_color = new_stroke_color;
            new_line_state = max<NewLineState>(new_line_state, NLS_SPAN);
        }
    }

    // rise
    // depends draw_text_scale
    if(all_changed || rise_changed || draw_text_scale_changed)
    {
        double new_rise = state->getRise() * draw_text_scale;
        if(!equal(new_rise, cur_html_state.rise))
        {
            cur_html_state.rise = new_rise;
            new_line_state = max<NewLineState>(new_line_state, NLS_SPAN);
        }
    }

    reset_state_change();
}

void HTMLRenderer::prepare_text_line(GfxState * state)
{
    if(!line_opened)
    {
        new_line_state = NLS_DIV;
    }
    
    if(new_line_state == NLS_DIV)
    {
        close_text_line();

        // update position such that they will be recorded by text_line_buf
        state->transform(state->getCurX(), state->getCurY(), &cur_html_state.x, &cur_html_state.y);

        //resync position
        draw_ty = cur_ty;
        draw_tx = cur_tx;
    }
    else
    {
        // align horizontal position
        // try to merge with the last line if possible
        double target = (cur_tx - draw_tx) * draw_text_scale;
        if(!equal(target, 0))
        {
            text_line_buf->append_offset(target);
            draw_tx += target / draw_text_scale;
        }
    }

    if(new_line_state != NLS_NONE)
    {
        text_line_buf->append_state(cur_html_state);
    }

    line_opened = true;
}

void HTMLRenderer::close_text_line()
{
    if(line_opened)
    {
        line_opened = false;
        text_line_buf->flush();
    }
}

} //namespace pdf2htmlEX

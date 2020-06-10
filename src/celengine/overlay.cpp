// overlay.cpp
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <cstring>
#include <cstdarg>
#include <iostream>
#include "glsupport.h"
#include <Eigen/Core>
#include <celutil/color.h>
#include <celutil/debug.h>
#include <celutil/utf8.h>
#include <celmath/geomutil.h>
#include "vecgl.h"
#include "overlay.h"
#include "rectangle.h"
#include "render.h"
#include "texture.h"
#if NO_TTF
#include <celtxf/texturefont.h>
#else
#include <celttf/truetypefont.h>
#endif

using namespace std;
using namespace Eigen;
using namespace celmath;

Overlay::Overlay(Renderer& r) :
    ostream(&sbuf),
    renderer(r)
{
    sbuf.setOverlay(this);
}

void Overlay::begin()
{
    mvp = Ortho2D(0.0f, (float)windowWidth, 0.0f, (float)windowHeight);
    // ModelView is Identity

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    global.reset();
    useTexture = false;
}

void Overlay::end()
{
}


void Overlay::setWindowSize(int w, int h)
{
    windowWidth = w;
    windowHeight = h;
}

void Overlay::setFont(TextureFont* f)
{
    if (f != font)
    {
        if (font != nullptr)
            font->flush();
        font = f;
        fontChanged = true;
    }
}


void Overlay::beginText()
{
    savePos();
    textBlock++;
    if (font != nullptr)
    {
        font->bind();
        font->setMVPMatrix(mvp);
        useTexture = true;
        fontChanged = false;
    }
}

void Overlay::endText()
{
    if (textBlock > 0)
    {
        textBlock--;
        restorePos();
    }
    font->unbind();
}


void Overlay::print(wchar_t c)
{
    if (font != nullptr)
    {
        if (!useTexture || fontChanged)
        {
            font->bind();
            font->setMVPMatrix(mvp);
            useTexture = true;
            fontChanged = false;
        }

        switch (c)
        {
        case '\n':
            if (textBlock > 0)
            {
                restorePos();
                global.y -= 1.0f + font->getHeight();
                savePos();
            }
            break;
        default:
            font->render(c, global.x + xoffset, global.y + yoffset);
            xoffset += font->getAdvance(c);
            break;
        }
    }
}


void Overlay::print(char c)
{
    if (font != nullptr)
    {
        if (!useTexture || fontChanged)
        {
            font->bind();
            font->setMVPMatrix(mvp);
            useTexture = true;
            fontChanged = false;
        }

        switch (c)
        {
        case '\n':
            if (textBlock > 0)
            {
                restorePos();
                global.y -= 1.0f + font->getHeight();
                savePos();
            }
            break;
        default:
            font->render(c, global.x + xoffset, global.y + yoffset);
            xoffset += font->getAdvance(c);
            break;
        }
    }
}

void Overlay::print(const char* s)
{
    int length = strlen(s);
    bool validChar = true;
    int i = 0;
    while (i < length && validChar)
    {
        wchar_t ch = 0;
        validChar = UTF8Decode(s, i, length, ch);
        i += UTF8EncodedSize(ch);
        print(ch);
    }
}

void Overlay::drawRectangle(const Rect& r)
{
    if (useTexture && r.tex == nullptr)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        useTexture = false;
    }

    renderer.drawRectangle(r, mvp);
}

void Overlay::setColor(float r, float g, float b, float a)
{
    if (font != nullptr)
        font->flush();
    glVertexAttrib4f(CelestiaGLProgram::ColorAttributeIndex, r, g, b, a);
}

void Overlay::setColor(const Color& c)
{
    if (font != nullptr)
        font->flush();
    glVertexAttrib4f(CelestiaGLProgram::ColorAttributeIndex,
                     c.red(), c.green(), c.blue(), c.alpha());
}


void Overlay::moveBy(float dx, float dy)
{
    global.x += dx;
    global.y += dy;
}

void Overlay::savePos()
{
    posStack.push_back(global);
}

void Overlay::restorePos()
{
    if (!posStack.empty())
    {
        global = posStack.back();
        posStack.pop_back();
    }
    xoffset = yoffset = 0.0f;
}

//
// OverlayStreamBuf implementation
//
OverlayStreamBuf::OverlayStreamBuf()
{
    setbuf(nullptr, 0);
};


void OverlayStreamBuf::setOverlay(Overlay* o)
{
    overlay = o;
}


int OverlayStreamBuf::overflow(int c)
{
    if (overlay != nullptr)
    {
        switch (decodeState)
        {
        case UTF8DecodeStart:
            if (c < 0x80)
            {
                // Just a normal 7-bit character
                overlay->print((char) c);
            }
            else
            {
                unsigned int count;

                if ((c & 0xe0) == 0xc0)
                    count = 2;
                else if ((c & 0xf0) == 0xe0)
                    count = 3;
                else if ((c & 0xf8) == 0xf0)
                    count = 4;
                else if ((c & 0xfc) == 0xf8)
                    count = 5;
                else if ((c & 0xfe) == 0xfc)
                    count = 6;
                else
                    count = 1; // Invalid byte

                if (count > 1)
                {
                    unsigned int mask = (1 << (7 - count)) - 1;
                    decodeShift = (count - 1) * 6;
                    decodedChar = (c & mask) << decodeShift;
                    decodeState = UTF8DecodeMultibyte;
                }
                else
                {
                    // If the character isn't valid multibyte sequence head,
                    // silently skip it by leaving the decoder state alone.
                }
            }
            break;

        case UTF8DecodeMultibyte:
            if ((c & 0xc0) == 0x80)
            {
                // We have a valid non-head byte in the sequence
                decodeShift -= 6;
                decodedChar |= (c & 0x3f) << decodeShift;
                if (decodeShift == 0)
                {
                    overlay->print(decodedChar);
                    decodeState = UTF8DecodeStart;
                }
            }
            else
            {
                // Bad byte in UTF-8 encoded sequence; we'll silently ignore
                // it and reset the state of the UTF-8 decoder.
                decodeState = UTF8DecodeStart;
            }
            break;
        }
    }

    return c;
}

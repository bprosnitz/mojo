/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SKY_ENGINE_CORE_CSS_CSSBORDERIMAGE_H_
#define SKY_ENGINE_CORE_CSS_CSSBORDERIMAGE_H_

#include "sky/engine/core/css/CSSBorderImageSliceValue.h"
#include "sky/engine/core/css/CSSValueList.h"
#include "sky/engine/wtf/PassRefPtr.h"

namespace blink {

PassRefPtr<CSSValueList> createBorderImageValue(PassRefPtr<CSSValue> image, PassRefPtr<CSSValue> imageSlice, PassRefPtr<CSSValue> borderSlice,
    PassRefPtr<CSSValue> outset, PassRefPtr<CSSValue> repeat);

} // namespace blink

#endif  // SKY_ENGINE_CORE_CSS_CSSBORDERIMAGE_H_

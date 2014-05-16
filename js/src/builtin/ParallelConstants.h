/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_ParallelConstants_h
#define builtin_ParallelConstants_h

#define JS_PIPELINE_OP_SHARED_CTOR_SLOT 0

#define JS_RANGE_OP_MIN_SLOT            1
#define JS_RANGE_OP_MAX_SLOT            2

#define JS_SHAPE_OP_DIM_SLOT            1

#define JS_MAPTO_OP_PREVOP_SLOT         1
#define JS_MAPTO_OP_FUNC_SLOT           2
#define JS_MAPTO_OP_GRAINTYPE_SLOT      3

#define JS_FILTER_OP_PREVOP_SLOT        1
#define JS_FILTER_OP_FUNC_SLOT          2

#define JS_PIPELINE_OP_SLOTS            4


#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_ParallelConstants_h
#define builtin_ParallelConstants_h

#define JS_PIPELINE_OP_SLOTS 3

#define JS_COMPREHENSION_OP_GRAINTYPE_SLOT 0
#define JS_COMPREHENSION_OP_SHAPE_SLOT     1
#define JS_COMPREHENSION_OP_FUNC_SLOT      2

#define JS_COMPREHENSION_STATE_GRAINTYPE_SLOT 0
#define JS_COMPREHENSION_STATE_SHAPE_SLOT     1
#define JS_COMPREHENSION_STATE_FUNC_SLOT      2

#define JS_MAPTO_OP_PREVOP_SLOT    0
#define JS_MAPTO_OP_GRAINTYPE_SLOT 1
#define JS_MAPTO_OP_FUNC_SLOT      2

#define JS_MAPTO_STATE_PREVOP_SLOT    0
#define JS_MAPTO_STATE_GRAINTYPE_SLOT 1
#define JS_MAPTO_STATE_FUNC_SLOT      2

#define JS_FILTER_OP_PREVOP_SLOT   0
#define JS_FILTER_OP_FUNC_SLOT     1

#define JS_FILTER_STATE_PREVOP_SLOT   0
#define JS_FILTER_STATE_FUNC_SLOT     1

#define JS_ARRAY_OP_GRAINTYPE_SLOT 0
#define JS_ARRAY_OP_SHAPE_SLOT     1
#define JS_ARRAY_OP_ARRAY_SLOT     2

#define JS_ARRAY_STATE_GRAINTYPE_SLOT 0
#define JS_ARRAY_STATE_SHAPE_SLOT     1
#define JS_ARRAY_STATE_ARRAY_SLOT     2

#endif

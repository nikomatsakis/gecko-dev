/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/Parallel.h"

#define JS_PIPELINE_OP_SLOTS 0

///////////////////////////////////////////////////////////////////////////
// Pipeline ops

const JSFunctionSpec PipelineOp::methods[] = {

    JS_SELF_HOSTED_FN("map",     "ParallelMap", 1, 0),
    JS_SELF_HOSTED_FN("mapTo",   "ParallelMapTo", 2, 0),
    JS_SELF_HOSTED_FN("filter",  "ParallelFilter", 1, 0),

    JS_SELF_HOSTED_FN("collect", "ParallelCollect", 0, 0),
    JS_SELF_HOSTED_FN("reduce",  "ParallelReduce", 1, 0),

    JS_FS_END

};

///////////////////////////////////////////////////////////////////////////
// Comprehensions

const Class ComprehensionOp::class_ = {
    "ComprehensionOp",
    JSCLASS_HAS_RESERVED_SLOTS(JS_PIPELINE_OP_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec ComprehensionOp::methods[] = {
    JS_FS_END
};

const Class ComprehensionState::class_ = {
    "ComprehensionState",
    JSCLASS_HAS_RESERVED_SLOTS(JS_PIPELINE_OP_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec ComprehensionState::methods[] = {
    JS_FS_END
};

bool
js::NewComprehensionOpState(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 3);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypeDescr>());
    JS_ASSERT(args[1].isObject());
    JS_ASSERT(args[2].isObject());

    Rooted<JSObject*> result(cx);

    Rooted<ComprehensionOp*> result(cx);
    result = NewObjectWithProto<ComprehensionOp>(cx, nullptr, nullptr);
    if (!result)
        return false;

    args.rval().setObject(*result);
    return true;
}

///////////////////////////////////////////////////////////////////////////
// MapTos

const Class MapToOp::class_ = {
    "MapToOp",
    JSCLASS_HAS_RESERVED_SLOTS(JS_PIPELINE_OP_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec MapToOp::methods[] = {
    JS_FS_END
};

const Class MapToState::class_ = {
    "MapToState",
    JSCLASS_HAS_RESERVED_SLOTS(JS_PIPELINE_OP_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec MapToState::methods[] = {
    JS_FS_END
};

///////////////////////////////////////////////////////////////////////////
// Filters

const Class FilterOp::class_ = {
    "FilterOp",
    JSCLASS_HAS_RESERVED_SLOTS(JS_PIPELINE_OP_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec FilterOp::methods[] = {
    JS_FS_END
};

const Class FilterState::class_ = {
    "FilterState",
    JSCLASS_HAS_RESERVED_SLOTS(JS_PIPELINE_OP_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec FilterState::methods[] = {
    JS_FS_END
};

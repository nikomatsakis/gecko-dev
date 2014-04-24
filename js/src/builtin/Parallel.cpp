/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/Parallel.h"
#include "builtin/ParallelConstants.h"

using namespace js;

///////////////////////////////////////////////////////////////////////////
// Comprehensions

const Class PipelineObject::class_ = {
    js_Object_str,
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

static bool
IsPipelineObject(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    args.rval().setBoolean(args[0].isObject() &&
                           args[0].toObject().is<PipelineObject>());
    return true;
}


bool
js::intrinsic_IsPipelineObject(JSContext *cx, unsigned argc, Value *vp)
{
    return IsPipelineObject(cx, argc, vp);
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::intrinsic_IsPipelineObjectJitInfo,
                                      IsPipelineObject,
                                      IsPipelineObject);

bool
js::intrinsic_NewPipelineObject(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());

    RootedObject proto(cx, &args[0].toObject());

    RootedObject result(cx);
    result = NewObjectWithProto<PipelineObject>(cx, proto, nullptr);
    if (!result)
        return false;

    args.rval().setObject(*result);
    return true;
}


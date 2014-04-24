/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Parallel_h
#define builtin_Parallel_h

namespace js {

class PipelineObject : public JSObject {
  public:
    static const Class class_;
    static const JSFunctionSpec methods[];
};

bool intrinsic_NewPipelineObject(JSContext *cx, unsigned argc, Value *vp);

bool intrinsic_IsPipelineObject(JSContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo intrinsic_IsPipelineObjectJitInfo;

} // namespace js

#endif

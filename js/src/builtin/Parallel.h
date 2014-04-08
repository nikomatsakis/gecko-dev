/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Parallel_h
#define builtin_Parallel_h

namespace js {
namespace parallel {

class PipelineOp : public JSObject {
  private:
    // Intentionally private and undefined.
    static const Class class_;

  public:
    static const JSFunctionSpec methods[];
};

class PipelineState : public JSObject {
  private:
    // Intentionally private and undefined.
    static const Class class_;

  public:
};

class ComprehensionOp : public PipelineOp {
  public:
    static const Class class_;
    static const JSFunctionSpec methods[];
};

bool NewComprehensionOpState(JSContext *cx, unsigned argc, Value *vp);

class ComprehensionState : public PipelineState {
  public:
    static const Class class_;
    static const JSFunctionSpec methods[];
};

bool NewComprehensionState(JSContext *cx, unsigned argc, Value *vp);

class MapToOp : public PipelineOp {
  public:
    static const Class class_;
    static const JSFunctionSpec methods[];
};

bool NewMapToOp(JSContext *cx, unsigned argc, Value *vp);

class MapToState : public PipelineState {
  public:
    static const Class class_;
    static const JSFunctionSpec methods[];
};

bool NewMapToState(JSContext *cx, unsigned argc, Value *vp);

class FilterOp : public PipelineOp {
  public:
    static const Class class_;
    static const JSFunctionSpec methods[];
};

bool NewFilterOpState(JSContext *cx, unsigned argc, Value *vp);

class FilterState : public PipelineState {
  public:
    static const Class class_;
    static const JSFunctionSpec methods[];
};

bool NewFilterState(JSContext *cx, unsigned argc, Value *vp);

} // namespace parallel
} // namespace js

#endif

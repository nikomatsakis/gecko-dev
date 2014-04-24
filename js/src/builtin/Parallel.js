#include "ParallelConstants.h"

function TypedArrayParallel() {
  var range = ParallelRange(0, this.length);
  return callFunction(ParallelMapTo, range, null, i => this[i]);
}

var PipelineOpProto = {
  map: ParallelMap,
  mapTo: ParallelMapTo,
  filter: ParallelFilter,

  reduce: ParallelReduce,
  collect: ParallelCollect,
};

function ParallelRange(min, max) {
  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_SLOT, _ParallelRangeState);
  UnsafeSetReservedSlot(obj, JS_RANGE_OP_MIN_SLOT, TO_INT32(min));
  UnsafeSetReservedSlot(obj, JS_RANGE_OP_MAX_SLOT, TO_INT32(max));
  return obj;
}

function ParallelShape(inDims) {
  var length = inDims.length;
  var dims = [];
  for (var i = 0; i < length; i++) {
    var d = TO_INT32(inDims[i]);
    ARRAY_PUSH(dims, d);
  }

  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_SLOT, _ParallelShapeState);
  UnsafeSetReservedSlot(obj, JS_SHAPE_OP_DIM_SLOT, dims);
  return obj;
}

function ParallelMap(func) {
}

function ParallelMapTo(grainType, func) {
  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_SLOT, _ParallelMapToState);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_GRAINTYPE_SLOT, grainType);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_FUNC_SLOT, func);
  return obj;
}

function ParallelFilter(func) {
  var obj = NewPipelineObject(ParallelFilterProto);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_FUNC_SLOT, func);
  return obj;
}

function ParallelReduce() {
  if (!IsPipelineObject(this))
    ThrowError();

  var state = _CreateState(this);
  var shape = state.computeShape();
  var grainType = state.grainType;

  var numElements = 1;
  for (var s of shape)
    numElements *= s;

  // Allocate a flat array.
  var FlatArrayType = state.grainType.array(numElements);

  var outOffset = 0;
  for (var i = 0; i < numElements; i++, outOffset += grainTypeSize) {
    var r = state.next(accums[1]);
  }
}

function ParallelCollect() {
  if (!IsPipelineObject(this))
    ThrowError();

  var state = _CreateState(this);
  var shape = state.computeShape();
  var grainType = state.grainType;
  var numElements = 1;
  for (var s of shape)
    numElements *= s;

  // Allocate a flat array to begin with
  var FlatArrayType = state.grainType.array(numElements); // FIXME
  var result = new FlatArrayType();

  var grainTypeIsComplex = !TypeDescrIsSimpleType(grainType);
  var grainTypeSize = grainType.byteLength; // FIXME

  var outOffset = 0;
  for (var i = 0; i < numElements; i++, outOffset += grainTypeSize) {
    result[i] = state.next();
  }

  global.print("Hi1");

  // Return, redimensioning if necessary
  if (shape.length == 1)
    return result;

  // FIXME
  var DeepArrayType = state.grainType.array.apply(state.grainType, shape);
  global.print(DeepArrayType.toSource());
  var x = result.redimension(DeepArrayType);
  global.print("Hi3");
  return x;
}

function _CreateState(op) {
  assert(IsPipelineObject(op), "_CreateState invoked with non-pipeline obj");
  var State = UnsafeGetReservedSlot(op, JS_PIPELINE_OP_STATE_SLOT);
  return new State(op);
}

function _ParallelRangeState(op) {
  var T = GetTypedObjectModule();
  this.op = op;
  this.index = 0;
  this.min = UnsafeGetReservedSlot(op, JS_RANGE_OP_MIN_SLOT);
  this.max = UnsafeGetReservedSlot(op, JS_RANGE_OP_MAX_SLOT);
  this.grainType = T.uint32;
}

MakeConstructible(_ParallelRangeState, {
  toString: function() {
    return "_ParallelRangeState(" + [this.index, this.min, this.max] + ")";
  },

  next: function() {
    return this.index++;
  },

  computeShape: function() {
    return [this.max - this.min];
  }
});

function _ParallelShapeState(op) {
  var T = GetTypedObjectModule();
  this.op = op;
  this.shape = UnsafeGetReservedSlot(op, JS_SHAPE_OP_DIM_SLOT);
  this.grainType = T.Object;

  this.indices = [];
  for (var i = 0; i < this.shape.length; i++) {
    ARRAY_PUSH(this.indices, 0);
  }
}

MakeConstructible(_ParallelShapeState, {
  toString: function() {
    return "_ParallelRangeState(" + [this.index, this.min, this.max] + ")";
  },

  next: function() {
    var output = [];
    for (var i = 0; i < this.shape.length; i++) {
      ARRAY_PUSH(output, this.indices[i]);
    }

    for (var i = this.shape.length - 1; i >= 0; i--) {
      if (++this.indices[i] < this.shape[i])
        break;
      this.indices[i] = 0;
    }

    return output;
  },

  computeShape: function() {
    return this.shape;
  }
});

function _ParallelMapToState(op) {
  var prevOp = UnsafeGetReservedSlot(op, JS_MAPTO_OP_PREVOP_SLOT);

  this.op = op;
  this.index = 0;
  this.prevState = _CreateState(prevOp);
  this.shape = this.prevState.shape;
  this.grainType = UnsafeGetReservedSlot(op, JS_MAPTO_OP_GRAINTYPE_SLOT);
  this.func = UnsafeGetReservedSlot(op, JS_MAPTO_OP_FUNC_SLOT);
}

MakeConstructible(_ParallelMapToState, {
  toString: function() {
    return "_ParallelMapToState(" + [this.index, this.shape,
                                     this.grainType.toSource()] + ")";
  },

  next: function() {
    var v = this.prevState.next(); // FIXME
    return this.func(v);
  },

  computeShape: function() {
    return this.prevState.computeShape();
  }
});

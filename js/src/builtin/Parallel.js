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
  var T = GetTypedObjectModule();

  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_CTOR_SLOT, _ParallelRangeState);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHAPE_FUNC_SLOT, _ParallelRangeShape);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_GRAINTYPE_SLOT, T.int32);
  UnsafeSetReservedSlot(obj, JS_RANGE_OP_MIN_SLOT, TO_INT32(min));
  UnsafeSetReservedSlot(obj, JS_RANGE_OP_MAX_SLOT, TO_INT32(max));
  return obj;
}

function ParallelShape(inDims) {
  var T = GetTypedObjectModule();

  var length = inDims.length;
  var dims = [];
  for (var i = 0; i < length; i++) {
    var d = TO_INT32(inDims[i]);
    ARRAY_PUSH(dims, d);
  }

  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_CTOR_SLOT, _ParallelShapeState);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHAPE_FUNC_SLOT, _ParallelShapeShape);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_GRAINTYPE_SLOT, T.Object);
  UnsafeSetReservedSlot(obj, JS_SHAPE_OP_DIM_SLOT, dims);
  return obj;
}

function ParallelMap(func) {
}

function ParallelMapTo(grainType, func) {
  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_CTOR_SLOT, _ParallelMapToState);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHAPE_FUNC_SLOT, _ParallelMapToShape);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_GRAINTYPE_SLOT, grainType);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_FUNC_SLOT, func);
  return obj;
}

function ParallelFilter(func) {
  var obj = NewPipelineObject(PipelineOpProto);
  var grainType = UnsafeGetReservedSlot(this, JS_PIPELINE_OP_GRAINTYPE_SLOT);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_CTOR_SLOT, _ParallelFilterState);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHAPE_FUNC_SLOT, _ParallelFilterShape);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_GRAINTYPE_SLOT, grainType);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_FUNC_SLOT, func);
  return obj;
}

function ParallelReduce() {
  return "NYI";
}

function ParallelCollect() {
  if (!IsPipelineObject(this))
    ThrowError();

  var [shape, extra] = _ComputeShape(this);

  var numElements = 1;
  for (var s of shape)
    numElements *= s;

  var grainType = UnsafeGetReservedSlot(this, JS_PIPELINE_OP_GRAINTYPE_SLOT);
  var grainTypeIsComplex = !TypeDescrIsSimpleType(grainType);
  var grainTypeSize = grainType.byteLength; // FIXME

  // Allocate a flat array to begin with
  var FlatArrayType = grainType.array(numElements); // FIXME
  var result = new FlatArrayType();

  // Divide into N pieces.
  var N = 4;
  var perProc = TO_INT32(numElements / N);
  var remainder = TO_INT32(numElements % N);
  for (var proc = 0; proc < N; proc++) {
    var state = _CreateState(this);

    var start = 0;
    for (var i = 0; i < proc; i++) {
      start += perProc;
      if (i < remainder)
        start += 1;
    }

    var end = start + perProc;
    if (proc < remainder)
      end += 1;

    state.init(start, end, extra);

    for (var i = start; i < end; i++) {
      result[i] = state.next(i);
    }
  }

  // Return, redimensioning if necessary
  if (shape.length == 1)
    return result;

  // FIXME
  var DeepArrayType = grainType.array.apply(grainType, shape);
  var x = result.redimension(DeepArrayType);
  return x;
}

function _CreateState(op, extra) {
  assert(IsPipelineObject(op), "_CreateState invoked with non-pipeline obj");
  var State = UnsafeGetReservedSlot(op, JS_PIPELINE_OP_STATE_CTOR_SLOT);
  return new State(op, extra);
}

function _ComputeShape(op) {
  assert(IsPipelineObject(op), "_ComputeShape invoked with non-pipeline obj");
  var shape = UnsafeGetReservedSlot(op, JS_PIPELINE_OP_SHAPE_FUNC_SLOT);
  return shape(op);
}

function _ParallelRangeState(op) {
  var T = GetTypedObjectModule();
  this.op = op;
}

MakeConstructible(_ParallelRangeState, {
  toString: function() {
    return "_ParallelRangeState(" + [this.index, this.min, this.max] + ")";
  },

  init: function(start, end) {
  },

  next: function(i) {
    return i;
  },
});

function _ParallelRangeShape(op) {
  var min = UnsafeGetReservedSlot(op, JS_RANGE_OP_MIN_SLOT);
  var max = UnsafeGetReservedSlot(op, JS_RANGE_OP_MAX_SLOT);
  return [[max - min], null];
}

function _ParallelShapeState(op) {
  var T = GetTypedObjectModule();
  this.op = op;
  this.shape = UnsafeGetReservedSlot(op, JS_SHAPE_OP_DIM_SLOT);
  this.indices = [];
  for (var i = 0; i < this.shape.length; i++) {
    ARRAY_PUSH(this.indices, 0);
  }
}

MakeConstructible(_ParallelShapeState, {
  toString: function() {
    return "_ParallelRangeState(" + [this.index, this.min, this.max] + ")";
  },

  init: function(start, end) {
    // Initialize n-dimensional indices based on the 1-dimensional index.
    // This is kind of expensive, so we try to amortize it.

    var n = start;

    var remaining = [];
    for (var i = 0; i < this.shape.length; i++)
      ARRAY_PUSH(remaining, 1);
    for (var i = this.shape.length - 2; i >= 0; i--) {
      remaining[i] = remaining[i + 1] * this.shape[i];
    }

    for (var i = 0; i < this.shape.length; i++) {
      var m = TO_INT32(n / remaining[i]);
      this.indices[i] = m;
      n -= m * remaining[i];
    }
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
  }
});

function _ParallelShapeShape(op) {
  var shape = UnsafeGetReservedSlot(op, JS_SHAPE_OP_DIM_SLOT);
  return [shape, null];
}

function _ParallelMapToState(op) {
  var prevOp = UnsafeGetReservedSlot(op, JS_MAPTO_OP_PREVOP_SLOT);

  this.op = op;
  this.index = 0;
  this.prevState = _CreateState(prevOp);
  this.func = UnsafeGetReservedSlot(op, JS_MAPTO_OP_FUNC_SLOT);
}

MakeConstructible(_ParallelMapToState, {
  toString: function() {
    return "_ParallelMapToState(" + [this.index, this.shape] + ")";
  },

  init: function(start, end, extra) {
    this.prevState.init(start, end, extra);
  },

  next: function() {
    var v = this.prevState.next(); // FIXME
    return this.func(v);
  },
});

function _ParallelMapToShape(op) {
  var prevOp = UnsafeGetReservedSlot(op, JS_MAPTO_OP_PREVOP_SLOT);
  return _ComputeShape(prevOp);
}


function _ParallelFilterState(op) {
}

MakeConstructible(_ParallelFilterState, {
  toString: function() {
    return "_ParallelFilterState(" + [this.index, this.shape] + ")";
  },

  init: function(start, end, [array, indices]) {
    this.array = array;
    this.indices = indices;
  },

  next: function(i) {
    return this.array[this.indices[i]];
  },
});

function _ParallelFilterShape(op) {
  var prevOp = UnsafeGetReservedSlot(op, JS_FILTER_OP_PREVOP_SLOT);
  var array = callFunction(ParallelCollect, prevOp);

  var func = UnsafeGetReservedSlot(op, JS_FILTER_OP_FUNC_SLOT);
  var indices = [];
  for (var i = 0; i < array.length; i++) {
    var b = !!func(array[i]);
    if (b) {
      ARRAY_PUSH(indices, i);
    }
  }
  return [[indices.length], [array, indices]];
}

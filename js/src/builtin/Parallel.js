#include "ParallelConstants.h"

///////////////////////////////////////////////////////////////////////////
// PUBLIC INTERFACE

// Common prototype for all pipeline operations.
var PipelineOpProto = {
  map: ParallelMap,
  mapTo: ParallelMapTo,
  filter: ParallelFilter,

  reduce: ParallelReduce,
  collect: ParallelCollect,
};

// array.parallel(): returns a pipeline that yields the elements of an
// array.
//
// FIXME - include an optional depth argument here?
function TypedArrayParallel() {
  var range = ParallelRange(0, this.length);
  return callFunction(ParallelMapTo, range, null, i => this[i]);
}

// parallel.detached(): returns a pipeline with no input. When this
// pipeline is collected or reduced, a starting array must be
// provided, in which case it acts equivalently to `array.parallel()`
function ParallelDetached() {
  var T = GetTypedObjectModule();

  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_CTOR_SLOT, _ParallelDetachedState);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHAPE_FUNC_SLOT, _ParallelDetachedShape);
  return obj;
}

// parallel.range(min, max): returns a 1-dimensional pipeline with
// shape [max-min], yielding integers from min (inclusive) to max
// (exclusive).
function ParallelRange(min, max) {
  var T = GetTypedObjectModule();

  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_CTOR_SLOT, _ParallelRangeState);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHAPE_FUNC_SLOT, _ParallelRangeShape);
  UnsafeSetReservedSlot(obj, JS_RANGE_OP_MIN_SLOT, TO_INT32(min));
  UnsafeSetReservedSlot(obj, JS_RANGE_OP_MAX_SLOT, TO_INT32(max));
  return obj;
}

// parallel.shape(shape): returns a N-dimensional pipeline with
// the given shape, where each element is a vector with the coordinates.
// The N-dimensional version of range.
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
  UnsafeSetReservedSlot(obj, JS_SHAPE_OP_DIM_SLOT, dims);
  return obj;
}

function ParallelMap(func) {
}

// pipeline.mapTo(grainType, func) -- returns a new pipeline with same shape
// as pipeline, but where each element is transformed by `func`
function ParallelMapTo(grainType, func) {
  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_CTOR_SLOT, _ParallelMapToState);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHAPE_FUNC_SLOT, _ParallelMapToShape);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_FUNC_SLOT, func);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_GRAINTYPE_SLOT, grainType);
  return obj;
}

// pipeline.filter(func) -- returns a new 1-D pipeline whose shape
// will be [n], where n is the number of elements returning true when
// `func(e)` is applied.
function ParallelFilter(func) {
  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_STATE_CTOR_SLOT, _ParallelFilterState);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHAPE_FUNC_SLOT, _ParallelFilterShape);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_FUNC_SLOT, func);
  return obj;
}

// pipeline.reduce(func) -- reduce the elements of the pipeline into
// a single result.
function ParallelReduce(func) {
  if (!IsPipelineObject(this))
    ThrowError();

  // This could be better optimized in some cases. In particular, if
  // the pipeline includes a filter, we will currently wind up
  // computing the intermediate values into an array, where I'd prefer
  // not to.

  var [shape, extra] = _ComputeShape(this);
  var numElements = _ComputeNumElements(shape);

  if (numElements == 0)
    ThrowError();

  // Divide into N pieces.
  var N = 4;

  // Handle the initial proc.
  var globalAccum;
  for (var proc = 0; proc < N; proc++) {
    var state = _CreateState(this);
    var [start, end] = _ComputeProcRange(proc, numElements, N);

    global.print("proc", proc, start, end);
    if (start != end) {
      state.init(start, end, extra);
      var localAccum = state.next(start);
      for (var i = start + 1; i < end; i++)
        localAccum = func(localAccum, state.next(i));

      if (proc == 0)
        globalAccum = localAccum;
      else
        globalAccum = func(globalAccum, localAccum);
    }
  }

  return globalAccum;
}

// pipeline.collect([input]) -- creates a typed object array and
// returns it.  The (optional) argument will be supplied to the
// pipeline, which may do something with it. Generally it's used to
// supply the initial array input if an "input-independent" pipeline
// is created.
function ParallelCollect(input) {
  if (!IsPipelineObject(this))
    ThrowError();

  var [shape, grainType, extra] = _ComputeShape(this, input);
  var numElements = _ComputeNumElements(shape);

  var grainTypeIsComplex = !TypeDescrIsSimpleType(grainType);

  // Allocate a flat array to begin with
  var FlatArrayType = grainType.array(numElements); // FIXME
  var result = new FlatArrayType();

  // Divide into N pieces.
  var N = 4;
  for (var proc = 0; proc < N; proc++) {
    var state = _CreateState(this);
    var [start, end] = _ComputeProcRange(proc, numElements, N);

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

///////////////////////////////////////////////////////////////////////////
// THE PROTOCOL
//
// For each operation, there are two associated functions, stored in
// hidden slots. You invoke these operations through helpers,
// described here, which extract the functions from the appropriate
// slots and call them.
//
// The first thing you are expected to do is call
// `_ComputeShape(op)`. This will return a pair `[shape, grainType, extra]`.
// `shape` is an array indicating the number of dimensions and size of
// each dimension. For example, a 2D image might return [768, 1024],
// indicating that the outermost dimension has size 768 and the
// innermost 1024.
//
// The next step is to invoke `_CreateState(op)`. This can be called
// any number of times. Each call returns a fresh state object.  The
// state objects are used to create the actual values. The idea is
// that independent state objects can be used from multiple threads in
// parallel.
//
// Before use, each state object must be initialized by calling
//
//     state.init(start, end, extra)
//
// Here, `start` and `end` are a range of indices that you plan to
// iterate over. These are derived from the `shape` -- basically the
// shape is flattened into a 1D array of equivalent size, and `start`
// and `end` are a subsection of that range. The value `extra` is
// whatever data was returned by `computeShape()`.
//
// Finally, you can invoke `state.compute(i)` for each value `i` from
// `start` to `end`. Each call returns the value for location `i`.

function _ComputeShape(op, input) {
  assert(IsPipelineObject(op), "_ComputeShape invoked with non-pipeline obj");
  var shape = UnsafeGetReservedSlot(op, JS_PIPELINE_OP_SHAPE_FUNC_SLOT);
  return shape(op, input);
}

function _CreateState(op, extra) {
  assert(IsPipelineObject(op), "_CreateState invoked with non-pipeline obj");
  var State = UnsafeGetReservedSlot(op, JS_PIPELINE_OP_STATE_CTOR_SLOT);
  return new State(op, extra);
}

///////////////////////////////////////////////////////////////////////////
// DETACHED IMPLEMENTATION

function _ParallelDetachedShape(op, input) {
  if (typeof input === "undefined")
    ThrowError(); // input required

  // For now, just handle JS arrays.
  var T = GetTypedObjectModule();
  return [[input.length], T.Any, input];
}

function _ParallelDetachedState(op) {
}

MakeConstructible(_ParallelDetachedState, {
  toString: function() {
    return "_ParallelDetachedState()";
  },

  init: function(start, end, input) {
    this.input = input;
  },

  next: function(i) {
    return this.input[i];
  },
});

///////////////////////////////////////////////////////////////////////////
// RANGE IMPLEMENTATION

function _ParallelRangeShape(op, input) {
  if (typeof input !== "undefined")
    ThrowError(); // no input expected

  var T = GetTypedObjectModule();
  var min = UnsafeGetReservedSlot(op, JS_RANGE_OP_MIN_SLOT);
  var max = UnsafeGetReservedSlot(op, JS_RANGE_OP_MAX_SLOT);
  return [[max - min], T.Any, null];
}

function _ParallelRangeState(op) {
  this.op = op;
  this.min = UnsafeGetReservedSlot(op, JS_RANGE_OP_MIN_SLOT);
}

MakeConstructible(_ParallelRangeState, {
  toString: function() {
    return "_ParallelRangeState(" + [this.index, this.min, this.max] + ")";
  },

  init: function(start, end) {
  },

  next: function(i) {
    return i + this.min;
  },
});

///////////////////////////////////////////////////////////////////////////
// SHAPE IMPLEMENTATION

function _ParallelShapeShape(op, input) {
  if (typeof input !== "undefined")
    ThrowError(); // no input expected

  var T = GetTypedObjectModule();
  var shape = UnsafeGetReservedSlot(op, JS_SHAPE_OP_DIM_SLOT);
  return [shape, T.Any, null];
}

function _ParallelShapeState(op) {
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

///////////////////////////////////////////////////////////////////////////
// MAP TO IMPLEMENTATION

function _ParallelMapToShape(op, input) {
  var prevOp = UnsafeGetReservedSlot(op, JS_MAPTO_OP_PREVOP_SLOT);
  var [shape, prevGrainType, extra] = _ComputeShape(prevOp, input);
  var grainType = UnsafeGetReservedSlot(op, JS_MAPTO_OP_GRAINTYPE_SLOT);
  return [shape, grainType, extra];
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

  next: function(i) {
    var v = this.prevState.next(i); // FIXME
    return this.func(v);
  },
});

///////////////////////////////////////////////////////////////////////////
// FILTER

function _ParallelFilterShape(op, input) {
  var T = GetTypedObjectModule();
  var prevOp = UnsafeGetReservedSlot(op, JS_FILTER_OP_PREVOP_SLOT);
  var array = callFunction(ParallelCollect, prevOp, input);

  // FIXME parallelize this
  var func = UnsafeGetReservedSlot(op, JS_FILTER_OP_FUNC_SLOT);
  var indices = [];
  for (var i = 0; i < array.length; i++) {
    var b = !!func(array[i]);
    if (b) {
      ARRAY_PUSH(indices, i);
    }
  }

  // Hmm, we can do better with this grain type, right? What should it
  // be?  If should be Object if n-dim, else the array element type,
  // basically.

  return [[indices.length], T.Any, [array, indices]];
}

function _ParallelFilterState(op) {
  // Not much to do here. All the work is basically done by
  // _ParallelFilterShape.
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

///////////////////////////////////////////////////////////////////////////
// HELPERS

function _ComputeNumElements(shape) {
  var numElements = 1;
  for (var s of shape) {
    // FIXME watch out for overflow
    numElements = std_Math_imul(numElements, s);
  }
  return numElements;
}

function _ComputeProcRange(proc, numElements, N) {
  var perProc = TO_INT32(numElements / N);
  var remainder = TO_INT32(numElements % N);

  var start = 0;
  for (var i = 0; i < proc; i++) {
    start += perProc;
    if (i < remainder)
      start += 1;
  }

  var end = start + perProc;
  if (proc < remainder)
    end += 1;

  return [start, end];
}

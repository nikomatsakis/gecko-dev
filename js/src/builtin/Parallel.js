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
  var T = GetTypedObjectModule();
  return callFunction(ParallelMapTo, range, T.Any, i => this[i]); // FIXME Any
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

  var [shape, grainType, extra] = _ComputeShape(this);
  let numElements = _ComputeNumElements(shape);

  if (numElements == 0)
    ThrowError();

  // Divide into N pieces.
  const N = 4;

  let grainTypeArray = grainType.array(N*2); // FIXME -- use new API

  // Handle the initial proc.
  var nonempty = [];
  var accums = new grainTypeArray();
  for (var proc = 0; proc < N; proc++) {
    const state = _CreateState(this);
    const [start, end] = _ComputeProcRange(proc, numElements, N);

    const buffer = new grainTypeArray();

    if (start != end) {
      state.init(start, end, extra);

      const A = proc * 2;
      const B = A + 1;

      // Find the first successful entry
      var i = start;
      for (; i < end; i++) {
        if (state.next(i, accums, A)) {
          ARRAY_PUSH(nonempty, true);
          break;
        }
      }

      if (i == end) {
        ARRAY_PUSH(nonempty, false);
      } else {
        for (; i < end; i++) {
          if (state.next(i, accums, B))
            accums[A] = func(accums[A], accums[B]); // FIXME -- exposes impl ptr
        }
      }
    }
  }

  // find first non-empty proc (if any)
  var proc0;
  nonempty: {
    for (proc0 = 0; proc0 < N; proc0++) {
      if (nonempty[proc0])
        break nonempty;
    }

    // if we reach here, all data was filtered out
    ThrowError();
  }

  // reduce remaining procs (if any)
  for (var proc1 = proc0 + 1; proc1 < N; proc1++) {
    if (nonempty[proc1]) {
      accums[proc0 * 2] = func(accums[proc0 * 2],
                               accums[proc1 * 2]);
    }
  }

  return accums[proc0 * 2]; // FIXME leaks remainder of array
}

// pipeline.collect([input]) -- creates a typed object array and
// returns it.  The (optional) argument will be supplied to the
// pipeline, which may do something with it. Generally it's used to
// supply the initial array input if an "input-independent" pipeline
// is created.
function ParallelCollect(input) {
  if (!IsPipelineObject(this))
    ThrowError();

  var [shape, grainType] = _ComputeShape(this, input);
  var numElements = _ComputeNumElements(shape);

  // Allocate a flat array to begin with
  var FlatArrayType = grainType.array(numElements); // FIXME -- use new API
  var result = new FlatArrayType();

  // Divide into N pieces.
  var N = 4;
  var counts = [];
  for (var proc = 0; proc < N; proc++) {
    var state = _CreateState(this);
    var [start, end] = _ComputeProcRange(proc, numElements, N);

    state.init(start, end, input);

    let flags = [];
    for (var i = start; i < end; i++) {
      flags[i - start] = state.next(result, i);
    }

    // compress required values into the beginning
    let live = 0;
    for (var i = start; i < end; i++) {
      if (flags[i - start]) {
        result[start + live] = result[i];
        live++;
      }
    }

    counts[proc] = live;
  }

  // Compute total that survived
  var totalLive = counts.reduce((a, b) => a+b);

  // If not everybody survived, must compact.
  if (numElements != totalLive) {
    assert(shape.length == 1, "filter at depth > 1"); // can only filter at outermost level
    var compacted = new (grainType.array(totalLive))(); //FIXME use new API
    var c = 0;
    for (var proc = 0; proc < N; proc++) {
      var [start, end] = _ComputeProcRange(proc, numElements, N);
      for (var i = 0; i < counts[proc]; i++) {
        compacted[c++] = result[i + start];
      }
    }
    return compacted;
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
    this.index = start;
    this.input = input;

    var T = GetTypedObjectModule();
    this.grainType = T.Any; // FIXME ought to be derived from input
  },

  next: function(out, o) {
    out[o] = this.input[this.index++];
    return true;
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

  init: function(start, end, _input) {
    this.index = start;

    var T = GetTypedObjectModule();
    this.grainType = T.Any;
  },

  next: function(out, o) {
    out[o] = this.index++;
    return true;
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

  init: function(start, end, _input) {
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

    var T = GetTypedObjectModule();
    this.grainType = T.Any;
  },

  next: function(out, o) {
    var output = [];
    for (var i = 0; i < this.shape.length; i++) {
      ARRAY_PUSH(output, this.indices[i]);
    }

    out[o] = output;

    for (var i = this.shape.length - 1; i >= 0; i--) {
      if (++this.indices[i] < this.shape[i])
        break;
      this.indices[i] = 0;
    }

    return true;
  }
});

///////////////////////////////////////////////////////////////////////////
// MAP TO IMPLEMENTATION

function _ParallelMapToShape(op, input) {
  var prevOp = UnsafeGetReservedSlot(op, JS_MAPTO_OP_PREVOP_SLOT);
  var [shape, prevGrainType] = _ComputeShape(prevOp, input);
  var grainType = UnsafeGetReservedSlot(op, JS_MAPTO_OP_GRAINTYPE_SLOT);
  return [shape, grainType];
}

function _ParallelMapToState(op) {
  var prevOp = UnsafeGetReservedSlot(op, JS_MAPTO_OP_PREVOP_SLOT);

  this.grainType = UnsafeGetReservedSlot(op, JS_MAPTO_OP_GRAINTYPE_SLOT);
  this.op = op;
  this.index = 0;
  this.prevState = _CreateState(prevOp);
  this.func = UnsafeGetReservedSlot(op, JS_MAPTO_OP_FUNC_SLOT);
}

MakeConstructible(_ParallelMapToState, {
  toString: function() {
    return "_ParallelMapToState(" + [this.index, this.shape] + ")";
  },

  init: function(start, end, input) {
    this.prevState.init(start, end, input);

    var prevGrainType = this.prevState.grainType;
    var prevGrainTypeArray = prevGrainType.array(1); // FIXME -- use new API
    this.buffer = new prevGrainTypeArray();
  },

  next: function(out, o) {
    if (!this.prevState.next(this.buffer, 0))
      return false; // element i was filtered out

    // FIXME -- exposes impl detail, shouldn't do it like this
    out[o] = this.func(this.buffer[0]);
    return true;
  },
});

///////////////////////////////////////////////////////////////////////////
// FILTER

function _ParallelFilterShape(op, input) {
  var prevOp = UnsafeGetReservedSlot(op, JS_FILTER_OP_PREVOP_SLOT);
  return _ComputeShape(prevOp, input);
}

function _ParallelFilterState(op) {
  var prevOp = UnsafeGetReservedSlot(op, JS_FILTER_OP_PREVOP_SLOT);
  this.prevState = _CreateState(prevOp);
  this.func = UnsafeGetReservedSlot(op, JS_FILTER_OP_FUNC_SLOT);
}

MakeConstructible(_ParallelFilterState, {
  toString: function() {
    return "_ParallelFilterState(" + [this.index, this.shape] + ")";
  },

  init: function(start, end, input) {
    this.prevState.init(start, end, input);
    this.grainType = this.prevState.grainType;
  },

  next: function(out, o) {
    if (!this.prevState.next(out, o))
      return false; // element i was already filtered out

    // FIXME -- exposes impl detail, shouldn't do it like this
    return !!this.func(out[o]); // take a crack at filtering it out
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

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

// parallel.range(min, max): returns a 1-dimensional pipeline with
// shape [max-min], yielding integers from min (inclusive) to max
// (exclusive).
function ParallelRange(min, max) {
  var T = GetTypedObjectModule();

  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHARED_CTOR_SLOT, _ParallelRangeShared);
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
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHARED_CTOR_SLOT, _ParallelShapeShared);
  UnsafeSetReservedSlot(obj, JS_SHAPE_OP_DIM_SLOT, dims);
  return obj;
}

function ParallelMap(func) {
}

// pipeline.mapTo(grainType, func) -- returns a new pipeline with same shape
// as pipeline, but where each element is transformed by `func`
function ParallelMapTo(grainType, func) {
  var obj = NewPipelineObject(PipelineOpProto);
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHARED_CTOR_SLOT, _ParallelMapToShared);
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
  UnsafeSetReservedSlot(obj, JS_PIPELINE_OP_SHARED_CTOR_SLOT, _ParallelFilterShared);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_FUNC_SLOT, func);
  return obj;
}

// pipeline.reduce(func) -- reduce the elements of the pipeline into
// a single result.
function ParallelReduce(func) {
  if (!IsPipelineObject(this))
    ThrowError();

  var shared = _CreateShared(this);
  let numElements = _ComputeNumElements(shared.shape);
  if (numElements == 0)
    ThrowError();

  // Divide into N pieces.
  const N = 7;

  // Handle the initial proc.
  var empty = [];
  var accums = [];
  for (var proc = 0; proc < N; proc++) {
    var [start, end] = _ComputeProcRange(proc, numElements, N);
    if (start == end) {
      empty[proc] = true;
      continue;
    }

    var state = shared.state(start, end);
    var A = proc * 2;
    var B = A + 1;

    // Find the first successful entry
    var i = start;
    foundFirst: {
      for (; i < end; i++) {
        if (state.next(accums, A))
          break foundFirst;
      }

      empty[proc] = true;
      continue;
    }

    empty[proc] = false;
    for (var j = i + 1; j < end; j++) {
      if (state.next(accums, B)) {
        var x = func(accums[A], accums[B]);
        accums[A] = x;
      }
    }
  }

  // find first non-empty proc (if any)
  var proc0 = 0;
  foundFirstProc: {
    for (; proc0 < N; proc0++) {
      if (!empty[proc0])
        break foundFirstProc;
    }

    // if we reach here, all data was filtered out
    ThrowError();
  }

  // reduce remaining procs (if any)
  for (var proc1 = proc0 + 1; proc1 < N; proc1++) {
    if (!empty[proc1]) {
      var x = func(accums[proc0 * 2], accums[proc1 * 2]);
      accums[proc0 * 2] = x;
    }
  }

  return accums[proc0 * 2];
}

// pipeline.collect() -- creates a typed object array and
// returns it.
function ParallelCollect() {
  if (!IsPipelineObject(this))
    ThrowError();

  var shared = _CreateShared(this);
  var numElements = _ComputeNumElements(shared.shape);

  // Allocate a flat array to begin with
  var FlatArrayType = shared.grainType.array(numElements); // FIXME -- use new API
  var result = new FlatArrayType();

  // Divide into N pieces.
  var N = 7;
  var counts = [];
  for (var proc = 0; proc < N; proc++) {
    var [start, end] = _ComputeProcRange(proc, numElements, N);
    var state = shared.state(start, end);

    // compute live results and store them into the result array; note
    // that some elements may be filtered out
    var live = 0;
    for (var i = start; i < end; i++) {
      if (state.next(result, start + live))
        live++;
    }

    counts[proc] = live;
  }

  // Compute total that survived
  var totalLive = counts.reduce((a, b) => a+b);

  // If not everybody survived, must compact.
  if (numElements != totalLive) {
    assert(shared.shape.length == 1, "filter at depth > 1"); // can only filter at outermost level
    var compacted = new (shared.grainType.array(totalLive))(); //FIXME use new API
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
  if (shared.shape.length == 1)
    return result;

  // FIXME
  var DeepArrayType = grainType.array.apply(grainType, shared.shape);
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
// `_CreateShared(op)`. This will return an object that represents the
// *shared state* for this pipeline (actually a chain of objects, one
// per op).
//
// All shared state objects offer the following properties:
// - `shape` -- an array with the maximal number of items that can be
//   produced
// - `grainType` -- the grainType that will be produced
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

function _CreateShared(op) {
  assert(IsPipelineObject(op), "_ComputeShape invoked with non-pipeline obj");
  var shape = UnsafeGetReservedSlot(op, JS_PIPELINE_OP_SHARED_CTOR_SLOT);
  return new shape(op);
}

///////////////////////////////////////////////////////////////////////////
// RANGE IMPLEMENTATION

function _ParallelRangeShared(op) {
  var T = GetTypedObjectModule();
  this.min = UnsafeGetReservedSlot(op, JS_RANGE_OP_MIN_SLOT);
  var max = UnsafeGetReservedSlot(op, JS_RANGE_OP_MAX_SLOT);
  this.shape = [max - this.min];
  this.grainType = T.Any;
}

MakeConstructible(_ParallelRangeShared, {
  state: function(start, end) {
    return new _ParallelRangeState(this, start, end);
  }
});

function _ParallelRangeState(shared, start, end) {
  this.index = start + shared.min;
}

MakeConstructible(_ParallelRangeState, {
  toString: function() {
    return "_ParallelRangeState(" + [this.index] + ")";
  },

  next: function(out, o) {
    out[o] = this.index++;
    return true;
  },
});

///////////////////////////////////////////////////////////////////////////
// SHAPE IMPLEMENTATION

function _ParallelShapeShared(op) {
  var T = GetTypedObjectModule();
  this.shape = UnsafeGetReservedSlot(op, JS_SHAPE_OP_DIM_SLOT);
  this.grainType = T.Any;
}

MakeConstructible(_ParallelShapeShared, {
  toString: function() {
    return "_ParallelShapeShared(" + [this.shape] + ")";
  },

  state: function(start, end) {
    return new _ParallelShapeState(this, start, end);
  }
});

function _ParallelShapeState(shared, start, end) {
  this.shape = shared.shape;

  // initialized multi-dimensional indices based on (flat) start
  // index.

  var remaining = [];
  for (var i = 0; i < this.shape.length; i++)
    ARRAY_PUSH(remaining, 1);
  for (var i = this.shape.length - 2; i >= 0; i--)
    remaining[i] = remaining[i + 1] * this.shape[i];

  this.indices = [];
  var n = start;
  for (var i = 0; i < this.shape.length; i++) {
    var m = TO_INT32(n / remaining[i]);
    ARRAY_PUSH(this.indices, m);
    n -= m * remaining[i];
  }
}

MakeConstructible(_ParallelShapeState, {
  toString: function() {
    return "_ParallelShapeState(" + [this.indices] + ")";
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

function _ParallelMapToShared(op) {
  var prevOp = UnsafeGetReservedSlot(op, JS_MAPTO_OP_PREVOP_SLOT);
  this.prevShared = _CreateShared(prevOp);
  this.shape = this.prevShared.shape;
  this.grainType = UnsafeGetReservedSlot(op, JS_MAPTO_OP_GRAINTYPE_SLOT);
  this.func = UnsafeGetReservedSlot(op, JS_MAPTO_OP_FUNC_SLOT);
}

MakeConstructible(_ParallelMapToShared, {
  state: function(start, end) {
    return new _ParallelMapToState(this, start, end);
  }
});

function _ParallelMapToState(shared, start, end) {
  this.prevState = shared.prevShared.state(start, end);
  var prevGrainType = shared.prevShared.grainType;
  var prevGrainTypeArray = prevGrainType.array(1); // FIXME -- use new API
  this.buffer = new prevGrainTypeArray();
  this.func = shared.func;
}

MakeConstructible(_ParallelMapToState, {
  toString: function() {
    return "_ParallelMapToState(" + this.prevState + ")";
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

function _ParallelFilterShared(op) {
  var prevOp = UnsafeGetReservedSlot(op, JS_FILTER_OP_PREVOP_SLOT);
  this.prevShared = _CreateShared(prevOp);
  this.shape = this.prevShared.shape;
  this.grainType = this.prevShared.grainType;
  this.func = UnsafeGetReservedSlot(op, JS_FILTER_OP_FUNC_SLOT);
}

MakeConstructible(_ParallelFilterShared, {
  state: function(start, end) {
    return new _ParallelFilterState(this, start, end);
  }
});

function _ParallelFilterState(shared, start, end) {
  this.prevState = shared.prevShared.state(start, end);
  this.func = shared.func;
}

MakeConstructible(_ParallelFilterState, {
  toString: function() {
    return "_ParallelFilterState(" + this.prevState + ")";
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

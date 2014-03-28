///////////////////////////////////////////////////////////////////////////
// The basic prototype shared by all ops.
//
// For now, use the SUPER SEKRET convention that `_foo` is an internal
// name.

var _PipelineOp = {
  // Construct new pipelines:
  map: function(func) {
    return this.mapTo(this.grainType, func);
  },

  mapTo: function(grainType, func) {
    return new _PipelineMapToOp(this, grainType, func);
  },

  filter: function(func) {
    if (this.depth() !== 1)
      throw new TypeError("Cannot filter a pipeline unless depth is 1");
    return new _PipelineFilterOp(this, func);
  },

  // Execute pipelines:
  toArray: PipelineToArray,
  reduce: PipelineReduce,
};

function ArrayPipeline() {
  // Usage: jsArray.parallel()
  var T = GetTypedObjectModule();
  return new _PipelineComprehensionOp(i => this[i], T.Any, [this.length]);
}

///////////////////////////////////////////////////////////////////////////
// Comprehension

function _PipelineComprehensionOp(func, grainType, shape) {
  this.func = func;
  this.grainType = grainType;
  this.shape = shape;
}

MakeConstructible(_PipelineComprehensionOp, std_Object_create({
  depth: function() {
    return this.shape.length;
  },

  prepare_: function() {
    return new ComprehensionState(this);
  },
}));

function ComprehensionState(op) {
  this.op = op;
  this.shape = this.op.shape;
  this.positions = [];
  for (var i = 0; i < this.shape.length; i++)
    this.positions.push(0);
  this.grainType = this.op.grainType;
}

MakeConstructible(ComprehensionState, {
  next: function() {
    var v = this.op.func.apply(null, this.positions);
    increment(this.positions, this.shape);
    return v;
  },
});

///////////////////////////////////////////////////////////////////////////
// _PipelineMapToOp

function _PipelineMapToOp(prevOp, grainType, func) {
  assertEq(prevOp instanceof BaseOp, true);
  this.prevOp = prevOp;
  this.grainType = grainType;
  this.func = func;
}

MakeConstructible(_PipelineMapToOp, std_Object_create(_PipelineOp, {
  depth: function() {
    return this.prevOp.depth();
  },

  prepare_: function() {
    return new MapState(this, this.prevOp.prepare_());
  },
}));

function MapState(op, prevState) {
  this.op = op;
  this.prevState = prevState;
  this.shape = prevState.shape;
  this.grainType = op.grainType;
}

MapState.prototype = {
  next: function() {
    var v = this.prevState.next();
    return this.op.func(v);
  }
};

///////////////////////////////////////////////////////////////////////////

function _PipelineFilterOp(prevOp, func) {
  this.prevOp = prevOp;
  this.grainType = prevOp.grainType;
  this.func = func;
}

MakeConstructible(_PipelineFilterOp, std_Object_create(_PipelineOp, {
  depth: function() {
    return 1;
  },

  prepare_: function() {
    var prevState = this.prevOp.prepare_();
    var grainType = prevState.grainType;
    var temp = build(prevState);
    var keeps = new Uint8Array(temp.length);
    var count = 0;
    for (var i = 0; i < temp.length; i++)
      if ((keeps[i] = this.func(temp[i])))
        count++;
    return new FilterState(grainType, temp, keeps, count);
  }
}));

function FilterState(grainType, temp, keeps, count) {
  this.temp = temp;
  this.keeps = keeps;
  this.grainType = grainType;
  this.shape = [count];
  this.position = 0;
}

MakeConstructible(FilterState, {
  next: function() {
    while (!this.keeps[this.position])
      this.position++;
    return this.temp[this.position++];
  }
});

///////////////////////////////////////////////////////////////////////////

function lastOp(pipeline) {
  return pipeline.ops[pipeline.ops.length - 1];
}

function build(state) {
  var resultArray = allocArray(state.grainType, state.shape);

  var total = 1;
  var position = [];
  for (var i = 0; i < state.shape.length; i++) {
    total *= state.shape[i];
    position.push(0);
  }

  for (var i = 0; i < total; i++) {
    setIndex(resultArray, position, state.next());
    increment(position, state.shape);
  }

  return resultArray;
}

function index(vec, positions) {
  var v = vec;
  for (var i = 0; i < positions.length; i++)
    v = v[positions[i]];
  return v;
}

function setIndex(vec, positions, value) {
  var v = vec;
  for (var i = 0; i < positions.length - 1; i++)
    v = v[positions[i]];
  v[positions[i]] = value;
}

function increment(positions, shape) {
  for (var i = positions.length - 1; i >= 0; i--) {
    var v = ++positions[i];
    if (v < shape[i])
      return;
    positions[i] = 0;
  }
}

function allocArray(grainType, shape) {
  var arrayType = grainType;
  for (var i = shape.length - 1; i >= 0; i--)
    arrayType = new ArrayType(arrayType).dimension(shape[i]);
  return new arrayType();
}

function subtype(proto, props) {
  var result = Object.create(proto);
  for (var key in props) {
    if (props.hasOwnProperty(key)) {
      result[key] = props[key];
    }
  }
  return result;
}

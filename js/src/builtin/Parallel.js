function ParallelMap(func) {
}

function ParallelMapTo(grainType, func) {
  var obj = NewPipelineOp(ParallelMapToProto);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_GRAINTYPE_SLOT, grainType);
  UnsafeSetReservedSlot(obj, JS_MAPTO_OP_FUNC_SLOT, func);
  return obj;
}

var ParallelMapToProto = {
};

function ParallelFilter(func) {
  var obj = NewPipelineOp(ParallelFilterProto);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_FUNC_SLOT, func);
  return obj;
}

var ParallelFilterProto = {
};

function ParallelCollect() {
  var obj = NewPipelineOp(ParallelCollectProto);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_PREVOP_SLOT, this);
  UnsafeSetReservedSlot(obj, JS_FILTER_OP_FUNC_SLOT, func);
  return obj;
}

var ParallelCollectProto = {
};

function ParallelReduce(func) {
  var obj = NewReduceOp(func);
}

var ParallelReduceProto = {
};


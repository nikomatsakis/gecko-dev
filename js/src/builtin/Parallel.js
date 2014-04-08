function ParallelMap(func) {
}

function ParallelMapTo(grainType, func) {
  var obj = NewMapToOp(grainType, func);
}

function ParallelFilter(func) {
  var obj = NewFilterOp(func);
}

function ParallelCollect() {
  
  var obj = NewCollectOp();
}

function ParallelReduce(func) {
  var obj = NewReduceOp(func);
}

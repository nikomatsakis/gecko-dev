var ArrayType = TypedObject.ArrayType;
var { float32x4 } = SIMD;
var Data = float32x4.array(100);

// instruct ion to kick in fast
setJitCompilerOption("ion.usecount.trigger", 30);

function sum(a) {
  var sum = float32x4.zero();
  for (var i = 0; i < a.length; i++) {
    sum = SIMD.float32x4.add(sum, a[i]);
  }
  return sum;
}

function main() {
  var a = new Data();
  sum(a); //warmup
  sum(a); //for reals
  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();

var ArrayType = TypedObject.ArrayType;
var { float32x4 } = SIMD;
var Data = float32x4.array(10000);

// instruct ion to kick in fast
setJitCompilerOption("ion.usecount.trigger", 10);

function sum(a) {
  var sum = 0;
  for (var i = 0; i < a.length; i++) {
    sum += i;
  }
  var m = SIMD.float32x4(1.0, -2.0, 3.0, -4.0);
  var lo = SIMD.float32x4(0.0, 0.0, 0.0, 0.0);
  var hi = SIMD.float32x4(2.0, 2.0, 2.0, 2.0);
  return SIMD.float32x4.clamp(m, lo, hi);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.x, 1.0);
  assertEq(c.y, 0);
  assertEq(c.z, 2.0);
  assertEq(c.w, 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


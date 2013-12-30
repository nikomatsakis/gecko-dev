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
  var a = SIMD.int32x4.zero();
  var b = SIMD.int32x4.splat(1);
  return SIMD.int32x4.sub(a, b);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.x, -1);
  assertEq(c.y, -1);
  assertEq(c.z, -1);
  assertEq(c.w, -1);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


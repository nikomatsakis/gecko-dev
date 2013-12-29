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
  var a = SIMD.int32x4(11, 22, 33, 44);
  return SIMD.int32x4.toFloat32x4(a);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.x, 11);
  assertEq(c.y, 22);
  assertEq(c.z, 33);
  assertEq(c.w, 44);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


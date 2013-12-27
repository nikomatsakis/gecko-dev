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
  return SIMD.float32x4.splat(2);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.x, 2);
  assertEq(c.y, 2);
  assertEq(c.z, 2);
  assertEq(c.w, 2);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


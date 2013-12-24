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
  var a = float32x4(1, 20, 3, 40);
  var b = float32x4(10, 2, 30, 4);
  return SIMD.float32x4.min(a, b);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 3);
  assertEq(c.w, 4);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


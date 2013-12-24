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
  var a = float32x4(1, 20, 30, 4);
  var b = float32x4(10, 2, 3, 40);
  return SIMD.float32x4.max(a, b);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.x, 10);
  assertEq(c.y, 20);
  assertEq(c.z, 30);
  assertEq(c.w, 40);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


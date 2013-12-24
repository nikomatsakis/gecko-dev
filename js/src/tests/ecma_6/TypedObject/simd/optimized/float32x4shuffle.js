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
  var a = float32x4(1, 2, 3, 4);
  return SIMD.float32x4.shuffle(a, 0x1B);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.x, 4);
  assertEq(c.y, 3);
  assertEq(c.z, 2);
  assertEq(c.w, 1);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


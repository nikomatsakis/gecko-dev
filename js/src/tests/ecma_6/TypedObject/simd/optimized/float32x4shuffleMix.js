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
  var b = float32x4(5, 6, 7, 8);
  return SIMD.float32x4.shuffle(a, b, 0x00);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.x, 1);
  assertEq(c.y, 1);
  assertEq(c.z, 5);
  assertEq(c.w, 5);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


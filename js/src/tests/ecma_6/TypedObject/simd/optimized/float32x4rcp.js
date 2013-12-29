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
  var a = SIMD.float32x4(-2,-0.5,2,0.5);
  return SIMD.float32x4.reciprocal(a);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertTrue(Math.abs(-0.5 - c.x) <= 0.01);
  assertTrue(Math.abs(-2 - c.y) <= 0.01);
  assertTrue(Math.abs(0.5 - c.z) <= 0.01);
  assertTrue(Math.abs(2 - c.w) <= 0.01);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


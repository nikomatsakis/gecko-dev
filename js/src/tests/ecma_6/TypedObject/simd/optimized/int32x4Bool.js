var ArrayType = TypedObject.ArrayType;
var { int32x4 } = SIMD;
var Data = int32x4.array(10000);

// instruct ion to kick in fast
setJitCompilerOption("ion.usecount.trigger", 10);

function sum(a) {
  var sum = 0;
  for (var i = 0; i < a.length; i++) {
    sum += i;
  }
  var a = int32x4.bool(true, false, false, true);
  return a;
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.x, -1);
  assertEq(c.y, 0);
  assertEq(c.z, 0);
  assertEq(c.w, -1);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


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
  var a = int32x4(1, 2, 3, 4);
  return int32x4.withFlagZ(a, true);
}

function main() {
  var a = new Data();
  sum(a); //warmup
  var c = sum(a); //for reals
  assertEq(c.z, -1);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

main();


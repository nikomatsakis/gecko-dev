// Test the case where we neuter an instance of a variable-length array.
// Here we can fold the neuter check into the bounds check.

if (!this.hasOwnProperty("TypedObject"))
  quit();

// FIXME adapt test at end of patch series
quit();

var {StructType, uint32, storage} = TypedObject;
var S = new StructType({f: uint32, g: uint32});
var A = S.array();

function readFrom(a) {
  return a[2].f + a[2].g;
}

function main() {
  var a = new A(10);
  a[2].f = 22;
  a[2].g = 44;

  for (var i = 0; i < 10; i++)
    assertEq(readFrom(a), 66);

  neuter(storage(a).buffer);

  for (var i = 0; i < 10; i++) {
    var ok = false;

    try {
      readFrom(a);
    } catch (e) {
      ok = e instanceof TypeError;
    }

    assertEq(ok, true);
  }
}

// FIXME adapt test at end of patch series
// main();

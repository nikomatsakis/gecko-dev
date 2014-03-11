// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 973238;

var {StructType, uint32, Object, Any, storage, objectType} = TypedObject;

function main() { // once a C programmer, always a C programmer.
  print(BUGNUMBER + ": " + summary);

  var Uints = new StructType({f: uint32, g: uint32});
  var p = Uints.prototype;
  Uints.prototype = {}; // no effect
  assertEq(p, Uints.prototype);

  var uints = new Uints();
  assertEq(uints.__proto__, p);
  assertThrowsInstanceOf(function() uints.__proto__ = {},
                         TypeError);
  assertEq(uints.__proto__, p);

  reportCompare(true, true);
  print("Tests complete");
}

main();

// Check for mapPar() applied to an empty array.
// Public domain.

if (!this.hasOwnProperty("TypedObject"))
  quit();

// FIXME update this patch at the end of the patch series
quit();

var { ArrayType, StructType, uint32 } = TypedObject;
var Point = new StructType({x: uint32, y: uint32});
var Points = Point.array();
var points = new Points();
points.mapPar(function() {});


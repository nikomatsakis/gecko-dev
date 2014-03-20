/* Testing TypeDescrIsArrayType() is somewhat straightforward: it's
 * used early in the map() method on TypedObject arrays.
 *
 * Run this with IONFLAGS=logs, generate pdfs with iongraph, and then
 * try running "pdfgrep TypeDescrIsArrayType func*pass00*.pdf", this
 * might net a function that is a likely candidate for manual inspection.
 *
 * (It is sometimes useful to neuter the assert() macro in the
 * self-hosted code.)
 */

if (!this.TypedObject)
    quit();

var T = TypedObject;
var AT = new T.ArrayType(T.int32);

function check(v) {
    return v.map(x => x+1);
}

function test() {
    var w = new AT(100);
    for ( var i=0 ; i < 1000 ; i++ )
	w = check(w);
    return w;
}

var w = test();
print(w.length);

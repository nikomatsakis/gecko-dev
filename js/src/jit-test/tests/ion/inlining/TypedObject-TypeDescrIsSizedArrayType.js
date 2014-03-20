/* Testing TypeDescrIsSizedArrayType() is somewhat straightforward:
 * it's used early in the sequential scatter() method on TypedObject
 * arrays, and is applied to the first argument (not to "this").
 *
 * Run this with IONFLAGS=logs, generate pdfs with iongraph, and then
 * try running "pdfgrep TypeDescrIsSizedArrayType func*pass00*.pdf", this
 * might net a function that is a likely candidate for manual inspection.
 *
 * (It is sometimes useful to neuter the assert() macro in the
 * self-hosted code.)
 */

if (!this.TypedObject)
    quit();

var T = TypedObject;
var AT = new T.ArrayType(T.int32);
var IT = AT.dimension(100);
var ix = AT.build(100, x => x == 0 ? 99 : x-1);

function check(v) {
    return v.scatter(IT, ix);
}

function test() {
    var w = new AT(100);
    for ( var i=0 ; i < 1000 ; i++ )
	w = check(w);
    return w;
}

var w = test();
print(w.length);

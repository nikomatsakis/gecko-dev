/* Testing TypeDescrIsSimpleType() is tricky because it's not exposed.
 * However, the implementation of <typed-object>.build() must use it.
 *
 * Run this with IONFLAGS=logs, generate pdfs with iongraph, and then
 * try running "pdfgrep TypeDescrIsSimpleType func*pass00*.pdf", this
 * might net a couple of functions that are likely candidates for
 * manual inspection.
 */

if (!this.TypedObject)
    quit();

var T = TypedObject;
var AT = new T.ArrayType(T.uint32);

function check() {
    return AT.build(100, x => x+1);
}

function test() {
    var w;
    for ( var i=0 ; i < 100 ; i++ )
	w = check();
    return w;
}

var w = test();
print(w.length);
